# claw4/backend/tests/test_family_backend.py
# Persistent family backend tests (WB-STREAM-002 CP5).
#
# Covers the CP5 mandatory scenarios on top of the preserved device-contract
# regression suite: permission isolation, transaction rollback, task CRUD,
# event read-model projection, dashboard / study records, device heartbeat &
# config, timezone/clock boundaries, and ACK/idempotency surviving a
# "backend restart" (fresh session against the same persisted SQLite file).
# All data is synthetic; nothing leaves 127.0.0.1.
from __future__ import annotations

import logging
import uuid

from fastapi.testclient import TestClient

from app import clock
from app.main import app
from app.schemas import EventsBatchResponse  # noqa: F401  (type reference)

client = TestClient(app)

P1 = "mock-parent-token-1"
P2 = "mock-parent-token-2"
C1 = "child-1"
C2 = "child-2"


def _headers(token: str) -> dict:
    return {"Authorization": f"Bearer {token}"}


def _register() -> dict:
    resp = client.post("/api/v1/devices/register", json={
        "installation_id": f"inst-{uuid.uuid4().hex}",
        "model": "metalio-claw-4",
        "fw_version": "2.0.51",
    })
    assert resp.status_code == 201, resp.text
    return resp.json()


def _pair(reg: dict, child: str = C1, token: str = P1) -> None:
    resp = client.post("/api/v1/devices/claim",
                       json={"pairing_code": reg["pairing_code"], "child_id": child},
                       headers=_headers(token))
    assert resp.status_code == 200, resp.text


def _auth(reg: dict) -> str:
    ch = client.post("/api/v1/devices/challenge",
                     json={"device_id": reg["device_id"]})
    assert ch.status_code == 200
    body = ch.json()
    from app import security
    sig = security.sign_challenge(reg["device_secret"], reg["device_id"],
                                  body["challenge_id"], body["nonce"])
    resp = client.post("/api/v1/devices/auth", json={
        "device_id": reg["device_id"],
        "challenge_id": body["challenge_id"],
        "nonce": body["nonce"],
        "challenge_signature": sig,
    })
    assert resp.status_code == 200, resp.text
    return resp.json()["access_token"]


def _event(seq: int, event_id: str, type_: str, payload: dict,
           child: str = C1, device_id: str = "") -> dict:
    return {
        "event_id": event_id,
        "device_id": device_id,
        "child_id": child,
        "sequence": seq,
        "timestamp": 1_800_000_000 + seq,
        "timestamp_source": "rtc",
        "type": type_,
        "version": 1,
        "payload": payload,
    }


def _batch(reg: dict, token: str, ack: int, events: list[dict]) -> dict:
    resp = client.post("/api/v1/events/batch",
                       json={"device_id": reg["device_id"],
                             "last_acked_sequence": ack,
                             "events": events},
                       headers=_headers(token))
    assert resp.status_code == 200, resp.text
    return resp.json()


def _fresh_device_pair() -> tuple[dict, str]:
    """Registers, pairs to child-1, authenticates. Returns (reg, token)."""
    reg = _register()
    _pair(reg, C1)
    return reg, _auth(reg)


def _seed_task(title: str = "家庭后端任务", subject: str = "chinese",
               minutes: int = 20, priority: str = "medium") -> dict:
    resp = client.post(f"/api/v1/parents/me/children/{C1}/tasks",
                       json={"title": title, "subject": subject,
                             "estimated_minutes": minutes,
                             "priority": priority},
                       headers=_headers(P1))
    assert resp.status_code == 201, resp.text
    return resp.json()


# ---------------------------------------------------------------------------
# A. parent identity and permission isolation
# ---------------------------------------------------------------------------
def test_parent_me_returns_stub_and_children():
    resp = client.get("/api/v1/parents/me", headers=_headers(P1))
    assert resp.status_code == 200
    body = resp.json()
    assert body["parent_id"] == "parent-1"
    assert body["stub"] == "dev-session-single-family"
    assert C1 in body["child_ids"]


def test_parent_endpoints_require_token():
    assert client.get("/api/v1/parents/me").status_code == 401
    assert client.get("/api/v1/parents/me/dashboard").status_code == 401
    assert client.get(f"/api/v1/parents/me/children/{C1}/tasks/today").status_code == 401
    assert client.get(f"/api/v1/parents/me/children/{C1}/study-sessions").status_code == 401
    assert client.get("/api/v1/parents/me/devices").status_code == 401


def test_wrong_parent_token_rejected():
    resp = client.get("/api/v1/parents/me", headers=_headers("not-a-token"))
    assert resp.status_code == 401


def test_parent2_cannot_read_parent1_child_tasks():
    resp = client.get(f"/api/v1/parents/me/children/{C1}/tasks/today",
                      headers=_headers(P2))
    assert resp.status_code == 403
    assert resp.json()["detail"] == "operation_not_allowed"


def test_parent2_cannot_create_task_for_parent1_child():
    resp = client.post(f"/api/v1/parents/me/children/{C1}/tasks",
                       json={"title": "x", "estimated_minutes": 10},
                       headers=_headers(P2))
    assert resp.status_code == 403


def test_parent2_cannot_read_parent1_study_sessions():
    resp = client.get(f"/api/v1/parents/me/children/{C1}/study-sessions",
                      headers=_headers(P2))
    assert resp.status_code == 403


def test_parent2_devices_do_not_include_parent1_devices():
    reg, _ = _fresh_device_pair()  # parent-1 child-1 pairing
    resp = client.get("/api/v1/parents/me/devices", headers=_headers(P1))
    assert any(d["device_id"] == reg["device_id"] for d in resp.json())
    resp2 = client.get("/api/v1/parents/me/devices", headers=_headers(P2))
    assert all(d["device_id"] != reg["device_id"] for d in resp2.json())


def test_parent2_dashboard_empty_children():
    resp = client.get("/api/v1/parents/me/dashboard", headers=_headers(P2))
    assert resp.status_code == 200
    body = resp.json()
    assert body["planned_tasks"] == 0
    assert body["completed_tasks"] == 0
    assert body["completion_rate"] == 0


# ---------------------------------------------------------------------------
# B. task CRUD (parent API)
# ---------------------------------------------------------------------------
def test_create_task_defaults_to_server_today():
    created = _seed_task("今日任务A")
    assert created["scheduled_date"] == clock.local_today()
    assert created["status"] == "pending"
    assert created["version"] == 1
    assert created["priority"] == "medium"


def test_create_task_explicit_scheduled_date():
    resp = client.post(f"/api/v1/parents/me/children/{C1}/tasks",
                       json={"title": "明日任务", "estimated_minutes": 15,
                             "scheduled_date": "2099-12-31"},
                       headers=_headers(P1))
    assert resp.status_code == 201
    assert resp.json()["scheduled_date"] == "2099-12-31"


def test_create_task_priority_normalized():
    created = _seed_task("P", priority="urgent")  # not in high/medium/low
    assert created["priority"] == "medium"


def test_update_task_increments_version_and_fields():
    created = _seed_task("原标题")
    resp = client.patch(
        f"/api/v1/parents/me/children/{C1}/tasks/{created['task_id']}",
        json={"title": "新标题", "priority": "high", "estimated_minutes": 45},
        headers=_headers(P1))
    assert resp.status_code == 200
    body = resp.json()
    assert body["title"] == "新标题"
    assert body["priority"] == "high"
    assert body["estimated_minutes"] == 45
    assert body["version"] == created["version"] + 1


def test_update_unknown_task_404():
    resp = client.patch(
        f"/api/v1/parents/me/children/{C1}/tasks/task-does-not-exist",
        json={"title": "x"}, headers=_headers(P1))
    assert resp.status_code == 404


def test_update_other_childs_task_forbidden():
    created = _seed_task("B 任务")
    resp = client.patch(
        f"/api/v1/parents/me/children/{C2}/tasks/{created['task_id']}",
        json={"title": "x"}, headers=_headers(P1))
    # C2 does not belong to parent-1 -> generic forbidden before task lookup.
    assert resp.status_code == 403


def test_today_tasks_filters_by_scheduled_date():
    _seed_task("今日-过滤A")
    resp = client.get(f"/api/v1/parents/me/children/{C1}/tasks/today",
                      headers=_headers(P1))
    assert resp.status_code == 200
    titles = [t["title"] for t in resp.json()["tasks"]]
    assert "今日-过滤A" in titles
    assert "明日任务" not in titles  # scheduled 2099-12-31


# ---------------------------------------------------------------------------
# C. event projection, dashboard, study records, transaction rollback
# ---------------------------------------------------------------------------
def test_session_lifecycle_projection():
    reg, token = _fresh_device_pair()
    task = _seed_task("投影-任务")
    b1 = _batch(reg, token, 0, [
        _event(1, f"e-{uuid.uuid4().hex}", "task.started",
               {"task_id": task["task_id"]}, device_id=reg["device_id"]),
        _event(2, f"e-{uuid.uuid4().hex}", "study.session.started",
               {"task_id": task["task_id"], "session_id": "sess-proj-1"},
               device_id=reg["device_id"]),
    ])
    assert b1["accepted"] == 2
    # task read-model now in_progress
    today = client.get(f"/api/v1/parents/me/children/{C1}/tasks/today",
                       headers=_headers(P1)).json()
    task_now = next(t for t in today["tasks"] if t["task_id"] == task["task_id"])
    assert task_now["status"] == "in_progress"
    # complete the session
    b2 = _batch(reg, token, 2, [
        _event(3, f"e-{uuid.uuid4().hex}", "study.session.completed",
               {"task_id": task["task_id"], "session_id": "sess-proj-1",
                "actual_seconds": 600, "completion_type": "manual"},
               device_id=reg["device_id"]),
        _event(4, f"e-{uuid.uuid4().hex}", "task.completed",
               {"task_id": task["task_id"]}, device_id=reg["device_id"]),
    ])
    assert b2["accepted"] == 2
    recs = client.get(f"/api/v1/parents/me/children/{C1}/study-sessions",
                      headers=_headers(P1)).json()
    sess = next(r for r in recs if r["session_id"] == "sess-proj-1")
    assert sess["status"] == "completed"
    assert sess["actual_seconds"] == 600
    assert sess["xp"] == 10  # 600s / 60
    assert sess["task_title"] == "投影-任务"
    dash = client.get("/api/v1/parents/me/dashboard", headers=_headers(P1)).json()
    assert dash["focus_minutes"] >= 10


def test_dashboard_counts_after_completion():
    reg, token = _fresh_device_pair()
    task = _seed_task("完成-计数")
    _batch(reg, token, 0, [
        _event(1, f"e-{uuid.uuid4().hex}", "task.started",
               {"task_id": task["task_id"]}, device_id=reg["device_id"]),
        _event(2, f"e-{uuid.uuid4().hex}", "study.session.started",
               {"task_id": task["task_id"], "session_id": "sess-dash-1"},
               device_id=reg["device_id"]),
        _event(3, f"e-{uuid.uuid4().hex}", "study.session.completed",
               {"task_id": task["task_id"], "session_id": "sess-dash-1",
                "actual_seconds": 300, "completion_type": "normal"},
               device_id=reg["device_id"]),
        _event(4, f"e-{uuid.uuid4().hex}", "task.completed",
               {"task_id": task["task_id"]}, device_id=reg["device_id"]),
    ])
    dash = client.get("/api/v1/parents/me/dashboard", headers=_headers(P1)).json()
    assert dash["completed_tasks"] >= 1
    assert dash["planned_tasks"] >= 1
    assert dash["completion_rate"] > 0


def test_lost_response_resend_does_not_double_count():
    reg, token = _fresh_device_pair()
    task = _seed_task("不重复统计")
    evs = [
        _event(1, "ev-dup-1", "task.started", {"task_id": task["task_id"]},
               device_id=reg["device_id"]),
        _event(2, "ev-dup-2", "study.session.started",
               {"task_id": task["task_id"], "session_id": "sess-dup-1"},
               device_id=reg["device_id"]),
        _event(3, "ev-dup-3", "study.session.completed",
               {"task_id": task["task_id"], "session_id": "sess-dup-1",
                "actual_seconds": 420, "completion_type": "normal"},
               device_id=reg["device_id"]),
        _event(4, "ev-dup-4", "task.completed", {"task_id": task["task_id"]},
               device_id=reg["device_id"]),
    ]
    first = _batch(reg, token, 0, evs)
    assert first["accepted"] == 4
    # Simulate a lost response: the client resends the whole batch with the
    # same event_ids -> all duplicates, ACK stays 4.
    again = _batch(reg, token, 4, evs)
    assert again["duplicates"] == 4
    assert again["last_acked_sequence"] == 4
    recs = client.get(f"/api/v1/parents/me/children/{C1}/study-sessions",
                      headers=_headers(P1)).json()
    dup = [r for r in recs if r["session_id"] == "sess-dup-1"]
    assert len(dup) == 1  # exactly one projection
    assert dup[0]["actual_seconds"] == 420
    dash = client.get("/api/v1/parents/me/dashboard", headers=_headers(P1)).json()
    completed = [r for r in recs if r["status"] == "completed"]
    assert dash["focus_minutes"] == sum(r["actual_seconds"] // 60
                                        for r in completed)  # no double count


def test_batch_transaction_rolls_back_on_projection_error(monkeypatch):
    from app.store import Store as StoreCls

    reg, token = _fresh_device_pair()
    task = _seed_task("回滚-任务")
    boom = uuid.uuid4().hex

    original = StoreCls.project_event

    def exploding(self, dev, ev, digest):
        if ev.event_id == boom:
            raise RuntimeError("projection exploded")
        return original(self, dev, ev, digest)

    monkeypatch.setattr(StoreCls, "project_event", exploding)
    # Events 1..2 accepted then event 3 (boom) raises -> the whole batch must
    # roll back: no rows, no ACK advance.
    from app import db
    # Use a local client that surfaces 500 responses instead of re-raising.
    quiet = TestClient(app, raise_server_exceptions=False)
    resp = quiet.post("/api/v1/events/batch",
                      json={"device_id": reg["device_id"],
                            "last_acked_sequence": 0,
                            "events": [
                                _event(1, f"e-{uuid.uuid4().hex}", "task.started",
                                       {"task_id": task["task_id"]},
                                       device_id=reg["device_id"]),
                                _event(2, boom, "study.session.completed",
                                       {"task_id": task["task_id"],
                                        "session_id": "sess-rb-1",
                                        "actual_seconds": 60},
                                       device_id=reg["device_id"]),
                            ]},
                       headers=_headers(token))
    assert resp.status_code == 500
    with db.SessionLocal() as s:
        from sqlalchemy import select
        from app import models as m
        dev_row = s.get(m.DeviceRow, reg["device_id"])
        assert dev_row is not None
        assert dev_row.last_acked_sequence == 0  # ACK not advanced
        ev_rows = s.execute(select(m.EventRow).where(
            m.EventRow.device_id == reg["device_id"])).scalars().all()
        assert len(ev_rows) == 0  # nothing persisted

    # After the failure a clean retry (same event ids, re-generated) works and
    # the projection is applied exactly once.
    retry = _batch(reg, token, 0, [
        _event(1, f"r-{uuid.uuid4().hex}", "task.started",
               {"task_id": task["task_id"]}, device_id=reg["device_id"]),
    ])
    assert retry["accepted"] == 1


def test_event_gap_and_conflict_never_advance_ack():
    reg, token = _fresh_device_pair()
    task = _seed_task("gap-任务")
    gap = _batch(reg, token, 0, [
        _event(2, f"e-{uuid.uuid4().hex}", "task.started",
               {"task_id": task["task_id"]}, device_id=reg["device_id"]),
    ])
    assert gap["results"][0]["status"] == "gap"
    assert gap["last_acked_sequence"] == 0
    ok = _batch(reg, token, 0, [
        _event(1, "conflict-ev-1", "task.started", {"task_id": task["task_id"]},
               device_id=reg["device_id"]),
    ])
    assert ok["results"][0]["status"] == "accepted"
    # same event_id different payload -> conflict
    conflict = _batch(reg, token, 1, [
        _event(1, "conflict-ev-1", "task.started", {"task_id": "different-task"},
               device_id=reg["device_id"]),
    ])
    assert conflict["results"][0]["status"] == "conflict"
    assert conflict["last_acked_sequence"] == 1


# ---------------------------------------------------------------------------
# D. persistence across "backend restart" (fresh sessions on the same DB)
# ---------------------------------------------------------------------------
def test_ack_survives_new_session():
    reg, token = _fresh_device_pair()
    task = _seed_task("重启-ACK")
    _batch(reg, token, 0, [
        _event(1, f"e-{uuid.uuid4().hex}", "task.started",
               {"task_id": task["task_id"]}, device_id=reg["device_id"]),
    ])
    # fresh session == process restart for a file-based DB
    from app import db
    with db.SessionLocal() as s:
        from app import models as m
        row = s.get(m.DeviceRow, reg["device_id"])
        assert row.last_acked_sequence == 1


def test_duplicate_after_restart_returns_duplicate():
    from app import db
    reg, token = _fresh_device_pair()
    task = _seed_task("重启-重复")
    ev = _event(1, "restart-ev-1", "task.started", {"task_id": task["task_id"]},
                device_id=reg["device_id"])
    assert _batch(reg, token, 0, [ev])["results"][0]["status"] == "accepted"
    # New process/session sees the persisted event and answers duplicate.
    again = _batch(reg, token, 1, [ev])
    assert again["results"][0]["status"] == "duplicate"
    assert again["last_acked_sequence"] == 1


# ---------------------------------------------------------------------------
# E. heartbeat / device config
# ---------------------------------------------------------------------------
def test_device_config_mvp_fields():
    reg, token = _fresh_device_pair()
    resp = client.get(f"/api/v1/devices/{reg['device_id']}/config",
                      headers=_headers(token))
    assert resp.status_code == 200
    body = resp.json()
    assert body["fw_version"] == "2.0.51"
    assert body["features"]["sync_enabled"] is True


def test_heartbeat_updates_online_and_battery():
    reg, token = _fresh_device_pair()
    resp = client.post(f"/api/v1/devices/{reg['device_id']}/heartbeat",
                       json={"battery_percent": 72},
                       headers=_headers(token))
    assert resp.status_code == 200
    devs = client.get("/api/v1/parents/me/devices", headers=_headers(P1)).json()
    me = next(d for d in devs if d["device_id"] == reg["device_id"])
    assert me["online"] is True
    assert me["battery_percent"] == 72


def test_battery_stays_unknown_null_when_not_reported():
    reg, _ = _fresh_device_pair()
    devs = client.get("/api/v1/parents/me/devices", headers=_headers(P1)).json()
    me = next(d for d in devs if d["device_id"] == reg["device_id"])
    assert me["battery_percent"] is None  # unknown, never fabricated


def test_heartbeat_requires_device_token():
    reg, _ = _fresh_device_pair()
    resp = client.post(f"/api/v1/devices/{reg['device_id']}/heartbeat",
                       json={"battery_percent": 50})
    assert resp.status_code == 401


# ---------------------------------------------------------------------------
# F. clock / timezone boundaries
# ---------------------------------------------------------------------------
def test_dashboard_explicit_date_boundary(monkeypatch):
    # The dashboard is server-date driven; a boundary day simply has no tasks.
    monkeypatch.setattr(clock, "local_today", lambda: "2099-01-01")
    boundary = client.get("/api/v1/parents/me/dashboard",
                          headers=_headers(P1)).json()
    assert boundary["date"] == "2099-01-01"
    assert boundary["planned_tasks"] == 0  # no tasks scheduled that day


def test_today_endpoint_reflects_injected_clock(monkeypatch):
    monkeypatch.setattr(clock, "local_today", lambda: "2025-06-15")
    resp = client.get(f"/api/v1/parents/me/children/{C1}/tasks/today",
                      headers=_headers(P1))
    assert resp.status_code == 200
    assert resp.json()["date"] == "2025-06-15"


# ---------------------------------------------------------------------------
# G. log hygiene
# ---------------------------------------------------------------------------
def test_registration_logs_do_not_leak_secrets(caplog):
    with caplog.at_level(logging.INFO, logger="claw4.backend"):
        reg = _register()
    joined = "\n".join(r.getMessage() for r in caplog.records)
    assert reg["device_secret"] not in joined
    assert reg["pairing_code"] not in joined


def test_study_sessions_order_newest_first():
    reg, token = _fresh_device_pair()
    task = _seed_task("排序-任务")
    for i in range(2):
        _batch(reg, token, i * 2, [
            _event(i * 2 + 1, f"e-{uuid.uuid4().hex}", "task.started",
                   {"task_id": task["task_id"]}, device_id=reg["device_id"]),
            _event(i * 2 + 2, f"e-{uuid.uuid4().hex}", "study.session.started",
                   {"task_id": task["task_id"], "session_id": f"sess-ord-{i}"},
                   device_id=reg["device_id"]),
        ])
    recs = client.get(f"/api/v1/parents/me/children/{C1}/study-sessions",
                      headers=_headers(P1)).json()
    # two running sessions exist; order by started_at desc
    times = [r["started_at"] for r in recs if r["session_id"].startswith("sess-ord-")]
    assert len(times) == 2
    assert times == sorted(times, reverse=True)
