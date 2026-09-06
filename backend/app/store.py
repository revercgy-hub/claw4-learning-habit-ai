# claw4/backend/app/store.py
# SQLAlchemy-backed repository for the persistent family backend
# (WB-STREAM-002 CP5).
#
# Evolved from the WB-STREAM-001 in-memory mock: the public method names and
# the lightweight value dataclasses are preserved so the device contract
# endpoints and the old regression tests keep working unchanged, but every
# read/write now goes through one injected ORM Session. Business code never
# sees a SQL dialect: tests run on SQLite, deployment points at PostgreSQL.
#
# Transaction semantics: single operations flush through the request session;
# the FastAPI get_db dependency commits after a successful response and rolls
# back on error. The event batch endpoint therefore runs idempotent-insert +
# ACK + read-model projection in ONE database transaction (CP5 req. 4).
from __future__ import annotations

import hashlib
import json
import secrets
import threading
import uuid
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

from sqlalchemy import delete, select, update
from sqlalchemy.orm import Session

from app import clock, models as m

# ---------------------------------------------------------------------------
# value objects (same shape as the WB-STREAM-001 mock so callers do not leak
# ORM rows)
# ---------------------------------------------------------------------------
@dataclass
class Device:
    device_id: str
    device_secret: str
    installation_id: str
    model: str
    fw_version: str
    pairing_code: str
    pairing_code_expires_at: float
    paired_parent_id: Optional[str] = None
    paired_child_ids: List[str] = field(default_factory=list)
    last_acked_sequence: int = 0


@dataclass
class Challenge:
    challenge_id: str
    device_id: str
    nonce: str
    expires_at: float
    used: bool = False


@dataclass
class StoredEvent:
    event_id: str
    device_id: str
    child_id: str
    sequence: int
    type: str
    payload_digest: str
    received_at: float


@dataclass
class Parent:
    parent_id: str
    token: str
    child_ids: List[str]


# Per-device batch serialization registry: a batch for one device is processed
# under exactly one lock even when FastAPI runs handlers on a thread pool.
_BATCH_LOCKS: Dict[str, threading.RLock] = {}
_LOCK_GUARD = threading.Lock()


def _device_batch_lock(device_id: str) -> threading.RLock:
    with _LOCK_GUARD:
        lock = _BATCH_LOCKS.get(device_id)
        if lock is None:
            lock = threading.RLock()
            _BATCH_LOCKS[device_id] = lock
        return lock


def _row_to_device(row: m.DeviceRow, child_ids: List[str]) -> Device:
    return Device(
        device_id=row.device_id,
        device_secret=row.device_secret,
        installation_id=row.installation_id,
        model=row.model,
        fw_version=row.fw_version,
        pairing_code=row.pairing_code,
        pairing_code_expires_at=row.pairing_code_expires_at,
        paired_parent_id=row.paired_parent_id,
        paired_child_ids=child_ids,
        last_acked_sequence=row.last_acked_sequence,
    )


# Task status words follow the domain vocabulary (task.h TaskStatus).
_TASK_STATUS = {
    "task.started": "in_progress",
    "task.paused": "paused",
    "task.resumed": "in_progress",
    "task.completed": "completed",
    "task.skipped": "skipped",
}


class Store:
    """Repository over one ORM session (per request)."""

    def __init__(self, session: Session) -> None:
        self._s = session

    def commit(self) -> None:
        """Explicit commit (used inside the per-device batch lock so a
        committed ACK becomes visible to the next request immediately)."""
        self._s.commit()

    def rollback(self) -> None:
        self._s.rollback()

    # --- identity: parents / children ------------------------------------
    def seed_parent(self, parent: Parent) -> None:
        """Development-session stub seeding (single family). Idempotent."""
        row = self._s.get(m.ParentRow, parent.parent_id)
        if row is None:
            self._s.add(m.ParentRow(parent_id=parent.parent_id,
                                    display_name=f"parent-{parent.parent_id}",
                                    token=parent.token,
                                    created_at=clock.utc_now_epoch()))
        for child_id in parent.child_ids:
            if self._s.get(m.ChildRow, child_id) is None:
                self._s.add(m.ChildRow(child_id=child_id,
                                       display_name=f"child-{child_id}",
                                       created_at=clock.utc_now_epoch()))
            has = self._s.execute(
                select(m.ParentChildRow.id).where(
                    m.ParentChildRow.parent_id == parent.parent_id,
                    m.ParentChildRow.child_id == child_id)
            ).first()
            if has is None:
                self._s.add(m.ParentChildRow(parent_id=parent.parent_id,
                                             child_id=child_id))
        # Make pending inserts visible to later queries in the same session
        # (the session factory runs with autoflush=False).
        self._s.flush()

    def parent_id_for_token(self, parent_token: str) -> Optional[str]:
        if not parent_token:
            return None
        row = self._s.execute(
            select(m.ParentRow).where(m.ParentRow.token == parent_token)
        ).scalars().first()
        return row.parent_id if row else None

    def parent_child_ids(self, parent_id: str) -> List[str]:
        self._s.flush()  # surface pending parent_child inserts of this session
        rows = self._s.execute(
            select(m.ParentChildRow.child_id).where(
                m.ParentChildRow.parent_id == parent_id)
        ).scalars().all()
        return list(rows)

    def child_owned_by(self, parent_id: str, child_id: str) -> bool:
        return child_id in self.parent_child_ids(parent_id)

    # --- register ---------------------------------------------------------
    def create_device(self, installation_id: str, model: str,
                      fw_version: str) -> Device:
        device_id = str(uuid.uuid4())
        device_secret = secrets.token_hex(32)
        pairing_code = secrets.token_hex(3).upper()
        now = clock.utc_now_epoch()
        row = m.DeviceRow(
            device_id=device_id,
            installation_id=installation_id,
            model=model,
            fw_version=fw_version,
            device_secret=device_secret,
            pairing_code=pairing_code,
            pairing_code_expires_at=now + 900,
            created_at=now,
        )
        self._s.add(row)
        self._s.add(m.DeviceConfigRow(device_id=device_id))
        self._s.flush()
        _device_batch_lock(device_id)
        return _row_to_device(row, [])

    # --- challenge --------------------------------------------------------
    def create_challenge(self, device_id: str, ttl: float = 300.0) -> Challenge:
        ch = m.ChallengeRow(
            challenge_id=str(uuid.uuid4()),
            device_id=device_id,
            nonce=secrets.token_hex(16),
            expires_at=clock.utc_now_epoch() + ttl,
        )
        self._s.add(ch)
        self._s.flush()
        return Challenge(challenge_id=ch.challenge_id, device_id=ch.device_id,
                         nonce=ch.nonce, expires_at=ch.expires_at)

    def get_active_challenge(self, challenge_id: str, device_id: str,
                             nonce: str) -> Optional[Challenge]:
        row = self._s.get(m.ChallengeRow, challenge_id)
        if row is None or row.used or row.expires_at < clock.utc_now_epoch():
            return None
        if row.device_id != device_id or not secrets.compare_digest(row.nonce, nonce):
            return None
        return Challenge(challenge_id=row.challenge_id, device_id=row.device_id,
                         nonce=row.nonce, expires_at=row.expires_at,
                         used=bool(row.used))

    def consume_challenge(self, challenge_id: str, device_id: str,
                          nonce: str) -> Optional[Challenge]:
        """Atomically consumes a verified challenge. The conditional UPDATE
        makes concurrent replays race safely: exactly one caller wins."""
        now = clock.utc_now_epoch()
        res = self._s.execute(
            update(m.ChallengeRow)
            .where(m.ChallengeRow.challenge_id == challenge_id,
                   m.ChallengeRow.device_id == device_id,
                   m.ChallengeRow.nonce == nonce,
                   m.ChallengeRow.used == 0,
                   m.ChallengeRow.expires_at >= now)
            .values(used=1)
        )
        self._s.flush()
        if res.rowcount != 1:
            return None
        row = self._s.get(m.ChallengeRow, challenge_id)
        if row is None:
            return None
        return Challenge(challenge_id=row.challenge_id, device_id=row.device_id,
                         nonce=row.nonce, expires_at=row.expires_at, used=True)

    # --- claim ------------------------------------------------------------
    def pair_device(self, pairing_code: str, parent_id: str,
                    child_id: str) -> Optional[Device]:
        row = self._s.execute(
            select(m.DeviceRow).where(m.DeviceRow.pairing_code == pairing_code)
        ).scalars().first()
        if row is None or row.pairing_code_expires_at < clock.utc_now_epoch():
            return None
        if row.paired_parent_id is not None:
            return None
        row.paired_parent_id = parent_id
        row.pairing_code = ""  # single use
        self._s.add(m.DeviceChildRow(device_id=row.device_id, child_id=child_id))
        self._s.flush()
        return _row_to_device(row, [child_id])

    def revoke_child_binding(self, device_id: str, child_id: str) -> bool:
        res = self._s.execute(
            delete(m.DeviceChildRow).where(
                m.DeviceChildRow.device_id == device_id,
                m.DeviceChildRow.child_id == child_id)
        )
        return res.rowcount > 0

    # --- devices ----------------------------------------------------------
    def get_device(self, device_id: str) -> Optional[Device]:
        row = self._s.get(m.DeviceRow, device_id)
        if row is None:
            return None
        return _row_to_device(row, self.child_ids_of(device_id))

    def device_batch_lock(self, device_id: str) -> threading.RLock:
        return _device_batch_lock(device_id)

    def child_ids_of(self, device_id: str) -> List[str]:
        self._s.flush()  # surface pending device-child inserts of this session
        rows = self._s.execute(
            select(m.DeviceChildRow.child_id).where(
                m.DeviceChildRow.device_id == device_id)
        ).scalars().all()
        return list(rows)

    def advance_ack(self, dev: Device, sequence: int) -> None:
        """Persists the new ACK and mirrors it onto the in-memory value object
        so subsequent events of the SAME batch keep correct expected values."""
        self._s.execute(
            update(m.DeviceRow)
            .where(m.DeviceRow.device_id == dev.device_id)
            .values(last_acked_sequence=sequence)
        )
        self._s.flush()
        dev.last_acked_sequence = sequence

    def set_device_heartbeat(self, device_id: str, online: bool,
                             battery_percent: Optional[int],
                             fw_version: Optional[str]) -> None:
        """MVP heartbeat: battery stays NULL when unknown — never fabricated."""
        row = self._s.get(m.DeviceConfigRow, device_id)
        now = clock.utc_now_epoch()
        if row is None:
            row = m.DeviceConfigRow(device_id=device_id)
            self._s.add(row)
            self._s.flush()
        row.online = 1 if online else 0
        row.last_seen_at = now
        if battery_percent is not None:
            row.battery_percent = battery_percent
        if fw_version:
            dev = self._s.get(m.DeviceRow, device_id)
            if dev is not None:
                dev.fw_version = fw_version
        self._s.flush()

    def devices_of_parent(self, parent_id: str) -> List[dict]:
        rows = self._s.execute(
            select(m.DeviceRow).where(m.DeviceRow.paired_parent_id == parent_id)
        ).scalars().all()
        out: List[dict] = []
        for row in rows:
            cfg = self._s.get(m.DeviceConfigRow, row.device_id)
            last_sync = None
            last_event = self._s.execute(
                select(m.EventRow.received_at)
                .where(m.EventRow.device_id == row.device_id)
                .order_by(m.EventRow.sequence.desc())
            ).scalars().first()
            if last_event is not None:
                last_sync = last_event
            out.append({
                "device_id": row.device_id,
                "model": row.model,
                "fw_version": row.fw_version,
                "online": bool(cfg.online) if cfg else False,
                "battery_percent": cfg.battery_percent if cfg else None,  # NULL=unknown
                "last_seen_at": cfg.last_seen_at if cfg else None,
                "last_sync_at": last_sync,
                "last_acked_sequence": row.last_acked_sequence,
            })
        return out

    # --- events / ACK -----------------------------------------------------
    @staticmethod
    def compute_digest(device_id: str, event_id: str, child_id: str,
                       sequence: int, timestamp: int, timestamp_source: str,
                       type_: str, version: int, payload: dict) -> str:
        canonical = json.dumps(payload, sort_keys=True, ensure_ascii=False)
        raw = (f"{device_id}|{event_id}|{child_id}|{sequence}|{timestamp}|"
               f"{timestamp_source}|{type_}|{version}|{canonical}")
        return hashlib.sha256(raw.encode("utf-8")).hexdigest()

    def lookup_event(self, device_id: str, event_id: str) -> Optional[StoredEvent]:
        row = self._s.execute(
            select(m.EventRow).where(
                m.EventRow.device_id == device_id,
                m.EventRow.event_id == event_id)
        ).scalars().first()
        if row is None:
            return None
        return StoredEvent(event_id=row.event_id, device_id=row.device_id,
                           child_id=row.child_id, sequence=row.sequence,
                           type=row.type, payload_digest=row.payload_digest,
                           received_at=row.received_at)

    def lookup_sequence(self, device_id: str,
                        sequence: int) -> Optional[StoredEvent]:
        """Finds which event_id already occupies a (device_id, sequence) slot.

        FIX-08 pre-check companion of the UNIQUE(device_id, sequence) DB
        constraint: lets the batch handler answer a sequence collision with a
        per-event `conflict` instead of surfacing a raw integrity error. The
        unique constraint remains the authoritative cross-process guard.
        """
        row = self._s.execute(
            select(m.EventRow).where(
                m.EventRow.device_id == device_id,
                m.EventRow.sequence == sequence)
        ).scalars().first()
        if row is None:
            return None
        return StoredEvent(event_id=row.event_id, device_id=row.device_id,
                           child_id=row.child_id, sequence=row.sequence,
                           type=row.type, payload_digest=row.payload_digest,
                           received_at=row.received_at)

    def store_event(self, dev: Device, event_id: str, child_id: str,
                    sequence: int, type_: str, digest: str,
                    payload: Optional[dict] = None,
                    state: str = "accepted",
                    deadletter_reason: Optional[str] = None) -> StoredEvent:
        row = m.EventRow(
            device_id=dev.device_id,
            child_id=child_id,
            event_id=event_id,
            sequence=sequence,
            type=type_,
            payload_json=json.dumps(payload or {}, sort_keys=True,
                                    ensure_ascii=False),
            payload_digest=digest,
            received_at=clock.utc_now_epoch(),
            state=state,
            deadletter_reason=deadletter_reason,
        )
        self._s.add(row)
        self._s.flush()
        return StoredEvent(event_id=row.event_id, device_id=row.device_id,
                           child_id=row.child_id, sequence=row.sequence,
                           type=row.type, payload_digest=row.payload_digest,
                           received_at=row.received_at)

    def mark_deadletter(self, device_id: str, event_id: str, reason: str) -> None:
        """Business 4xx: keep the original row + a redacted reason so the same
        event_id can be repaired/replayed later."""
        row = self._s.execute(
            select(m.EventRow).where(
                m.EventRow.device_id == device_id,
                m.EventRow.event_id == event_id)
        ).scalars().first()
        if row is not None:
            row.state = "deadletter"
            row.deadletter_reason = reason
            self._s.flush()

    # --- read-model projection (CP5 req. 4, same transaction as insert) ---
    def project_event(self, dev: Device, ev: Any, digest: str) -> None:
        """Updates the Task/StudySession read models from an accepted event.
        Runs in the same DB transaction as the event insert, so a lost
        response followed by a duplicate resend never double-counts."""
        payload = dict(ev.payload or {})
        etype = ev.type

        if etype in _TASK_STATUS and payload.get("task_id"):
            task_row = self._s.get(m.TaskRow, payload["task_id"])
            if task_row is None:
                # Unknown task id (device-side synthetic task): record nothing;
                # the projection only tracks server-created tasks.
                return
            task_row.status = _TASK_STATUS[etype]
            task_row.updated_at = clock.utc_now_epoch()

        if etype == "study.session.started":
            session_id = payload.get("session_id")
            task_id = payload.get("task_id", "")
            if session_id:
                planned = 0
                if task_id:
                    t = self._s.get(m.TaskRow, task_id)
                    if t is not None:
                        planned = t.estimated_minutes
                self._s.add(m.StudySessionRow(
                    session_id=session_id,
                    device_id=dev.device_id,
                    child_id=ev.child_id,
                    task_id=task_id,
                    status="running",
                    completion_type=None,
                    planned_minutes=planned,
                    started_at=float(ev.timestamp),
                ))

        if etype == "study.session.completed":
            session_id = payload.get("session_id")
            task_id = payload.get("task_id", "")
            if not session_id:
                return
            # FIX-10: every completed/aborted/auto_saved session must carry its
            # task_id (the device reducer always includes it). A completed
            # event WITHOUT one cannot be attributed to a Task — it is safely
            # ignored instead of landing a task_id="" row (FK integrity).
            if not task_id:
                return
            row = self._s.get(m.StudySessionRow, session_id)
            actual = max(0, int(payload.get("actual_seconds", 0) or 0))
            pauses = max(0, int(payload.get("pause_count", 0) or 0))
            completion = payload.get("completion_type", "manual")
            if row is None:
                # The session may have started before this server came up; a
                # completed projection creates a terminal row from the event.
                self._s.add(m.StudySessionRow(
                    session_id=session_id,
                    device_id=dev.device_id,
                    child_id=ev.child_id,
                    task_id=task_id,
                    status="completed",
                    completion_type=completion,
                    actual_seconds=actual,
                    pause_count=pauses,
                    xp=actual // 60,  # MVP +XP: 1 per full minute
                    started_at=float(ev.timestamp),
                    finished_at=float(ev.timestamp),
                ))
                return
            row.status = "completed"
            row.completion_type = completion
            row.actual_seconds = actual
            row.pause_count = pauses
            row.xp = actual // 60
            row.finished_at = float(ev.timestamp)

        # task.completed also closes any still-open sessions for that task.
        if etype == "task.completed" and payload.get("task_id"):
            self._s.execute(
                update(m.StudySessionRow)
                .where(m.StudySessionRow.task_id == payload["task_id"],
                       m.StudySessionRow.status == "running")
                .values(status="completed", completion_type="manual",
                        finished_at=clock.utc_now_epoch())
            )

    # --- tasks ------------------------------------------------------------
    def tasks_for_child(self, child_id: str,
                        scheduled_date: Optional[str] = None) -> List[dict]:
        q = select(m.TaskRow).where(m.TaskRow.child_id == child_id)
        if scheduled_date:
            q = q.where(m.TaskRow.scheduled_date == scheduled_date)
        q = q.order_by(m.TaskRow.created_at)
        rows = self._s.execute(q).scalars().all()
        return [self._task_dict(r) for r in rows]

    @staticmethod
    def _task_dict(r: m.TaskRow) -> dict:
        return {
            "task_id": r.task_id,
            "title": r.title,
            "subject": r.subject,
            "description": r.description,
            "task_type": r.task_type,
            "estimated_minutes": r.estimated_minutes,
            "priority": r.priority,
            "status": r.status,
            "scheduled_date": r.scheduled_date,
            "version": r.version,
        }

    def create_task(self, parent_id: str, child_id: str, subject: str,
                    title: str, estimated_minutes: int, priority: str,
                    scheduled_date: str) -> Optional[dict]:
        if not self.child_owned_by(parent_id, child_id):
            return None
        now = clock.utc_now_epoch()
        task_id = str(uuid.uuid4())
        # Task status follows the domain vocabulary (task.h TaskStatus):
        #   ready   = scheduled for TODAY's local date -> may start now
        #   pending = scheduled for a FUTURE date (planned task, not yet runnable)
        # A task the device pulls via /tasks/today is therefore always `ready`.
        default_status = ("ready" if scheduled_date == clock.local_today()
                          else "pending")
        self._s.add(m.TaskRow(
            task_id=task_id, child_id=child_id, subject=subject, title=title,
            description="", task_type="practice",
            estimated_minutes=max(1, int(estimated_minutes)),
            priority=priority if priority in {"high", "medium", "low"} else "medium",
            status=default_status, scheduled_date=scheduled_date, version=1,
            created_at=now, updated_at=now,
        ))
        self._s.flush()
        row = self._s.get(m.TaskRow, task_id)
        return self._task_dict(row)

    def update_task(self, parent_id: str, child_id: str, task_id: str,
                    subject: Optional[str], title: Optional[str],
                    estimated_minutes: Optional[int],
                    priority: Optional[str]) -> Optional[dict]:
        if not self.child_owned_by(parent_id, child_id):
            return None
        row = self._s.get(m.TaskRow, task_id)
        if row is None or row.child_id != child_id:
            return None
        if title is not None:
            row.title = title
        if subject is not None:
            row.subject = subject
        if estimated_minutes is not None:
            row.estimated_minutes = max(1, int(estimated_minutes))
        if priority is not None and priority in {"high", "medium", "low"}:
            row.priority = priority
        row.version += 1
        row.updated_at = clock.utc_now_epoch()
        self._s.flush()
        return self._task_dict(row)

    # --- dashboard / history (parent API) --------------------------------
    def dashboard(self, parent_id: str, date: str) -> dict:
        children = self.parent_child_ids(parent_id)
        planned = 0
        completed = 0
        focus_minutes = 0
        active_label: Optional[str] = None
        # A StudySession belongs to the dashboard day of its `started_at` in
        # the configured family local timezone (FIX-03). Deriving the day from
        # started_at means a session that crossed midnight is credited to the
        # day it began, and sessions from other days are never aggregated.
        day_start, day_end = clock.local_day_epoch_bounds(date)
        for child_id in children:
            tasks = self.tasks_for_child(child_id, scheduled_date=date)
            planned += len(tasks)
            completed += sum(1 for t in tasks if t["status"] == "completed")
            sess_rows = self._s.execute(
                select(m.StudySessionRow)
                .where(m.StudySessionRow.child_id == child_id,
                       m.StudySessionRow.status == "completed",
                       m.StudySessionRow.started_at >= day_start,
                       m.StudySessionRow.started_at < day_end)
            ).scalars().all()
            for s in sess_rows:
                focus_minutes += s.actual_seconds // 60
            running = self._s.execute(
                select(m.StudySessionRow)
                .where(m.StudySessionRow.child_id == child_id,
                       m.StudySessionRow.status == "running")
            ).scalars().first()
            if running is not None and active_label is None:
                t = self._s.get(m.TaskRow, running.task_id)
                active_label = t.title if t is not None else "学习任务"
        rate = round(completed / planned * 100) if planned else 0
        return {
            "date": date,
            "planned_tasks": planned,
            "completed_tasks": completed,
            "completion_rate": rate,
            "focus_minutes": focus_minutes,
            "current_activity": active_label,
        }

    def study_sessions(self, parent_id: str, child_id: str) -> List[dict]:
        if not self.child_owned_by(parent_id, child_id):
            return []
        rows = self._s.execute(
            select(m.StudySessionRow)
            .where(m.StudySessionRow.child_id == child_id)
            .order_by(m.StudySessionRow.started_at.desc())
        ).scalars().all()
        out: List[dict] = []
        for r in rows:
            task_title = ""
            t = self._s.get(m.TaskRow, r.task_id)
            if t is not None:
                task_title = t.title
            out.append({
                "session_id": r.session_id,
                "task_id": r.task_id,
                "task_title": task_title,
                "status": r.status,
                "completion_type": r.completion_type,
                "actual_seconds": r.actual_seconds,
                "pause_count": r.pause_count,
                "pause_seconds": r.pause_seconds,
                "xp": r.xp,
                "started_at": r.started_at,
                "finished_at": r.finished_at,
            })
        return out

    # --- task status helpers used by parent API ---------------------------
    def task_status(self, task_id: str) -> Optional[str]:
        row = self._s.get(m.TaskRow, task_id)
        return row.status if row else None
