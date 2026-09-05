#!/usr/bin/env python3
"""AF2b: repeat the same C++-generated study events and inspect parent views."""
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


def fixture(exe: Path, *args: str, stdin: str | None = None) -> str:
    result = subprocess.run([str(exe), *args], input=stdin, text=True,
                            capture_output=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(result.stderr or f"fixture failed: {args}")
    return result.stdout.strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=10)
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
    if compile_result.returncode:
        print(compile_result.stdout + compile_result.stderr, file=sys.stderr)
        return compile_result.returncode

    tmp = Path(tempfile.mkdtemp(prefix="claw4-af2b-"))
    os.environ["CLAW4_DATABASE_URL"] = "sqlite:///" + str(tmp / "backend.db").replace(os.sep, "/")
    sys.path.insert(0, str(args.repo / "backend"))
    from fastapi.testclient import TestClient  # type: ignore
    from app import clock, security  # type: ignore
    from app.main import app  # type: ignore

    client = TestClient(app)
    parent_headers = {"Authorization": "Bearer mock-parent-token-1"}
    registration = client.post("/api/v1/devices/register", json={
        "installation_id": f"af2b-{uuid.uuid4().hex}",
        "model": "metalio-claw-4", "fw_version": "2.0.51"}).json()
    device = registration["device_id"]
    paired = client.post("/api/v1/devices/claim", headers=parent_headers,
                         json={"pairing_code": registration["pairing_code"],
                               "child_id": "child-1"})
    assert paired.status_code == 200, paired.text
    challenge_body = fixture(exe, "challenge", device)
    challenge = client.post("/api/v1/devices/challenge", content=challenge_body,
                            headers={"content-type": "application/json"}).json()
    message = f"{device}|{challenge['challenge_id']}|{challenge['nonce']}".encode()
    signature = base64.b64encode(hmac.new(registration["device_secret"].encode(),
                                          message, hashlib.sha256).digest()).decode()
    auth_body = fixture(exe, "auth", device, challenge["challenge_id"],
                        challenge["nonce"], signature, "unused")
    auth = client.post("/api/v1/devices/auth", content=auth_body,
                       headers={"content-type": "application/json"})
    assert auth.status_code == 200, auth.text
    device_headers = {"Authorization": f"Bearer {auth.json()['access_token']}"}

    task = client.post("/api/v1/parents/me/children/child-1/tasks",
                       headers=parent_headers,
                       json={"title": "AF2b exactly once", "subject": "math",
                             "estimated_minutes": 20, "priority": "high"})
    assert task.status_code == 201, task.text
    task_id = task.json()["task_id"]
    day_start, _ = clock.local_day_epoch_bounds(clock.local_today())

    accepted = 0
    duplicates = 0
    for event_id, sequence, event_type, ack in [
        ("af2b-session-start", 1, "study.session.started", 0),
        ("af2b-session-complete", 2, "study.session.completed", 1),
        ("af2b-task-complete", 3, "task.completed", 2),
    ]:
        body = fixture(exe, "batch", device, str(ack), event_id, "child-1",
                       str(sequence), str(int(day_start + sequence)), event_type,
                       task_id, "af2b-session")
        for _ in range(args.rounds):
            response = client.post("/api/v1/events/batch", content=body,
                                   headers={**device_headers, "content-type": "application/json"})
            assert response.status_code == 200, response.text
            decoded = fixture(exe, "decode-batch", stdin=response.text)
            assert decoded == f"ACK {sequence} 1", decoded
            accepted += response.json()["accepted"]
            duplicates += response.json()["duplicates"]

    sessions = client.get("/api/v1/parents/me/children/child-1/study-sessions",
                          headers=parent_headers)
    assert sessions.status_code == 200, sessions.text
    rows = [row for row in sessions.json() if row["session_id"] == "af2b-session"]
    assert len(rows) == 1, sessions.text
    assert rows[0]["status"] == "completed", sessions.text
    today = client.get("/api/v1/children/child-1/tasks/today", headers=device_headers)
    assert today.status_code == 200, today.text
    task_rows = [row for row in today.json()["tasks"] if row["task_id"] == task_id]
    assert len(task_rows) == 1 and task_rows[0]["status"] == "completed", today.text
    print(f"AF2B_EXACTLY_ONCE=PASS rounds={args.rounds} accepted={accepted} "
          f"duplicates={duplicates} sessions={len(rows)} task_status=completed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
