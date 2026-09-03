# claw4/backend/app/models.py
# SQLAlchemy ORM models for the persistent family backend (WB-STREAM-002 CP5).
#
# MVP identity/domain model per ARCHITECTURE.md §5/§9 and the CP5 task pack:
# parents/users (minimal), children, parent_child, devices, tasks,
# study_sessions, events, device_configs. Rewards stay at the single +XP
# field — no rewards economy. No real child/parent data is stored by this
# repo; the development session stub seeds synthetic rows only.
from __future__ import annotations

from sqlalchemy import (
    Column,
    Float,
    ForeignKey,
    Index,
    Integer,
    String,
    Text,
    UniqueConstraint,
)
from sqlalchemy.orm import declarative_base

Base = declarative_base()


class ParentRow(Base):
    __tablename__ = "parents"

    parent_id = Column(String(64), primary_key=True)
    display_name = Column(String(128), nullable=False, default="")
    # Development-session parent credential. This is an explicitly labelled
    # single-family DEV STUB, not production-grade login: no password hashing,
    # no OAuth, never written to logs (CP5 requirement 8).
    token = Column(String(256), nullable=False, default="")
    created_at = Column(Float, nullable=False, default=0.0)


class ChallengeRow(Base):
    """One-time auth challenges (CR-WB002-07): short TTL, single use, bound to
    device_id + nonce."""

    __tablename__ = "challenges"

    challenge_id = Column(String(64), primary_key=True)
    device_id = Column(String(64), ForeignKey("devices.device_id"), nullable=False)
    nonce = Column(String(256), nullable=False)
    expires_at = Column(Float, nullable=False, default=0.0)
    used = Column(Integer, nullable=False, default=0)


class ChildRow(Base):
    __tablename__ = "children"

    child_id = Column(String(64), primary_key=True)
    display_name = Column(String(128), nullable=False, default="")
    created_at = Column(Float, nullable=False, default=0.0)


class ParentChildRow(Base):
    """Many-to-many parent->child ownership. MVP is single-parent per child,
    but the join row keeps the permission check explicit and extensible."""

    __tablename__ = "parent_child"
    __table_args__ = (UniqueConstraint("parent_id", "child_id", name="uq_parent_child"),)

    id = Column(Integer, primary_key=True, autoincrement=True)
    parent_id = Column(String(64), ForeignKey("parents.parent_id"), nullable=False)
    child_id = Column(String(64), ForeignKey("children.child_id"), nullable=False)


class DeviceRow(Base):
    __tablename__ = "devices"

    device_id = Column(String(64), primary_key=True)
    # Not unique: the contract mock semantics let the same installation
    # register repeatedly (each call issues a fresh server-side device_id).
    installation_id = Column(String(128), nullable=False, default="")
    model = Column(String(64), nullable=False, default="")
    fw_version = Column(String(64), nullable=False, default="")
    device_secret = Column(String(256), nullable=False)
    pairing_code = Column(String(64), nullable=False, default="")
    pairing_code_expires_at = Column(Float, nullable=False, default=0.0)
    paired_parent_id = Column(String(64), ForeignKey("parents.parent_id"),
                              nullable=True)
    last_acked_sequence = Column(Integer, nullable=False, default=0)
    created_at = Column(Float, nullable=False, default=0.0)


class DeviceChildRow(Base):
    """device -> child binding (a device can serve several children; MVP uses
    one active child per device)."""

    __tablename__ = "device_children"
    __table_args__ = (UniqueConstraint("device_id", "child_id", name="uq_device_child"),)

    id = Column(Integer, primary_key=True, autoincrement=True)
    device_id = Column(String(64), ForeignKey("devices.device_id"), nullable=False)
    child_id = Column(String(64), ForeignKey("children.child_id"), nullable=False)


class DeviceConfigRow(Base):
    """MVP heartbeat/config rows. Battery stays NULL/unknown until a real
    value is observed — we never fabricate hardware readings."""

    __tablename__ = "device_configs"

    device_id = Column(String(64), ForeignKey("devices.device_id"),
                       primary_key=True)
    battery_percent = Column(Integer, nullable=True)  # NULL == unknown
    online = Column(Integer, nullable=False, default=0)  # 0/1 flag (MVP)
    last_seen_at = Column(Float, nullable=True)
    last_sync_at = Column(Float, nullable=True)
    config_json = Column(Text, nullable=False, default="{}")


class TaskRow(Base):
    __tablename__ = "tasks"
    __table_args__ = (
        Index("ix_tasks_child_date", "child_id", "scheduled_date"),
    )

    task_id = Column(String(64), primary_key=True)
    child_id = Column(String(64), ForeignKey("children.child_id"), nullable=False)
    subject = Column(String(32), nullable=False, default="")
    title = Column(String(256), nullable=False, default="")
    description = Column(Text, nullable=False, default="")
    task_type = Column(String(32), nullable=False, default="practice")
    estimated_minutes = Column(Integer, nullable=False, default=0)
    priority = Column(String(16), nullable=False, default="medium")
    # Domain words (task.h TaskStatus): 'ready' = today's runnable task;
    # 'pending' = future planned task (never the default for today's tasks).
    status = Column(String(16), nullable=False, default="pending")
    scheduled_date = Column(String(10), nullable=False, default="")
    version = Column(Integer, nullable=False, default=0)
    created_at = Column(Float, nullable=False, default=0.0)
    updated_at = Column(Float, nullable=False, default=0.0)


class StudySessionRow(Base):
    __tablename__ = "study_sessions"
    __table_args__ = (
        Index("ix_sessions_child_time", "child_id", "started_at"),
    )

    session_id = Column(String(64), primary_key=True)
    device_id = Column(String(64), ForeignKey("devices.device_id"), nullable=False)
    child_id = Column(String(64), ForeignKey("children.child_id"), nullable=False)
    task_id = Column(String(64), ForeignKey("tasks.task_id"), nullable=False)
    status = Column(String(16), nullable=False, default="created")
    completion_type = Column(String(16), nullable=True)  # NULL until terminal
    planned_minutes = Column(Integer, nullable=False, default=0)
    actual_seconds = Column(Integer, nullable=False, default=0)
    pause_count = Column(Integer, nullable=False, default=0)
    pause_seconds = Column(Integer, nullable=False, default=0)
    xp = Column(Integer, nullable=False, default=0)  # MVP single +XP field
    started_at = Column(Float, nullable=False, default=0.0)
    finished_at = Column(Float, nullable=True)


class EventRow(Base):
    __tablename__ = "events"
    __table_args__ = (
        UniqueConstraint("device_id", "event_id", name="uq_device_event"),
        # FIX-08: per-device sequence uniqueness is a database-level defense in
        # depth. The in-process RLock serializes batches only inside ONE Python
        # process; a multi-worker / multi-instance deployment relies on this
        # constraint (plus row locks / SELECT FOR UPDATE on PostgreSQL) so the
        # same device+sequence can never carry two different events.
        UniqueConstraint("device_id", "sequence", name="uq_device_sequence"),
        Index("ix_events_device_seq", "device_id", "sequence"),
    )

    id = Column(Integer, primary_key=True, autoincrement=True)
    device_id = Column(String(64), ForeignKey("devices.device_id"), nullable=False)
    child_id = Column(String(64), ForeignKey("children.child_id"), nullable=False)
    event_id = Column(String(64), nullable=False)
    sequence = Column(Integer, nullable=False)
    type = Column(String(64), nullable=False)
    version = Column(Integer, nullable=False, default=1)
    payload_json = Column(Text, nullable=False, default="{}")
    payload_digest = Column(String(64), nullable=False, default="")
    received_at = Column(Float, nullable=False, default=0.0)
    state = Column(String(16), nullable=False, default="accepted")
    # 'accepted' | 'duplicate' | 'deadletter'
    deadletter_reason = Column(Text, nullable=True)
