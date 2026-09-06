#!/usr/bin/env python3
"""Real C++ LearningApp -> loopback HTTP -> SQLite, with process restarts.

Python is an I/O relay only: it never creates or edits device event JSON.
Parent operations use HTTP API (browser coverage is a separate gate).
All identities/data are synthetic; credentials and raw protocol are not logged.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import hmac
import json
import os
from pathlib import Path
import queue
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request
from urllib.parse import urlparse
import uuid


def hx(value: bytes) -> str:
    return value.hex() or "-"


def unhex(value: str) -> bytes:
    return b"" if value == "-" else bytes.fromhex(value)


def http(method, url, body=b"", authorization="", timeout=5):
    request = urllib.request.Request(url, data=body or None, method=method,
        headers={"Content-Type": "application/json", "Authorization": authorization})
    # Explicitly avoid machine-configured proxies: this harness is loopback only.
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    try:
        with opener.open(request, timeout=timeout) as response:
            return response.status, response.read()
    except urllib.error.HTTPError as error:
        return error.code, error.read()


class Runner:
    def __init__(self, exe, base, registration, disk, epoch):
        self.base, self.registration, self.disk = base, registration, disk
        self.mode = "online"
        self.fail_save = False
        self.batches = []
        self.process = subprocess.Popen([str(exe), base, registration["device_id"],
            "child-1", uuid.uuid4().hex, str(epoch)], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
        self.lines = queue.Queue()
        def read():
            for line in self.process.stdout:
                self.lines.put(line.rstrip("\n"))
            self.lines.put(None)
        self.reader = threading.Thread(target=read, daemon=True)
        self.reader.start()

    def reply(self, value):
        self.process.stdin.write(value + "\n")
        self.process.stdin.flush()

    def command(self, command=None):
        if command is not None:
            self.reply(command)
        while True:
            line = self.lines.get(timeout=15)
            if line is None:
                raise RuntimeError("C++ runner exited: " + self.process.stderr.read())
            if line.startswith("RESULT "):
                return json.loads(line[7:])
            parts = line.split(" ")
            if parts[0] == "LOAD":
                self.reply(hx(self.disk.read_bytes()) if self.disk.exists() else "MISSING")
            elif parts[0] == "SAVE":
                if self.fail_save:
                    self.reply("ERROR")
                    continue
                blob = unhex(parts[1])
                # Commit is complete before C++ can publish its state. This
                # tests process death, NOT OS power-loss/fsync equivalence to NVS.
                temporary = self.disk.with_suffix(".tmp")
                with temporary.open("wb") as output:
                    output.write(blob)
                    output.flush()
                    os.fsync(output.fileno())
                os.replace(temporary, self.disk)
                self.reply("OK")
            elif parts[0] == "SIGN":
                assert parts[1] == self.registration["device_id"]
                message = "|".join(parts[1:]).encode()
                signature = hmac.new(self.registration["device_secret"].encode(),
                                     message, hashlib.sha256).digest()
                self.reply(base64.b64encode(signature).decode())
            elif parts[0] == "HTTP":
                _, method, url, token, body, timeout = parts
                assert url.startswith(self.base + "/api/v1/"), "non-local relay target"
                if self.mode == "offline":
                    self.reply("0 -")
                    continue
                status, response = http(method, url, unhex(body),
                                        unhex(token).decode(), int(timeout) / 1000)
                if url.endswith("/events/batch"):
                    self.batches.append((unhex(body), status, json.loads(response)))
                    if self.mode == "lost":
                        # Backend committed; only the response is dropped.
                        self.reply("0 -")
                        continue
                self.reply(f"{status} {hx(response)}")
            else:
                raise RuntimeError("unknown relay operation")

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
        self.process.wait(timeout=10)
        self.reader.join(timeout=2)
        for pipe in (self.process.stdin, self.process.stdout, self.process.stderr):
            pipe.close()


def compile_runner(args):
    source = args.repo / "firmware"
    implementations = ["learning_domain/reducer.cpp", "sync/outbox_core.cpp",
        "application/coordinator.cpp", "interaction/dispatcher.cpp",
        "mcp/learning_mcp_host.cpp", "ui/presenters.cpp", "sync/backend_client.cpp",
        "sync/wire_codec.cpp"]
    exe = args.out_dir / "connected_learning_app.exe"
    command = [str(args.compiler), "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-I", str(source / "main"), "-I", str(source / "tests"),
        "-I", str(args.repo / "integration"),
        str(source / "tests/host/connected_learning_app.cpp")]
    command += [str(source / "main" / path) for path in implementations]
    command += [str(args.repo / "integration/metalio_claw4/device/core/outbox_codec.cpp"),
                "-o", str(exe)]
    env = dict(os.environ, PATH=str(args.compiler.parent) + os.pathsep + os.environ["PATH"])
    subprocess.run(command, env=env, check=True, timeout=180)
    return exe


def run_scenarios(args, exe, base, directory):
    parent = "Bearer mock-parent-token-1"
    def api(method, path, body=None):
        status, response = http(method, base + "/api/v1" + path,
            json.dumps(body).encode() if body is not None else b"", parent)
        assert 200 <= status < 300, f"API {path}: status={status}"
        return json.loads(response)

    accepted = duplicates = restarts = 0
    for iteration in range(args.rounds):
        registration = api("POST", "/devices/register", {
            "installation_id": "connected-" + uuid.uuid4().hex,
            "model": "host-connected", "fw_version": "app-first"})
        api("POST", "/devices/claim", {"pairing_code": registration["pairing_code"],
                                      "child_id": "child-1"})
        task = {"task_id": args.task_id[iteration]} if args.task_id else api("POST", "/parents/me/children/child-1/tasks", {
            "title": f"Synthetic connected loop {iteration}", "subject": "math",
            "estimated_minutes": 20, "priority": "high"})
        task_id = task["task_id"]
        disk = directory / f"outbox-{iteration}.blob"
        epoch = int(time.time())
        runner = Runner(exe, base, registration, disk, epoch)
        try:
            assert runner.command()["pending"] == 0
            assert runner.command("auth")["ok"]
            assert runner.command("today")["ok"]
            runner.mode = "offline"
            # Persistence failure must neither publish nor change the old blob.
            before = disk.read_bytes()
            runner.fail_save = True
            failed = runner.command("start " + task_id)
            assert not failed["ok"] and failed["pending"] == 0 and not failed["active"]
            assert disk.read_bytes() == before
            runner.fail_save = False
            assert runner.command("start " + task_id)["ok"]
            assert runner.command("tick 60")["ok"]
            paused = runner.command("pause " + task_id)
            assert paused["ok"] and paused["pending"] == 3 and paused["active"]
            failed = runner.command("sync")
            assert not failed["ok"] and failed["pending"] == 3

            # Actual OS processes die; only the codec blob survives each boot.
            for _ in range(args.restarts_per_round):
                runner.close()
                runner = Runner(exe, base, registration, disk, epoch + 120)
                restored = runner.command()
                assert restored["pending"] == 3 and restored["active"]
                assert next(t for t in restored["tasks"]
                            if unhex(t["id_hex"]).decode() == task_id)["status"] == 3
                restarts += 1
            assert runner.command("resume " + task_id)["ok"]
            assert runner.command("tick 60")["ok"]
            completed = runner.command("complete " + task_id)
            assert completed["ok"] and completed["pending"] == 6 and not completed["active"]
            assert runner.command("auth")["ok"]
            runner.mode = "lost"
            lost = runner.command("sync")
            assert not lost["ok"] and lost["pending"] == 6 and lost["ack"] == 0
            first_body, status, first = runner.batches[-1]
            assert status == 200 and first["accepted"] == 6
            accepted += first["accepted"]
            events = json.loads(first_body)["events"]
            assert [event["sequence"] for event in events] == list(range(1, 7))
            assert len({event["event_id"] for event in events}) == 6

            runner.close()
            runner = Runner(exe, base, registration, disk, epoch + 180)
            assert runner.command()["pending"] == 6
            restarts += 1
            assert runner.command("auth")["ok"]
            synced = runner.command("sync")
            assert synced["ok"] and synced["pending"] == 0 and synced["ack"] == 6
            second_body, status, second = runner.batches[-1]
            assert second_body == first_body, "retry must preserve exact C++ request bytes"
            assert status == 200 and second["duplicates"] == 6
            duplicates += second["duplicates"]
            sessions = api("GET", "/parents/me/children/child-1/study-sessions")
            matching = [s for s in sessions if s["task_id"] == task_id]
            assert len(matching) == 1 and matching[0]["status"] == "completed"
            assert matching[0]["actual_seconds"] == 120, matching[0]
            assert matching[0]["pause_count"] == 1, matching[0]
            assert matching[0]["xp"] == 2, matching[0]
            assert runner.command("today")["ok"]
            final = runner.command("state")
            assert next(t for t in final["tasks"]
                        if unhex(t["id_hex"]).decode() == task_id)["status"] == 4
        finally:
            runner.close()
    return {"gate": "CONNECTED_LEARNING_APP", "passed": True, "rounds": args.rounds,
        "actual_process_restarts": restarts, "accepted": accepted, "duplicates": duplicates,
        "browser_verified": False, "hardware_verified": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=5)
    parser.add_argument("--restarts-per-round", type=int, default=10)
    parser.add_argument("--base-url", help="Existing synthetic loopback backend for browser-created tasks")
    parser.add_argument("--task-id", action="append", default=[], help="One browser-created synthetic task per round")
    args = parser.parse_args()
    if args.rounds < 1 or args.restarts_per_round < 1:
        parser.error("rounds and restarts must be positive")
    if args.base_url:
        parsed = urlparse(args.base_url)
        if parsed.scheme != "http" or parsed.hostname != "127.0.0.1" or not parsed.port or parsed.path or parsed.query or parsed.fragment or parsed.username:
            parser.error("base-url must be http://127.0.0.1:<port>")
        if len(args.task_id) != args.rounds:
            parser.error("external mode requires one explicit synthetic task-id per round")
    elif args.task_id:
        parser.error("task-id requires base-url")
    args.out_dir.mkdir(parents=True, exist_ok=True)
    exe = compile_runner(args)
    with tempfile.TemporaryDirectory(prefix="claw4-connected-") as temporary:
        directory = Path(temporary)
        if args.base_url:
            result = run_scenarios(args, exe, args.base_url, directory)
            (args.out_dir / "summary.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
            print(json.dumps(result))
            return
        os.environ["CLAW4_DATABASE_URL"] = "sqlite:///" + str(directory / "backend.db").replace(os.sep, "/")
        sys.path.insert(0, str(args.repo / "backend"))
        import uvicorn
        from app.main import app
        listener = socket.socket()
        listener.bind(("127.0.0.1", 0))
        base = f"http://127.0.0.1:{listener.getsockname()[1]}"
        server = uvicorn.Server(uvicorn.Config(app, access_log=False, log_level="error"))
        worker = threading.Thread(target=lambda: server.run(sockets=[listener]), daemon=True)
        worker.start()
        try:
            deadline = time.monotonic() + 10
            while not server.started:
                if not worker.is_alive() or time.monotonic() > deadline:
                    raise RuntimeError("local backend did not start")
                time.sleep(0.02)
            result = run_scenarios(args, exe, base, directory)
            (args.out_dir / "summary.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
            print(json.dumps(result))
        finally:
            server.should_exit = True
            worker.join(timeout=10)
            listener.close()
            from app.db import engine
            engine.dispose()


if __name__ == "__main__":
    main()
