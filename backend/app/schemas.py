# claw4/backend/app/schemas.py
# Pydantic request/response schemas for the contract mock backend.
"""Pydantic schemas (ARCHITECTURE.md §6.3).

Note: RegisterRequest intentionally has NO device_id (server-issued) and
ClaimRequest intentionally has NO parent_id (derived from the authenticated
parent context, CR-WB002-08). Extra request fields are silently ignored by
Pydantic, which is itself part of the contract test (self-reported parent_id
must be ignored).
"""

from __future__ import annotations

from typing import Any, Dict, List, Optional

from pydantic import BaseModel, Field


class RegisterRequest(BaseModel):
    installation_id: str
    model: str = "metalio-claw-4"
    fw_version: str = "2.0.51"


class RegisterResponse(BaseModel):
    device_id: str
    device_secret: str
    pairing_code: str
    pairing_code_expires_in: int


class ChallengeRequest(BaseModel):
    device_id: str


class ChallengeResponse(BaseModel):
    challenge_id: str
    nonce: str
    expires_at: int


class ClaimRequest(BaseModel):
    pairing_code: str
    child_id: str


class AuthRequest(BaseModel):
    device_id: str
    challenge_id: str
    nonce: str
    challenge_signature: str


class AuthResponse(BaseModel):
    access_token: str
    expires_in: int


class EventIn(BaseModel):
    event_id: str
    device_id: str
    child_id: str
    sequence: int
    timestamp: int
    timestamp_source: str = "local"
    type: str
    version: int = 1
    payload: Dict[str, Any] = Field(default_factory=dict)


class EventsBatchRequest(BaseModel):
    device_id: str
    last_acked_sequence: int = 0
    events: List[EventIn]


class EventResult(BaseModel):
    sequence: int
    event_id: str
    status: str  # accepted | duplicate | conflict | rejected | gap
    http_status: int


class EventsBatchResponse(BaseModel):
    last_acked_sequence: int
    server_time: int
    results: List[EventResult]
    accepted: int
    duplicates: int
    rejected: List[Dict[str, Any]]
    gaps: List[Dict[str, Any]]


class TaskOut(BaseModel):
    task_id: str
    title: str
    subject: str
    estimated_minutes: int
    priority: str
    status: str
    scheduled_date: str
    version: int


class TodayTasksResponse(BaseModel):
    date: str
    tasks: List[TaskOut]


# ---------------------------------------------------------------------------
# parent API (CP5)
# ---------------------------------------------------------------------------
class ParentMeResponse(BaseModel):
    parent_id: str
    stub: str  # "dev-session-single-family" — explicitly NOT production auth
    child_ids: List[str]


class TaskCreateRequest(BaseModel):
    subject: str = "other"
    title: str
    estimated_minutes: int = Field(ge=1, le=600)
    priority: str = "medium"  # high/medium/low
    scheduled_date: Optional[str] = None  # defaults to server-local today


class TaskUpdateRequest(BaseModel):
    subject: Optional[str] = None
    title: Optional[str] = None
    estimated_minutes: Optional[int] = Field(default=None, ge=1, le=600)
    priority: Optional[str] = None


class DashboardResponse(BaseModel):
    date: str
    planned_tasks: int
    completed_tasks: int
    completion_rate: int  # 0-100
    focus_minutes: int
    current_activity: Optional[str] = None


class StudySessionOut(BaseModel):
    session_id: str
    task_id: str
    task_title: str
    status: str
    completion_type: Optional[str] = None
    actual_seconds: int
    pause_count: int
    pause_seconds: int
    xp: int
    started_at: float
    finished_at: Optional[float] = None


class DeviceOut(BaseModel):
    device_id: str
    model: str
    fw_version: str
    online: bool
    battery_percent: Optional[int] = None  # None == unknown (never fabricated)
    last_seen_at: Optional[float] = None
    last_sync_at: Optional[float] = None
    last_acked_sequence: int


class HeartbeatRequest(BaseModel):
    battery_percent: Optional[int] = Field(default=None, ge=0, le=100)
    fw_version: Optional[str] = None


class DeviceConfigResponse(BaseModel):
    device_id: str
    fw_version: str
    features: Dict[str, bool]


class HealthResponse(BaseModel):
    status: str
    service: str = "claw4-family-backend"
    bind: str = "127.0.0.1"
