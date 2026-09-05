# claw4/backend/app/main.py
# Persistent family backend API (WB-STREAM-002 CP5).
#
# Device contract endpoints evolved from the WB-STREAM-001 in-memory mock
# (register / challenge / claim / auth / today tasks / events / health) plus
# the new parent-side MVP API (dashboard, today-task create/update, study
# records, device list). All persistence goes through the SQLAlchemy Store
# repository; the FastAPI get_db dependency commits each request in one
# transaction and rolls back on error.
#
# Hard gates: 127.0.0.1 only; no real database/NAS/child accounts/external
# services; no secrets or real data in logs. Parent identity is an explicitly
# labelled single-family DEVELOPMENT SESSION STUB.
from __future__ import annotations

import logging

from fastapi import Depends, FastAPI, Header, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from sqlalchemy.orm import Session

from app import clock, db, security, store as store_mod
from app.db import get_db
from app.schemas import (
    AuthRequest,
    AuthResponse,
    ChallengeRequest,
    ChallengeResponse,
    ClaimRequest,
    DashboardResponse,
    DeviceConfigResponse,
    DeviceOut,
    EventsBatchRequest,
    EventsBatchResponse,
    EventResult,
    HealthResponse,
    HeartbeatRequest,
    ParentMeResponse,
    RegisterRequest,
    RegisterResponse,
    StudySessionOut,
    TaskCreateRequest,
    TaskOut,
    TaskUpdateRequest,
    TodayTasksResponse,
)
from app.store import Parent, Store

logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s: %(message)s")
logger = logging.getLogger("claw4.backend")

app = FastAPI(title="Claw4 Family Backend (Host MVP)", version="0.2.0")

# The host PWA is served by Vite on a separate loopback port during local
# development. Keep the allow-list explicit and loopback-only; production
# deployment must provide its own origin policy instead of widening this.
app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://127.0.0.1:4173",
        "http://127.0.0.1:4174",
        "http://127.0.0.1:5173",
        "http://localhost:4173",
        "http://localhost:4174",
        "http://localhost:5173",
    ],
    allow_credentials=False,
    allow_methods=["GET", "POST", "PATCH", "OPTIONS"],
    allow_headers=["Authorization", "Content-Type"],
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


# ---------------------------------------------------------------------------
# bootstrap (development-session stub data; no real families/children)
# ---------------------------------------------------------------------------
def bootstrap() -> None:
    db.create_schema()
    with db.SessionLocal() as s:
        repo = Store(s)
        repo.seed_parent(Parent(parent_id="parent-1", token="mock-parent-token-1",
                                child_ids=["child-1"]))
        repo.seed_parent(Parent(parent_id="parent-2", token="mock-parent-token-2",
                                child_ids=["child-2"]))
        existing = repo.tasks_for_child("child-1",
                                        scheduled_date=clock.local_today())
        if not existing:
            # One synthetic seed task per day so the Dashboard/today screens
            # have data; the date is always derived from the configured clock.
            repo.create_task(
                parent_id="parent-1", child_id="child-1",
                subject="math", title="完成练习册 P32",
                estimated_minutes=25, priority="high",
                scheduled_date=clock.local_today())
        s.commit()


bootstrap()


# ---------------------------------------------------------------------------
# dependencies
# ---------------------------------------------------------------------------
def get_store(db_session: Session = Depends(get_db)) -> Store:
    return Store(db_session)


def _auth_device(authorization: str | None) -> dict:
    """Validates the device Bearer token and returns its claims."""
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


def _auth_parent(authorization: str | None, store: Store) -> str:
    """Development-session parent credential check (CR-WB002-08: parent_id is
    ALWAYS derived from the authenticated context, never from a body field)."""
    if not authorization or not authorization.startswith("Bearer "):
        raise HTTPException(status_code=401, detail="unauthorized")
    parent_id = store.parent_id_for_token(
        authorization[len("Bearer "):].strip())
    if parent_id is None:
        raise HTTPException(status_code=401, detail="unauthorized")
    return parent_id


# ---------------------------------------------------------------------------
# ops
# ---------------------------------------------------------------------------
@app.get("/health", response_model=HealthResponse, tags=["ops"])
def health() -> HealthResponse:
    return HealthResponse(status="ok")


# ---------------------------------------------------------------------------
# device identity
# ---------------------------------------------------------------------------
@app.post(f"{API_V1}/devices/register", response_model=RegisterResponse,
          status_code=201, tags=["identity"])
def register(req: RegisterRequest, store: Store = Depends(get_store)) -> RegisterResponse:
    """Server-issues device_id + secret + one-time pairing code. The secret is
    never logged."""
    dev = store.create_device(req.installation_id, req.model, req.fw_version)
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
def challenge(req: ChallengeRequest, store: Store = Depends(get_store)) -> ChallengeResponse:
    """Two-phase auth step 1: one-time challenge (CR-WB002-07)."""
    dev = store.get_device(req.device_id)
    if dev is None:
        raise HTTPException(status_code=404, detail="not_found")
    ch = store.create_challenge(req.device_id)
    logger.info("challenge issued: device_id=%s", req.device_id)
    return ChallengeResponse(
        challenge_id=ch.challenge_id,
        nonce=ch.nonce,
        expires_at=int(ch.expires_at),
    )


@app.post(f"{API_V1}/devices/auth", response_model=AuthResponse,
          tags=["identity"])
def auth(req: AuthRequest, store: Store = Depends(get_store)) -> AuthResponse:
    """Two-phase auth step 2: verify signature, consume challenge, issue token."""
    dev = store.get_device(req.device_id)
    if dev is None:
        raise HTTPException(status_code=401, detail="unauthorized")
    ch = store.get_active_challenge(req.challenge_id, req.device_id, req.nonce)
    if ch is None:
        raise HTTPException(status_code=401, detail="unauthorized")
    if not security.verify_challenge_signature(
            dev.device_secret, req.device_id, req.challenge_id, req.nonce,
            req.challenge_signature):
        raise HTTPException(status_code=401, detail="unauthorized")
    if store.consume_challenge(req.challenge_id, req.device_id, req.nonce) is None:
        raise HTTPException(status_code=401, detail="unauthorized")
    token = security.issue_device_token(dev.device_id,
                                        store.child_ids_of(dev.device_id))
    logger.info("device authed: device_id=%s", dev.device_id)
    return AuthResponse(access_token=token,
                        expires_in=security.TOKEN_TTL_SECONDS)


@app.post(f"{API_V1}/devices/claim", tags=["identity"])
def claim(req: ClaimRequest,
          authorization: str | None = Header(default=None),
          store: Store = Depends(get_store)) -> JSONResponse:
    """Binds a device to an authenticated parent's authorized child.
    parent_id is derived from the auth context; the body has no parent_id
    field. Failures return one generic error (no child-existence disclosure)."""
    parent_id = security.parent_token_ok(
        (authorization or "").removeprefix("Bearer ").strip(), store) \
        if authorization else None
    if parent_id is None:
        raise HTTPException(status_code=401, detail="unauthorized")
    if not store.child_owned_by(parent_id, req.child_id):
        raise HTTPException(status_code=403, detail="operation_not_allowed")
    dev = store.pair_device(req.pairing_code, parent_id, req.child_id)
    if dev is None:
        raise HTTPException(status_code=403, detail="operation_not_allowed")
    logger.info("device paired: device_id=%s child_id=%s",
                dev.device_id, req.child_id)
    return JSONResponse({"device_id": dev.device_id, "paired": True})


# ---------------------------------------------------------------------------
# device data (device token)
# ---------------------------------------------------------------------------
@app.get(f"{API_V1}/children/{{child_id}}/tasks/today",
         response_model=TodayTasksResponse, tags=["tasks"])
def tasks_today(child_id: str,
                authorization: str | None = Header(default=None),
                store: Store = Depends(get_store)) -> TodayTasksResponse:
    """Today's tasks for the device. Token device/child binding is enforced."""
    claims = _auth_device(authorization)
    device_id = claims.get("sub")
    current_bindings = set(store.child_ids_of(device_id))
    if child_id not in claims.get("child_ids", []) or \
            child_id not in current_bindings:
        raise HTTPException(status_code=403, detail="forbidden")
    tasks = store.tasks_for_child(child_id, scheduled_date=clock.local_today())
    return TodayTasksResponse(
        date=clock.local_today(),
        tasks=[TaskOut(**t) for t in tasks],
    )


@app.post(f"{API_V1}/events/batch", response_model=EventsBatchResponse,
          tags=["sync"])
def events_batch(req: EventsBatchRequest,
                 authorization: str | None = Header(default=None),
                 store: Store = Depends(get_store)) -> EventsBatchResponse:
    """Single authoritative device write entry (ARCHITECTURE.md §6.2 / D9).

    Per-event order (CR-WB002-06):
      1) auth (401) + ownership/binding check (per-event rejected)
      2) (device_id, event_id) idempotency — identical digest -> duplicate;
         different digest/sequence/type -> conflict
      3) only NEW events get sequence continuity: seq == ack+1 -> accepted;
         seq > ack+1 -> gap; seq <= ack -> rejected regression.

    Accepted events are persisted AND projected (Task/StudySession read
    models) in the SAME database transaction, so a lost response followed by
    a duplicate resend never double-counts statistics (CP5 req. 4).
    """
    claims = _auth_device(authorization)
    if claims.get("sub") != req.device_id:
        raise HTTPException(status_code=403, detail="forbidden")
    with store.device_batch_lock(req.device_id):
        try:
            return _process_batch(req, store, claims, allowed_children=None)
        except HTTPException:
            store.rollback()
            raise
        except Exception:
            store.rollback()
            raise


def _process_batch(req: EventsBatchRequest, store: Store, claims: dict,
                   allowed_children: set | None) -> EventsBatchResponse:
    """Per-event processing under the per-device lock. Commits inside the
    lock so the advanced ACK is immediately visible to the next serialized
    request (SQLite otherwise keeps the uncommitted write invisible)."""
    dev = store.get_device(req.device_id)
    if dev is None:
        raise HTTPException(status_code=404, detail="not_found")

    if allowed_children is None:
        allowed_children = set(claims.get("child_ids", [])) & set(
            store.child_ids_of(req.device_id)
        )

    results: list[EventResult] = []
    rejected_meta: list[dict] = []
    gap_meta: list[dict] = []
    accepted = 0
    duplicates = 0

    for ev in req.events:
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
        if ev.type not in MVP_EVENT_TYPES or ev.version != 1 \
                or ev.timestamp_source not in {"rtc", "local"} or ev.sequence <= 0:
            rejected_meta.append({"event_id": ev.event_id, "sequence": ev.sequence,
                                  "status": "rejected", "reason": "invalid_event"})
            results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                       status="rejected", http_status=422))
            continue
        digest = store.compute_digest(
            req.device_id, ev.event_id, ev.child_id, ev.sequence,
            ev.timestamp, ev.timestamp_source, ev.type, ev.version, ev.payload)
        existing = store.lookup_event(req.device_id, ev.event_id)
        if existing is not None:
            if existing.payload_digest == digest and \
                    existing.sequence == ev.sequence and existing.type == ev.type:
                duplicates += 1
                results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                           status="duplicate", http_status=200))
            else:
                rejected_meta.append({"event_id": ev.event_id, "sequence": ev.sequence,
                                      "status": "conflict",
                                      "reason": "event_id_reused_with_different_payload"})
                results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                           status="conflict", http_status=409))
            continue
        # FIX-08: (device_id, sequence) must be globally unique per device.
        # If this sequence slot is already taken by a DIFFERENT event_id (same
        # batch or earlier), answer per-event conflict — the UNIQUE DB
        # constraint stays the cross-process authority and this pre-check keeps
        # the answer clean and deterministic inside the serialized batch.
        occupied = store.lookup_sequence(req.device_id, ev.sequence)
        if occupied is not None and occupied.event_id != ev.event_id:
            rejected_meta.append({"event_id": ev.event_id, "sequence": ev.sequence,
                                  "status": "conflict",
                                  "reason": "sequence_already_used_by_other_event"})
            results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                       status="conflict", http_status=409))
            continue
        expected = dev.last_acked_sequence + 1
        if ev.sequence == expected:
            store.store_event(dev, ev.event_id, ev.child_id, ev.sequence,
                              ev.type, digest, payload=dict(ev.payload))
            store.advance_ack(dev, ev.sequence)
            store.project_event(dev, ev, digest)
            accepted += 1
            results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                       status="accepted", http_status=200))
        elif ev.sequence > expected:
            gap_meta.append({"event_id": ev.event_id, "sequence": ev.sequence,
                             "status": "gap", "expected": expected})
            results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                       status="gap", http_status=409))
        else:
            rejected_meta.append({"event_id": ev.event_id, "sequence": ev.sequence,
                                  "status": "rejected", "reason": "sequence_regression"})
            results.append(EventResult(sequence=ev.sequence, event_id=ev.event_id,
                                       status="rejected", http_status=409))

    store.commit()  # visible ACK inside the per-device lock
    ack_now = store.get_device(req.device_id)
    logger.info("batch: device_id=%s accepted=%d duplicates=%d rejected=%d gaps=%d ack=%d",
                req.device_id, accepted, duplicates, len(rejected_meta), len(gap_meta),
                ack_now.last_acked_sequence if ack_now else 0)
    return EventsBatchResponse(
        last_acked_sequence=ack_now.last_acked_sequence if ack_now else 0,
        server_time=int(clock.utc_now_epoch()),
        results=results,
        accepted=accepted,
        duplicates=duplicates,
        rejected=rejected_meta,
        gaps=gap_meta,
    )


@app.post(f"{API_V1}/devices/{{device_id}}/heartbeat", tags=["sync"])
def heartbeat(device_id: str, req: HeartbeatRequest,
              authorization: str | None = Header(default=None),
              store: Store = Depends(get_store)) -> JSONResponse:
    """MVP heartbeat: online + optional battery/fw. Battery stays NULL when
    unknown — never fabricate hardware values (CP5 req. 5)."""
    claims = _auth_device(authorization)
    if claims.get("sub") != device_id:
        raise HTTPException(status_code=403, detail="forbidden")
    dev = store.get_device(device_id)
    if dev is None:
        raise HTTPException(status_code=404, detail="not_found")
    store.set_device_heartbeat(device_id, online=True,
                               battery_percent=req.battery_percent,
                               fw_version=req.fw_version)
    return JSONResponse({"device_id": device_id, "accepted": True})


@app.get(f"{API_V1}/devices/{{device_id}}/config",
         response_model=DeviceConfigResponse, tags=["sync"])
def device_config(device_id: str,
                  authorization: str | None = Header(default=None),
                  store: Store = Depends(get_store)) -> DeviceConfigResponse:
    """MVP device config (feature flags etc.). No hardware guarantees."""
    claims = _auth_device(authorization)
    if claims.get("sub") != device_id:
        raise HTTPException(status_code=403, detail="forbidden")
    dev = store.get_device(device_id)
    if dev is None:
        raise HTTPException(status_code=404, detail="not_found")
    return DeviceConfigResponse(
        device_id=device_id,
        fw_version=dev.fw_version,
        features={k: True for k in ("sync_enabled", "offline_queue")},
    )


# ---------------------------------------------------------------------------
# parent API (development-session parent credential)
# ---------------------------------------------------------------------------
@app.get(f"{API_V1}/parents/me", response_model=ParentMeResponse,
         tags=["parent"])
def parents_me(authorization: str | None = Header(default=None),
               store: Store = Depends(get_store)) -> ParentMeResponse:
    parent_id = _auth_parent(authorization, store)
    return ParentMeResponse(
        parent_id=parent_id,
        stub="dev-session-single-family",
        child_ids=store.parent_child_ids(parent_id),
    )


@app.get(f"{API_V1}/parents/me/dashboard", response_model=DashboardResponse,
         tags=["parent"])
def parent_dashboard(authorization: str | None = Header(default=None),
                     store: Store = Depends(get_store)) -> DashboardResponse:
    """Dashboard: planned/completed/completion rate/focus minutes/current."""
    parent_id = _auth_parent(authorization, store)
    data = store.dashboard(parent_id, date=clock.local_today())
    return DashboardResponse(**data)


@app.get(f"{API_V1}/parents/me/children/{{child_id}}/tasks/today",
         response_model=TodayTasksResponse, tags=["parent"])
def parent_tasks_today(child_id: str,
                       authorization: str | None = Header(default=None),
                       store: Store = Depends(get_store)) -> TodayTasksResponse:
    parent_id = _auth_parent(authorization, store)
    if not store.child_owned_by(parent_id, child_id):
        raise HTTPException(status_code=403, detail="operation_not_allowed")
    tasks = store.tasks_for_child(child_id, scheduled_date=clock.local_today())
    return TodayTasksResponse(date=clock.local_today(),
                              tasks=[TaskOut(**t) for t in tasks])


@app.post(f"{API_V1}/parents/me/children/{{child_id}}/tasks",
          response_model=TaskOut, status_code=201, tags=["parent"])
def parent_create_task(child_id: str, req: TaskCreateRequest,
                       authorization: str | None = Header(default=None),
                       store: Store = Depends(get_store)) -> TaskOut:
    parent_id = _auth_parent(authorization, store)
    if not store.child_owned_by(parent_id, child_id):
        raise HTTPException(status_code=403, detail="operation_not_allowed")
    created = store.create_task(
        parent_id=parent_id, child_id=child_id,
        subject=req.subject, title=req.title,
        estimated_minutes=req.estimated_minutes, priority=req.priority,
        scheduled_date=req.scheduled_date or clock.local_today())
    if created is None:
        raise HTTPException(status_code=403, detail="operation_not_allowed")
    return TaskOut(**created)


@app.patch(f"{API_V1}/parents/me/children/{{child_id}}/tasks/{{task_id}}",
           response_model=TaskOut, tags=["parent"])
def parent_update_task(child_id: str, task_id: str, req: TaskUpdateRequest,
                       authorization: str | None = Header(default=None),
                       store: Store = Depends(get_store)) -> TaskOut:
    parent_id = _auth_parent(authorization, store)
    if not store.child_owned_by(parent_id, child_id):
        raise HTTPException(status_code=403, detail="operation_not_allowed")
    updated = store.update_task(
        parent_id=parent_id, child_id=child_id, task_id=task_id,
        subject=req.subject, title=req.title,
        estimated_minutes=req.estimated_minutes, priority=req.priority)
    if updated is None:
        raise HTTPException(status_code=404, detail="not_found")
    return TaskOut(**updated)


@app.get(f"{API_V1}/parents/me/children/{{child_id}}/study-sessions",
         response_model=list[StudySessionOut], tags=["parent"])
def parent_study_sessions(child_id: str,
                          authorization: str | None = Header(default=None),
                          store: Store = Depends(get_store)) -> list[StudySessionOut]:
    parent_id = _auth_parent(authorization, store)
    if not store.child_owned_by(parent_id, child_id):
        raise HTTPException(status_code=403, detail="operation_not_allowed")
    return [StudySessionOut(**s)
            for s in store.study_sessions(parent_id, child_id)]


@app.get(f"{API_V1}/parents/me/devices",
         response_model=list[DeviceOut], tags=["parent"])
def parent_devices(authorization: str | None = Header(default=None),
                   store: Store = Depends(get_store)) -> list[DeviceOut]:
    parent_id = _auth_parent(authorization, store)
    return [DeviceOut(**d) for d in store.devices_of_parent(parent_id)]


@app.exception_handler(Exception)
def _unhandled(request: Request, exc: Exception):
    logger.error("unhandled error: %s", exc)
    return JSONResponse(status_code=500, content={"detail": "internal_error"})
