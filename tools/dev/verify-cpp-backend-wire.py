#!/usr/bin/env python3
"""AF1c: exercise C++ wire JSON against the real local FastAPI backend.

The C++ fixture owns every device JSON body. This script only performs local
HTTP calls and feeds backend responses back through the same C++ decoder.
Nothing binds a public listener or uses a real account/device.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import hmac
import os
import subprocess
import sys
import tempfile
import uuid
from pathlib import Path


def run_fixture(exe: Path, *args: str, stdin: str | None = None) -> str:
    result = subprocess.run([str(exe), *args], input=stdin, text=True,
                            capture_output=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"fixture failed: {args}: {result.stderr}")
    return result.stdout.strip()


def sign(secret: str, device: str, challenge: str, nonce: str) -> str:
    raw = f"{device}|{challenge}|{nonce}".encode()
    return base64.b64encode(hmac.new(secret.encode(), raw, hashlib.sha256).digest()).decode()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    exe = args.out_dir / "backend_wire_fixture.exe"
    main_dir = args.repo / "firmware" / "main"
    source = args.repo / "firmware" / "tests" / "host" / "backend_wire_fixture.cpp"
    codec = main_dir / "sync" / "wire_codec.cpp"
    compile_result = subprocess.run(
        [str(args.compiler), "-std=c++17", "-Wall", "-Wextra", "-Werror",
         "-I", str(main_dir), str(source), str(codec), "-o", str(exe)],
        capture_output=True, text=True, check=False)
    if compile_result.returncode != 0:
        print(compile_result.stdout + compile_result.stderr, file=sys.stderr)
        return compile_result.returncode

    # Keep backend import isolated to a throwaway SQLite database.
    tmp = Path(tempfile.mkdtemp(prefix="claw4-af1c-"))
    os.environ["CLAW4_DATABASE_URL"] = "sqlite:///" + str(tmp / "backend.db").replace(os.sep, "/")
    sys.path.insert(0, str(args.repo / "backend"))
    from fastapi.testclient import TestClient  # type: ignore
    from app import clock, security  # type: ignore
    from app.main import app  # type: ignore

    client = TestClient(app)
    parent_headers = {"Authorization": "Bearer mock-parent-token-1"}
    registration = client.post("/api/v1/devices/register", json={
        "installation_id": f"af1c-{uuid.uuid4().hex}",
        "model": "metalio-claw-4",
        "fw_version": "2.0.51",
    }).json()
    device = registration["device_id"]
    pairing = client.post("/api/v1/devices/claim", headers=parent_headers,
                          json={"pairing_code": registration["pairing_code"],
                                "child_id": "child-1"})
    assert pairing.status_code == 200, pairing.text

    challenge_body = run_fixture(exe, "challenge", device)
    challenge = client.post("/api/v1/devices/challenge", content=challenge_body,
                            headers={"content-type": "application/json"})
    assert challenge.status_code == 200, challenge.text
    challenge_json = challenge.json()
    signature = sign(registration["device_secret"], device,
                     challenge_json["challenge_id"], challenge_json["nonce"])
    auth_body = run_fixture(exe, "auth", device, challenge_json["challenge_id"],
                            challenge_json["nonce"], signature, "unused")
    auth = client.post("/api/v1/devices/auth", content=auth_body,
                       headers={"content-type": "application/json"})
    assert auth.status_code == 200, auth.text
    token_headers = {"Authorization": f"Bearer {auth.json()['access_token']}"}

    # Parent creates the task; device-side Today response is decoded by C++.
    task = client.post("/api/v1/parents/me/children/child-1/tasks",
                       headers=parent_headers,
                       json={"title": "AF1c task", "subject": "math",
                             "estimated_minutes": 20, "priority": "high"})
    assert task.status_code == 201, task.text
    today = client.get("/api/v1/children/child-1/tasks/today",
                       headers=token_headers)
    assert today.status_code == 200, today.text
    decoded_today = run_fixture(exe, "decode-today", stdin=today.text)
    today_count = int(decoded_today.rsplit(" ", 1)[1])
    assert decoded_today.startswith("TODAY ") and today_count >= 1, decoded_today

    day_start, _ = clock.local_day_epoch_bounds(clock.local_today())
    batch_body = run_fixture(exe, "batch", device, "0", "af1c-event-1", "child-1",
                             "1", str(int(day_start + 1)), "task.started")
    accepted = client.post("/api/v1/events/batch", content=batch_body,
                           headers={**token_headers, "content-type": "application/json"})
    assert accepted.status_code == 200, accepted.text
    decoded_batch = run_fixture(exe, "decode-batch", stdin=accepted.text)
    assert decoded_batch == "ACK 1 1", decoded_batch
    duplicate = client.post("/api/v1/events/batch", content=batch_body,
                            headers={**token_headers, "content-type": "application/json"})
    assert duplicate.status_code == 200, duplicate.text
    assert duplicate.json()["duplicates"] == 1, duplicate.text

    print("AF1C_CPP_BACKEND_WIRE=PASS")
    print(f"device={device} today={decoded_today} accepted={decoded_batch} duplicate=1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
