# claw4/backend/tests/test_mock_backend.py
# Contract tests for the FastAPI in-memory mock (WB-STREAM-001 CP2).
#
# Covers the mandatory scenarios from WB-STREAM-001_MVP_CORE.md §6:
#   1. register issues device_id; logs do not leak the secret
#   2. challenge expiry / reuse / cross-device reuse are rejected
#   3. claim cannot self-report parent_id; child outside the authenticated
#      parent's scope returns a generic external error
#   4. token device/child binding is enforced for task queries and every event
#   5. seq 42 accepted but response lost -> same event_id resend -> duplicate + ACK
#   6. same event_id with a different payload is rejected and not re-persisted
#   7. gaps, regressions and in-batch failures never advance the consecutive ACK
#   8. 401/403 do not delete pending business events; business 4xx return a
#      per-event fixable rejection

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
import logging
import time

from fastapi.testclient import TestClient

from app.main import app
from app import security
import app.main as main_mod

client = TestClient(app)

PARENT1_TOKEN = "mock-parent-token-1"
CHILD1 = "child-1"
CHILD2 = "child-2"


def _register_device() -> dict:
    resp = client.post("/api/v1/devices/register", json={
        "installation_id": "inst-test-001",
        "model": "metalio-claw-4",
        "fw_version": "2.0.51",
    })
    assert resp.status_code == 201, resp.text
    return resp.json()


def _challenge(device_id: str) -> dict:
    resp = client.post("/api/v1/devices/challenge", json={"device_id": device_id})
    assert resp.status_code == 200, resp.text
    return resp.json()


def _auth_device(device_id: str, secret: str, challenge: dict) -> str:
    sig = security.sign_challenge(secret, device_id, challenge["challenge_id"],
                                  challenge["nonce"])
    resp = client.post("/api/v1/devices/auth", json={
        "device_id": device_id,
        "challenge_id": challenge["challenge_id"],
        "nonce": challenge["nonce"],
        "challenge_signature": sig,
    })
    assert resp.status_code == 200, resp.text
    return resp.json()["access_token"]


def _pair(device_id: str, pairing_code: str, child_id: str = CHILD1,
          parent_token: str = PARENT1_TOKEN) -> dict:
    resp = client.post(
        "/api/v1/devices/claim",
        json={"pairing_code": pairing_code, "child_id": child_id},
        headers={"Authorization": f"Bearer {parent_token}"},
    )
    assert resp.status_code == 200, resp.text
    return resp.json()


def _event(sequence: int, event_id: str, child_id: str = CHILD1,
           payload: dict | None = None) -> dict:
    return {
        "event_id": event_id,
        "device_id": "",
        "child_id": child_id,
        "sequence": sequence,
        "timestamp": 1756718530,
        "timestamp_source": "rtc",
        "type": "study.session.completed",
        "version": 1,
        "payload": payload or {"session_id": f"sess-{sequence}", "actual_seconds": 1500},
    }


def _batch(device_id: str, token: str, last_acked: int, events: list[dict]) -> dict:
    for e in events:
        e["device_id"] = device_id
    resp = client.post(
        "/api/v1/events/batch",
        json={"device_id": device_id, "last_acked_sequence": last_acked,
              "events": events},
        headers={"Authorization": f"Bearer {token}"},
    )
    assert resp.status_code == 200, resp.text
    return resp.json()


# ---------------------------------------------------------------------------
# 1) register issues device_id; logs do not leak the secret
# ---------------------------------------------------------------------------

def test_register_issues_device_id_and_secret_not_logged(caplog):
    with caplog.at_level(logging.INFO):
        reg = _register_device()
    assert reg["device_id"]
    # secret must never appear in any log line.
    log_text = caplog.text
    assert reg["device_secret"] not in log_text
    assert reg["pairing_code"] not in log_text
    # pairing code is one-time and expires.
    assert reg["pairing_code_expires_in"] == 900


def test_register_installation_id_is_unique_each_time():
    reg1 = _register_device()
    reg2 = _register_device()
    assert reg1["device_id"] != reg2["device_id"]


# ---------------------------------------------------------------------------
# 2) challenge expiry / reuse / cross-device reuse rejected
# ---------------------------------------------------------------------------

def test_challenge_reuse_rejected():
    reg = _register_device()
    ch = _challenge(reg["device_id"])
    sig = security.sign_challenge(reg["device_secret"], reg["device_id"],
                                  ch["challenge_id"], ch["nonce"])
    body = {
        "device_id": reg["device_id"],
        "challenge_id": ch["challenge_id"],
        "nonce": ch["nonce"],
        "challenge_signature": sig,
    }
    assert client.post("/api/v1/devices/auth", json=body).status_code == 200
    # second use of the same challenge must fail (single-use).
    assert client.post("/api/v1/devices/auth", json=body).status_code == 401


def test_challenge_expired_rejected():
    from app.db import SessionLocal
    from app.store import Store

    reg = _register_device()
    # Insert an already-expired challenge directly into the store (ttl=-10
    # puts expires_at in the past without touching the process clock).
    with SessionLocal() as s:
        expired = Store(s).create_challenge(reg["device_id"], ttl=-10)
        s.commit()
    sig = security.sign_challenge(reg["device_secret"], reg["device_id"],
                                  expired.challenge_id, expired.nonce)
    resp = client.post("/api/v1/devices/auth", json={
        "device_id": reg["device_id"],
        "challenge_id": expired.challenge_id,
        "nonce": expired.nonce,
        "challenge_signature": sig,
    })
    assert resp.status_code == 401


def test_challenge_cross_device_rejected():
    reg_a = _register_device()
    reg_b = _register_device()
    ch_a = _challenge(reg_a["device_id"])
    # Device B tries to use device A's challenge.
    sig = security.sign_challenge(reg_b["device_secret"], reg_b["device_id"],
                                  ch_a["challenge_id"], ch_a["nonce"])
    resp = client.post("/api/v1/devices/auth", json={
        "device_id": reg_b["device_id"],
        "challenge_id": ch_a["challenge_id"],
        "nonce": ch_a["nonce"],
        "challenge_signature": sig,
    })
    assert resp.status_code == 401


def test_auth_wrong_signature_rejected():
    reg = _register_device()
    ch = _challenge(reg["device_id"])
    resp = client.post("/api/v1/devices/auth", json={
        "device_id": reg["device_id"],
        "challenge_id": ch["challenge_id"],
        "nonce": ch["nonce"],
        "challenge_signature": "YmFkIHNpZ25hdHVyZQ==",
    })
    assert resp.status_code == 401
    # A bad signature must not consume the challenge. Architecture §6.4.3
    # says it is invalidated after successful auth; a correct retry can win.
    sig = security.sign_challenge(reg["device_secret"], reg["device_id"],
                                  ch["challenge_id"], ch["nonce"])
    retry = client.post("/api/v1/devices/auth", json={
        "device_id": reg["device_id"],
        "challenge_id": ch["challenge_id"],
        "nonce": ch["nonce"],
        "challenge_signature": sig,
    })
    assert retry.status_code == 200


def test_concurrent_challenge_replay_has_exactly_one_winner():
    reg = _register_device()
    ch = _challenge(reg["device_id"])
    sig = security.sign_challenge(reg["device_secret"], reg["device_id"],
                                  ch["challenge_id"], ch["nonce"])
    body = {
        "device_id": reg["device_id"],
        "challenge_id": ch["challenge_id"],
        "nonce": ch["nonce"],
        "challenge_signature": sig,
    }

    def authenticate(_: int) -> int:
        local_client = TestClient(app)
        return local_client.post("/api/v1/devices/auth", json=body).status_code

    with ThreadPoolExecutor(max_workers=2) as pool:
        statuses = sorted(pool.map(authenticate, range(2)))
    assert statuses == [200, 401]


# ---------------------------------------------------------------------------
# 3) claim: no self-reported parent_id; child scope + generic errors
# ---------------------------------------------------------------------------

def test_claim_ignores_self_reported_parent_id():
    reg = _register_device()
    # Request body carries a forged parent_id field; it must be ignored and the
    # binding must use the authenticated parent context (parent-1).
    resp = client.post(
        "/api/v1/devices/claim",
        json={"pairing_code": reg["pairing_code"],
              "child_id": CHILD1,
              "parent_id": "parent-999"},  # forged; must be ignored
        headers={"Authorization": f"Bearer {PARENT1_TOKEN}"},
    )
    assert resp.status_code == 200, resp.text
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))
    claims = security.decode_device_token(token)
    assert claims["sub"] == reg["device_id"]
    assert CHILD1 in claims["child_ids"]


def test_claim_child_not_in_parent_scope_generic_error():
    reg = _register_device()
    # parent-1 does not own child-2 -> generic error, child existence hidden.
    resp = client.post(
        "/api/v1/devices/claim",
        json={"pairing_code": reg["pairing_code"], "child_id": CHILD2},
        headers={"Authorization": f"Bearer {PARENT1_TOKEN}"},
    )
    assert resp.status_code == 403
    assert resp.json()["detail"] == "operation_not_allowed"


def test_claim_invalid_pairing_code_generic_error():
    resp = client.post(
        "/api/v1/devices/claim",
        json={"pairing_code": "NOPE00", "child_id": CHILD1},
        headers={"Authorization": f"Bearer {PARENT1_TOKEN}"},
    )
    assert resp.status_code == 403
    assert resp.json()["detail"] == "operation_not_allowed"


def test_claim_without_parent_session_unauthorized():
    reg = _register_device()
    resp = client.post(
        "/api/v1/devices/claim",
        json={"pairing_code": reg["pairing_code"], "child_id": CHILD1},
    )
    assert resp.status_code == 401


# ---------------------------------------------------------------------------
# 4) token device/child binding enforced for tasks and every event
# ---------------------------------------------------------------------------

def test_tasks_today_enforces_token_child_binding():
    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))

    ok = client.get(f"/api/v1/children/{CHILD1}/tasks/today",
                    headers={"Authorization": f"Bearer {token}"})
    assert ok.status_code == 200
    assert ok.json()["tasks"][0]["title"] == "完成练习册 P32"

    # child-2 is not bound to this device -> forbidden.
    denied = client.get(f"/api/v1/children/{CHILD2}/tasks/today",
                        headers={"Authorization": f"Bearer {token}"})
    assert denied.status_code == 403


def test_tasks_today_requires_token():
    resp = client.get(f"/api/v1/children/{CHILD1}/tasks/today")
    assert resp.status_code == 401


def test_event_child_not_bound_is_rejected_per_event():
    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))
    events = [_event(1, "ev-bound-1", child_id=CHILD2)]  # child-2 not bound
    body = _batch(reg["device_id"], token, 0, events)
    assert body["results"][0]["status"] == "rejected"
    assert body["last_acked_sequence"] == 0


def test_event_device_id_must_match_batch_and_token():
    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))
    ev = _event(1, "ev-wrong-device")
    ev["device_id"] = "forged-device-id"
    resp = client.post(
        "/api/v1/events/batch",
        json={"device_id": reg["device_id"], "last_acked_sequence": 0,
              "events": [ev]},
        headers={"Authorization": f"Bearer {token}"},
    )
    assert resp.status_code == 200
    body = resp.json()
    assert body["results"][0]["status"] == "rejected"
    assert body["results"][0]["http_status"] == 403
    assert body["last_acked_sequence"] == 0


def test_current_server_binding_is_revalidated_after_token_issue():
    from app.db import SessionLocal
    from app.store import Store

    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))
    with SessionLocal() as s:
        assert Store(s).revoke_child_binding(reg["device_id"], CHILD1)
        s.commit()

    tasks = client.get(
        f"/api/v1/children/{CHILD1}/tasks/today",
        headers={"Authorization": f"Bearer {token}"},
    )
    assert tasks.status_code == 403
    event = _batch(reg["device_id"], token, 0, [_event(1, "ev-revoked-child")])
    assert event["results"][0]["status"] == "rejected"
    assert event["last_acked_sequence"] == 0


# ---------------------------------------------------------------------------
# 5) seq 42 accepted but response lost -> same event_id resend -> duplicate+ACK
# ---------------------------------------------------------------------------

def test_duplicate_after_lost_response():
    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))

    ev = _event(1, "ev-lost-1")
    first = _batch(reg["device_id"], token, 0, [ev])
    assert first["results"][0]["status"] == "accepted"
    assert first["last_acked_sequence"] == 1

    # Client "lost" the response and resends the exact same event.
    second = _batch(reg["device_id"], token, 0, [ev])
    assert second["results"][0]["status"] == "duplicate"
    assert second["last_acked_sequence"] == 1  # current consecutive ACK returned


# ---------------------------------------------------------------------------
# 6) same event_id different payload -> conflict, not re-persisted
# ---------------------------------------------------------------------------

def test_conflict_same_event_id_different_payload():
    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))

    ev1 = _event(1, "ev-conflict-1", payload={"session_id": "sess-a", "actual_seconds": 100})
    assert _batch(reg["device_id"], token, 0, [ev1])["results"][0]["status"] == "accepted"

    # Same event_id, different payload digest -> conflict, ACK stays at 1.
    ev2 = _event(2, "ev-conflict-1", payload={"session_id": "sess-a", "actual_seconds": 999})
    body = _batch(reg["device_id"], token, 1, [ev2])
    assert body["results"][0]["status"] == "conflict"
    assert body["last_acked_sequence"] == 1
    # The conflicting event must not be re-persisted: same event_id still maps
    # to the original digest.
    third = _batch(reg["device_id"], token, 1, [ev1])
    assert third["results"][0]["status"] == "duplicate"


def test_conflict_same_event_id_different_sequence():
    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))

    ev1 = _event(1, "ev-conflict-seq", payload={"actual_seconds": 100})
    assert _batch(reg["device_id"], token, 0, [ev1])["results"][0]["status"] == "accepted"

    # Same event_id but a different sequence number -> conflict (not duplicate).
    ev2 = _event(2, "ev-conflict-seq", payload={"actual_seconds": 100})
    body = _batch(reg["device_id"], token, 1, [ev2])
    assert body["results"][0]["status"] == "conflict"


# ---------------------------------------------------------------------------
# 7) gaps, regressions and in-batch failures never advance the consecutive ACK
# ---------------------------------------------------------------------------

def test_gap_does_not_advance_ack():
    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))

    # First event seq 1 accepted -> ACK=1. Then submit seq 3 (gap).
    assert _batch(reg["device_id"], token, 0, [_event(1, "ev-gap-1")])["last_acked_sequence"] == 1
    body = _batch(reg["device_id"], token, 1, [_event(3, "ev-gap-3")])
    assert body["results"][0]["status"] == "gap"
    assert body["last_acked_sequence"] == 1


def test_sequence_regression_rejected():
    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))

    assert _batch(reg["device_id"], token, 0, [_event(1, "ev-reg-1")])["last_acked_sequence"] == 1
    # Never-seen event with sequence <= ACK -> rejected (regression) and ACK
    # must NOT advance because of the regression itself.
    body = _batch(reg["device_id"], token, 1, [_event(1, "ev-reg-b")])
    assert body["results"][0]["status"] == "rejected"
    assert body["last_acked_sequence"] == 1


def test_in_batch_failure_does_not_advance_past_failure():
    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))

    # Batch: seq1 accepted, seq3 gap, seq4 should not be accepted past the gap.
    body = _batch(reg["device_id"], token, 0,
                  [_event(1, "ev-b1"), _event(3, "ev-b3"), _event(4, "ev-b4")])
    statuses = [r["status"] for r in body["results"]]
    assert statuses == ["accepted", "gap", "gap"]
    assert body["last_acked_sequence"] == 1


def test_batch_conflict_does_not_advance_past_conflict():
    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))

    ev1 = _event(1, "ev-bc-1", payload={"actual_seconds": 10})
    assert _batch(reg["device_id"], token, 0, [ev1])["last_acked_sequence"] == 1

    # Batch: seq2 uses ev-bc-1 (conflict), seq3 follows -> stays rejected-ish, ACK at 1.
    ev2 = _event(2, "ev-bc-1", payload={"actual_seconds": 99})
    ev3 = _event(3, "ev-bc-3", payload={"actual_seconds": 30})
    body = _batch(reg["device_id"], token, 1, [ev2, ev3])
    statuses = [r["status"] for r in body["results"]]
    assert statuses[0] == "conflict"
    # seq3 is a new event but its expected is 2 (ACK not advanced past conflict):
    # 3 > 1+1=2 -> gap.
    assert statuses[1] == "gap"
    assert body["last_acked_sequence"] == 1


def test_invalid_event_semantics_do_not_advance_ack():
    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))
    ev = _event(1, "ev-invalid-type")
    ev["type"] = "unrecognized.business.event"
    body = _batch(reg["device_id"], token, 0, [ev])
    assert body["results"][0]["status"] == "rejected"
    assert body["results"][0]["http_status"] == 422
    assert body["last_acked_sequence"] == 0


def test_replay_with_changed_immutable_envelope_is_not_duplicate():
    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))
    original = _event(1, "ev-envelope-conflict")
    assert _batch(reg["device_id"], token, 0, [original])["results"][0]["status"] == "accepted"

    changed = _event(1, "ev-envelope-conflict")
    changed["timestamp"] += 1
    replay = _batch(reg["device_id"], token, 1, [changed])
    assert replay["results"][0]["status"] == "conflict"
    assert replay["last_acked_sequence"] == 1


def test_concurrent_same_sequence_batches_are_serialized(monkeypatch):
    from app.store import Store as StoreCls

    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))

    original_store_event = StoreCls.store_event

    def slow_store_event(self, *args, **kwargs):
        # Opens a deterministic race window if the route loses the required
        # per-device batch lock.
        time.sleep(0.05)
        return original_store_event(self, *args, **kwargs)

    monkeypatch.setattr(StoreCls, "store_event", slow_store_event)

    def submit(event_id: str) -> dict:
        local_client = TestClient(app)
        event = _event(1, event_id)
        event["device_id"] = reg["device_id"]
        response = local_client.post(
            "/api/v1/events/batch",
            json={"device_id": reg["device_id"], "last_acked_sequence": 0,
                  "events": [event]},
            headers={"Authorization": f"Bearer {token}"},
        )
        assert response.status_code == 200
        return response.json()

    with ThreadPoolExecutor(max_workers=2) as pool:
        responses = list(pool.map(submit, ["ev-race-a", "ev-race-b"]))

    outcomes = sorted(body["results"][0]["status"] for body in responses)
    assert outcomes == ["accepted", "rejected"]
    assert all(body["last_acked_sequence"] == 1 for body in responses)


# ---------------------------------------------------------------------------
# 8) 401/403 do not delete pending business events; business 4xx per-event
# ---------------------------------------------------------------------------

def test_invalid_token_returns_401_and_state_unchanged():
    reg = _register_device()
    _pair(reg["device_id"], reg["pairing_code"], CHILD1)
    token = _auth_device(reg["device_id"], reg["device_secret"], _challenge(reg["device_id"]))

    # Establish ACK=1 with a valid token.
    assert _batch(reg["device_id"], token, 0, [_event(1, "ev-auth-1")])["last_acked_sequence"] == 1

    # Invalid/expired token -> 401; nothing is deleted or advanced.
    bad = client.post("/api/v1/events/batch",
                      json={"device_id": reg["device_id"], "last_acked_sequence": 1,
                            "events": [_event(2, "ev-auth-2")]},
                      headers={"Authorization": "Bearer bad.token.value"})
    assert bad.status_code == 401

    # Valid token still sees ACK=1 and the original event as duplicate.
    body = _batch(reg["device_id"], token, 1, [_event(1, "ev-auth-1")])
    assert body["results"][0]["status"] == "duplicate"
    assert body["last_acked_sequence"] == 1


def test_token_impersonating_other_device_rejected():
    reg_a = _register_device()
    reg_b = _register_device()
    _pair(reg_a["device_id"], reg_a["pairing_code"], CHILD1)
    token_a = _auth_device(reg_a["device_id"], reg_a["device_secret"],
                           _challenge(reg_a["device_id"]))
    # Device B tries to push events under device A's identity using A's token.
    resp = client.post("/api/v1/events/batch",
                       json={"device_id": reg_b["device_id"], "last_acked_sequence": 0,
                             "events": [_event(1, "ev-imp")]},
                       headers={"Authorization": f"Bearer {token_a}"})
    assert resp.status_code == 403
