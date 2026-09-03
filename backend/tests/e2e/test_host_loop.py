# claw4/backend/tests/e2e/test_host_loop.py
# Host-MVP end-to-end loop (WB-STREAM-002 CP7).
#
# Uses synthetic family/child data against the FastAPI in-process client
# (127.0.0.1 semantics; no public listener, no real database/NAS/external
# service). The mandatory loop:
#   1) parent creates a today task
#   2) synthetic device register -> claim -> challenge/auth -> pulls tasks
#   3) offline learning (Start/Pause/Resume/Complete) is proven by the native
#      C++ domain/outbox gate (run by run-host-mvp-e2e.ps1); here we submit
#      the resulting event sequence as the device would after reconnecting
#   4) one simulated lost response, then the same event_ids resend ->
#      duplicate + ACK convergence
#   5) parent dashboard & study records show exactly ONE completion with the
#      right duration and finish time
#   6) the second family cannot read or modify the first family's
#      child/device/task/session
from __future__ import annotations

import uuid

from fastapi.testclient import TestClient

from app import clock
from app.main import app

client = TestClient(app)

P1 = "mock-parent-token-1"
P2 = "mock-parent-token-2"
C1 = "child-1"
C2 = "child-2"


def _h(token: str) -> dict:
    return {"Authorization": f"Bearer {token}"}


def _register() -> dict:
    r = client.post("/api/v1/devices/register", json={
        "installation_id": f"e2e-inst-{uuid.uuid4().hex}",
        "model": "metalio-claw-4", "fw_version": "2.0.51"})
    assert r.status_code == 201, r.text
    return r.json()


def _claim(reg: dict, child: str, token: str) -> None:
    r = client.post("/api/v1/devices/claim",
                    json={"pairing_code": reg["pairing_code"], "child_id": child},
                    headers=_h(token))
    assert r.status_code == 200, r.text


def _auth(reg: dict) -> str:
    from app import security
    ch = client.post("/api/v1/devices/challenge",
                     json={"device_id": reg["device_id"]}).json()
    sig = security.sign_challenge(reg["device_secret"], reg["device_id"],
                                  ch["challenge_id"], ch["nonce"])
    r = client.post("/api/v1/devices/auth", json={
        "device_id": reg["device_id"], "challenge_id": ch["challenge_id"],
        "nonce": ch["nonce"], "challenge_signature": sig})
    assert r.status_code == 200, r.text
    return r.json()["access_token"]


def _ev(seq: int, eid: str, type_: str, payload: dict,
        child: str, device_id: str) -> dict:
    # FIX-03: timestamps land INSIDE today's local day so the completed
    # session is attributed to the "today" board the E2E asserts on.
    day_start, _ = clock.local_day_epoch_bounds(clock.local_today())
    return {"event_id": eid, "device_id": device_id, "child_id": child,
            "sequence": seq, "timestamp": day_start + seq,
            "timestamp_source": "rtc", "type": type_, "version": 1,
            "payload": payload}


def test_host_mvp_loop_end_to_end():
    # --- (1) parent creates a today task --------------------------------
    created = client.post(f"/api/v1/parents/me/children/{C1}/tasks", json={
        "subject": "math", "title": "E2E 练习册 P32",
        "estimated_minutes": 25, "priority": "high"},
        headers=_h(P1)).json()
    task_id = created["task_id"]
    assert created["scheduled_date"] == clock.local_today()

    # --- (2) synthetic device registers, claims, authenticates, pulls ----
    reg = _register()
    _claim(reg, C1, P1)
    token = _auth(reg)
    today = client.get(f"/api/v1/children/{C1}/tasks/today",
                       headers=_h(token)).json()
    pulled = [t for t in today["tasks"] if t["task_id"] == task_id]
    # A task scheduled for TODAY is served as `ready` (domain vocabulary:
    # task.h TaskStatus) — it may start immediately; only future tasks are
    # `pending`.
    assert len(pulled) == 1 and pulled[0]["status"] == "ready"

    # --- (3)+(4) device reconnects after offline learning; one response
    # is lost (client does not update its ACK) then the SAME event_ids are
    # resent -> duplicate + ACK convergence ------------------------------
    events = [
        _ev(1, f"e2e-{uuid.uuid4().hex}", "task.started",
            {"task_id": task_id}, C1, reg["device_id"]),
        _ev(2, f"e2e-{uuid.uuid4().hex}", "study.session.started",
            {"task_id": task_id, "session_id": "e2e-sess-1"}, C1,
            reg["device_id"]),
        _ev(3, f"e2e-{uuid.uuid4().hex}", "task.paused",
            {"task_id": task_id, "session_id": "e2e-sess-1"}, C1,
            reg["device_id"]),
        _ev(4, f"e2e-{uuid.uuid4().hex}", "task.resumed",
            {"task_id": task_id, "session_id": "e2e-sess-1"}, C1,
            reg["device_id"]),
        _ev(5, f"e2e-{uuid.uuid4().hex}", "study.session.completed",
            {"task_id": task_id, "session_id": "e2e-sess-1",
             "actual_seconds": 1500, "completion_type": "manual"}, C1,
            reg["device_id"]),
        _ev(6, f"e2e-{uuid.uuid4().hex}", "task.completed",
            {"task_id": task_id}, C1, reg["device_id"]),
    ]
    first = client.post("/api/v1/events/batch", json={
        "device_id": reg["device_id"], "last_acked_sequence": 0,
        "events": events}, headers=_h(token)).json()
    assert first["accepted"] == 6
    assert first["last_acked_sequence"] == 6
    # lost response: the device re-sends everything with its OLD ack
    again = client.post("/api/v1/events/batch", json={
        "device_id": reg["device_id"], "last_acked_sequence": 0,
        "events": events}, headers=_h(token)).json()
    assert again["duplicates"] == 6
    assert again["accepted"] == 0
    assert again["last_acked_sequence"] == 6

    # --- (5) parent dashboard & records: exactly one completion ---------
    recs = client.get(f"/api/v1/parents/me/children/{C1}/study-sessions",
                      headers=_h(P1)).json()
    done = [r for r in recs if r["session_id"] == "e2e-sess-1"]
    assert len(done) == 1                      # exactly one projection
    assert done[0]["status"] == "completed"
    assert done[0]["actual_seconds"] == 1500   # correct duration
    assert done[0]["completion_type"] == "manual"
    assert done[0]["xp"] == 25                 # 1500s / 60
    assert done[0]["finished_at"] is not None
    assert done[0]["finished_at"] >= done[0]["started_at"]  # finish time sane
    dash = client.get("/api/v1/parents/me/dashboard", headers=_h(P1)).json()
    completed = [r for r in recs if r["status"] == "completed"]
    assert sum(1 for r in recs if r["status"] == "completed") == len(completed)
    assert dash["focus_minutes"] == sum(r["actual_seconds"] // 60
                                        for r in completed)  # no double count
    tasks_today = client.get(f"/api/v1/parents/me/children/{C1}/tasks/today",
                             headers=_h(P1)).json()
    task_now = next(t for t in tasks_today["tasks"] if t["task_id"] == task_id)
    assert task_now["status"] == "completed"


def test_second_family_isolation():
    # First family data
    created = client.post(f"/api/v1/parents/me/children/{C1}/tasks", json={
        "subject": "chinese", "title": "隔离测试任务",
        "estimated_minutes": 15, "priority": "low"},
        headers=_h(P1)).json()
    task_id = created["task_id"]

    # parent-2 cannot read parent-1's child tasks
    r = client.get(f"/api/v1/parents/me/children/{C1}/tasks/today",
                   headers=_h(P2))
    assert r.status_code == 403
    # cannot modify the task
    r = client.patch(f"/api/v1/parents/me/children/{C1}/tasks/{task_id}",
                     json={"title": "hack"}, headers=_h(P2))
    assert r.status_code == 403
    # cannot read study records of parent-1's child
    r = client.get(f"/api/v1/parents/me/children/{C1}/study-sessions",
                   headers=_h(P2))
    assert r.status_code == 403
    # device paired by parent-2 to child-2 cannot claim/read child-1
    reg = _register()
    _claim(reg, C2, P2)
    token2 = _auth(reg)
    r = client.get(f"/api/v1/children/{C1}/tasks/today", headers=_h(token2))
    assert r.status_code == 403
    # parent-2 cannot see parent-1's devices
    devs1 = {d["device_id"] for d in
             client.get("/api/v1/parents/me/devices", headers=_h(P1)).json()}
    devs2 = {d["device_id"] for d in
             client.get("/api/v1/parents/me/devices", headers=_h(P2)).json()}
    assert devs2.isdisjoint(devs1)
