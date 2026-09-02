# claw4/backend/app/main.py
# FastAPI in-memory contract mock backend (WB-STREAM-001 CP2).
#
# Endpoints (ARCHITECTURE.md §6.2, CP2 minimum set):
#   POST /api/v1/devices/register
#   POST /api/v1/devices/challenge      (two-phase auth step 1, CR-WB002-07)
#   POST /api/v1/devices/claim          (authenticated parent session, CR-WB002-08)
#   POST /api/v1/devices/auth           (two-phase auth step 2)
#   GET  /api/v1/children/{child_id}/tasks/today
#   POST /api/v1/events/batch           (single write entry point)
#   GET  /health
#
# All traffic is 127.0.0.1 only; no real database, NAS, child accounts or
# external services are used.

from __future__ import annotations

import logging
import time

from fastapi import Depends, FastAPI, Header, HTTPException, Request
from fastapi.responses import JSONResponse

from app import security
from app.schemas import (
    AuthRequest,
    AuthResponse,
    ChallengeRequest,
    ChallengeResponse,
    ClaimRequest,
    EventsBatchRequest,
    EventsBatchResponse,
    EventResult,
    HealthResponse,
    RegisterRequest,
    RegisterResponse,
    TodayTasksResponse,
    TaskOut,
)
from app.store import Store, Parent

logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s: %(message)s")
logger = logging.getLogger("claw4.mock")

app = FastAPI(title="Claw4 Contract Mock Backend", version="0.1.0")

# In-memory store, seeded with mock parents/children/tasks (no real accounts).
store = Store()
store.seed(
    Parent(parent_id="parent-1", token="mock-parent-token-1", child_ids=["child-1"]),
    {
        "child-1": [
            {
                "task_id": "task-00000000-0000-0000-0000-000000000001",
                "title": "完成练习册 P32",
                "subject": "math",
                "estimated_minutes": 25,
                "priority": "high",
                "status": "ready",
                "scheduled_date": "2026-09-02",
                "version": 3,
            }
        ]
    },
)
store.seed(
    Parent(parent_id="parent-2", token="mock-parent-token-2", child_ids=["child-2"]),
    {"child-2": []},
)

API_V1 = "/api/v1"

MVP_EVENT_TYPES = {
    "device.booted",
    "device.online",
    "device.offline",
    "task.started",
    "task.paused",
    "task.resumed",
    "task.completed",
    "task.skipped",
    "study.session.started",
    "study.session.completed",
}


def _now() -> int:
    return int(time.time())


def _auth_device(authorization: str | None) -> dict:
    """Validates the device Bearer token and returns its claims (CR-WB002-04)."""
    if not authorization or not authorization.startswith("Bearer "):
        raise HTTPException(status_code=401, detail="unauthorized")
    token = authorization[len("Bearer "):].strip()
    try:
        claims = security.decode_device_token(token)
    except ValueError:
        raise HTTPException(status_code=401, detail="unauthorized") from None
    if claims.get("scope") != "device":
        raise HTTPException(status_code=401, detail="unauthorized")
    return claims


@app.get("/health", response_model=HealthResponse, tags=["ops"])
def health() -> HealthResponse:
    return HealthResponse(status="ok")


@app.post(f"{API_V1}/devices/register", response_model=RegisterResponse,
          status_code=201, tags=["identity"])
def register(req: RegisterRequest) -> RegisterResponse:
    """Server-issues device_id + secret + one-time pairing code.

    CR-WB002-04: device identity is NOT trusted from the client; device_id is
    issued by the server. The secret must never appear in logs.
    """
    dev = store.create_device(req.installation_id, req.model, req.fw_version)
    # Log only non-secret identifiers.
    logger.info("device registered: device_id=%s model=%s fw=%s",
                dev.device_id, dev.model, dev.fw_version)
    return RegisterResponse(
        device_id=dev.device_id,
        device_secret=dev.device_secret,
        pairing_code=dev.pairing_code,
        pairing_code_expires_in=900,
    )


@app.post(f"{API_V1}/devices/challenge", response_model=ChallengeResponse,
          tags=["identity"])
def challenge(req: ChallengeRequest) -> ChallengeResponse:
    """Two-phase auth step 1: issue a one-time challenge (CR-WB002-07)."""
    dev = store.get_device(req.device_id)
    if dev is None:
        # Generic error; never disclose whether a device exists.
        raise HTTPException(status_code=404, detail="not_found")
    ch = store.create_challenge(req.device_id)
    logger.info("challenge issued: device_id=%s challenge_id=%s",
                req.device_id, ch.challenge_id)
    return ChallengeResponse(
        challenge_id=ch.challenge_id,
        nonce=ch.nonce,
        expires_at=int(ch.expires_at),
    )


@app.post(f"{API_V1}/devices/auth", response_model=AuthResponse,
          tags=["identity"])
def auth(req: AuthRequest) -> AuthResponse:
    """Two-phase auth step 2: verify challenge signature, issue short token.

    CR-WB002-07: challenge is single-use, expires, and is bound to device_id +
    challenge_id + nonce. Replay or expiry -> 401. Token carries device/child
    bindings (CR-WB002-04).
    """
    dev = store.get_device(req.device_id)
    if dev is None:
        raise HTTPException(status_code=401, detail="unauthorized")
    ch = store.get_active_challenge(req.challenge_id, req.device_id, req.nonce)
    if ch is None:
        # Expired / already used / not for this device.
        raise HTTPException(status_code=401, detail="unauthorized")
    if not security.verify_challenge_signature(
            dev.device_secret, req.device_id, req.challenge_id, req.nonce,
            req.challenge_signature):
        raise HTTPException(status_code=401, detail="unauthorized")
    # Consume only after signature verification. The atomic consume makes two
    # concurrent valid replays race safely: exactly one succeeds.
    if store.consume_challenge(req.challenge_id, req.device_id, req.nonce) is None:
        raise HTTPException(status_code=401, detail="unauthorized")
    token = security.issue_device_token(dev.device_id, store.child_ids_of(dev.device_id))
    logger.info("device authed: device_id=%s", dev.device_id)
    return AuthResponse(access_token=token, expires_in=security.TOKEN_TTL_SECONDS)


@app.post(f"{API_V1}/devices/claim", tags=["identity"])
def claim(req: ClaimRequest,
          authorization: str | None = Header(default=None)) -> JSONResponse:
    """Binds a device to an authenticated parent's authorized child.

    CR-WB002-08: MUST be called by an authenticated parent session; parent_id
    is derived from the auth context (never from the request body, which has
    no parent_id field). The target child MUST belong to the parent's
    authorized scope; all failures return one generic error so child
    existence is never disclosed.
    """
    parent_id = security.parent_token_ok((authorization or "").removeprefix("Bearer ").strip(),
                                         store) if authorization else None
    if parent_id is None:
        raise HTTPException(status_code=401, detail="unauthorized")
    if req.child_id not in store.parent_child_ids(parent_id):
        # Generic external error; do not disclose child existence.
        raise HTTPException(status_code=403, detail="operation_not_allowed")
    dev = store.pair_device(req.pairing_code, parent_id, req.child_id)
    if dev is None:
        raise HTTPException(status_code=403, detail="operation_not_allowed")
    logger.info("device paired: device_id=%s child_id=%s", dev.device_id, req.child_id)
    return JSONResponse({"device_id": dev.device_id, "paired": True})


@app.get(f"{API_V1}/children/{{child_id}}/tasks/today",
         response_model=TodayTasksResponse, tags=["tasks"])
def tasks_today(child_id: str,
                authorization: str | None = Header(default=None)) -> TodayTasksResponse:
    """Today's tasks. The token's device/child binding is enforced (CR-WB002-04)."""
    claims = _auth_device(authorization)
    device_id = claims.get("sub")
    current_bindings = set(store.child_ids_of(device_id))
    if child_id not in claims.get("child_ids", []) or child_id not in current_bindings:
        raise HTTPException(status_code=403, detail="forbidden")
    tasks = store.tasks_for_child(child_id)
    return TodayTasksResponse(
        date="2026-09-02",
        tasks=[TaskOut(**t) for t in tasks],
    )


@app.post(f"{API_V1}/events/batch", response_model=EventsBatchResponse,
          tags=["sync"])
def events_batch(req: EventsBatchRequest,
                 authorization: str | None = Header(default=None)) -> EventsBatchResponse:
    """Single authoritative device write entry (ARCHITECTURE.md §6.2 / D9).

    Per-event processing order (CR-WB002-06):
      1) auth (401) and ownership/binding check (per-event rejected)
      2) (device_id, event_id) idempotency lookup — identical digest -> duplicate,
         different digest -> conflict
      3) only NEW events get sequence continuity check:
         seq == last_acked+1 -> accepted (persist, advance ACK)
         seq >  last_acked+1 -> gap (do not advance)
         seq <= last_acked   -> rejected (regression, do not advance)
    """
    claims = _auth_device(authorization)
    if claims.get("sub") != req.device_id:
        # Token belongs to a different device: impersonation attempt.
        raise HTTPException(status_code=403, detail="forbidden")
    # The architecture requires complete batches for one device to be
    # serialized. FastAPI sync handlers run in a thread pool, so an explicit
    # per-device lock is required even for this in-memory mock.
    with store.device_batch_lock(req.device_id):
        dev = store.get_device(req.device_id)
        if dev is None:
            raise HTTPException(status_code=404, detail="not_found")

        # Token claims are a signed snapshot, but current server-side bindings
        # remain authoritative and are revalidated on every request/event.
        allowed_children = set(claims.get("child_ids", [])) & set(
            store.child_ids_of(req.device_id)
        )
        results: list[EventResult] = []
        rejected_meta: list[dict] = []
        gap_meta: list[dict] = []
        accepted = 0
        duplicates = 0

        for ev in req.events:
            # (a) binding check per event (CR-WB002-04). Both envelope device_id
            # and child_id are untrusted input and must match current bindings.
            if ev.device_id != req.device_id:
                rejected_meta.append({"event_id": ev.event_id, "sequence": ev.sequence,
                                      "status": "rejected", "reason": "device_not_bound"})
                results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                           status="rejected", http_status=403))
                continue
            if ev.child_id not in allowed_children:
                rejected_meta.append({"event_id": ev.event_id, "sequence": ev.sequence,
                                      "status": "rejected", "reason": "child_not_bound"})
                results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                           status="rejected", http_status=403))
                continue
            # (b) reject malformed new/replayed envelopes before idempotency
            # success can be granted. An invalid replay never advances ACK.
            if ev.type not in MVP_EVENT_TYPES or ev.version != 1 \
                    or ev.timestamp_source not in {"rtc", "local"} or ev.sequence <= 0:
                rejected_meta.append({"event_id": ev.event_id, "sequence": ev.sequence,
                                      "status": "rejected", "reason": "invalid_event"})
                results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                           status="rejected", http_status=422))
                continue
            digest = store.compute_digest(
                req.device_id, ev.event_id, ev.child_id, ev.sequence, ev.timestamp,
                ev.timestamp_source, ev.type, ev.version, ev.payload
            )
            existing = store.lookup_event(req.device_id, ev.event_id)
            if existing is not None:
                # (c) idempotency before sequence continuity (CR-WB002-06).
                if existing.payload_digest == digest and existing.sequence == ev.sequence \
                        and existing.type == ev.type:
                    duplicates += 1
                    results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                               status="duplicate", http_status=200))
                else:
                    # Same event_id, different sequence/type/payload -> conflict.
                    rejected_meta.append({"event_id": ev.event_id, "sequence": ev.sequence,
                                          "status": "conflict",
                                          "reason": "event_id_reused_with_different_payload"})
                    results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                               status="conflict", http_status=409))
                continue
            # (d) sequence continuity for new, semantically valid events only.
            expected = dev.last_acked_sequence + 1
            if ev.sequence == expected:
                store.store_event(dev, ev.event_id, ev.child_id, ev.sequence, ev.type, digest)
                dev.last_acked_sequence = ev.sequence
                accepted += 1
                results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                           status="accepted", http_status=200))
            elif ev.sequence > expected:
                gap_meta.append({"event_id": ev.event_id, "sequence": ev.sequence,
                                 "status": "gap", "expected": expected})
                results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                           status="gap", http_status=409))
            else:  # ev.sequence <= dev.last_acked_sequence and never seen
                rejected_meta.append({"event_id": ev.event_id, "sequence": ev.sequence,
                                      "status": "rejected", "reason": "sequence_regression"})
                results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                           status="rejected", http_status=409))

        logger.info("batch: device_id=%s accepted=%d duplicates=%d rejected=%d gaps=%d ack=%d",
                    req.device_id, accepted, duplicates, len(rejected_meta), len(gap_meta),
                    dev.last_acked_sequence)
        return EventsBatchResponse(
            last_acked_sequence=dev.last_acked_sequence,
            server_time=_now(),
            results=results,
            accepted=accepted,
            duplicates=duplicates,
            rejected=rejected_meta,
            gaps=gap_meta,
        )


@app.exception_handler(Exception)
def _unhandled(request: Request, exc: Exception):
    logger.error("unhandled error: %s", exc)
    return JSONResponse(status_code=500, content={"detail": "internal_error"})
