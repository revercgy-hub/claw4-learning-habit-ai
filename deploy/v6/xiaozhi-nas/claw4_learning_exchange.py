"""Sign a learning command and verify the response across the device link."""

from dataclasses import dataclass
import base64
import hashlib
import hmac
import re
import secrets


_FIELD = re.compile(r"[A-Za-z0-9_:.-]+")
_KINDS = frozenset({
    "QUERY_TODAY_TASKS", "START_TASK", "PAUSE_TASK", "RESUME_TASK",
    "COMPLETE_REQUEST", "QUERY_REMAINING_TIME", "SNOOZE_REMINDER",
})
_STATUSES = frozenset({
    "APPLIED", "CONFIRMATION_REQUIRED", "AMBIGUOUS_TASK", "INVALID_STATE",
    "STALE", "BUSY", "STORAGE_ERROR",
})
_MAX_INT64 = (1 << 63) - 1


def _valid_field(value, limit):
    return isinstance(value, str) and len(value) <= limit and _FIELD.fullmatch(value) is not None


def _positive(value):
    return type(value) is int and 0 < value <= _MAX_INT64


def _mac(key, *fields):
    message = "\n".join(fields).encode("ascii")
    return base64.b64encode(hmac.digest(key, message, hashlib.sha256)).decode("ascii")


@dataclass(frozen=True, repr=False)
class PairedLearningSession:
    """Trusted pairing data resolved on the server outside the voice payload."""

    device_id: str
    child_id: str
    session_epoch: int
    key: bytes

    def valid(self):
        return (_valid_field(self.device_id, 128) and _valid_field(self.child_id, 128)
                and _positive(self.session_epoch) and isinstance(self.key, bytes) and len(self.key) >= 32)


@dataclass(frozen=True)
class LearningCommand:
    command_id: str
    kind: str
    session_epoch: int
    expires_at_ms: int

    def fields(self, session):
        return (session.device_id, session.child_id, str(self.session_epoch), self.command_id,
                self.kind, str(self.expires_at_ms))

    def arguments(self, session):
        return {
            "command_id": self.command_id,
            "kind": self.kind,
            "session_epoch": str(self.session_epoch),
            "expires_at_ms": str(self.expires_at_ms),
            "mac": _mac(session.key, "claw4-learning-request-v1", *self.fields(session)),
        }


def new_command(kind, session, now_ms):
    if (not isinstance(session, PairedLearningSession) or not session.valid()
            or not isinstance(kind, str) or kind not in _KINDS
            or not _positive(now_ms) or now_ms > _MAX_INT64 - 30_000):
        return None
    return LearningCommand(secrets.token_hex(16), kind, session.session_epoch, now_ms + 30_000)


def session_challenge():
    return secrets.token_hex(16)


def verify_session(record, connection_id, challenge, response):
    if not isinstance(record, PairedLearningSession) or not record.valid() or not isinstance(response, dict):
        return None
    epoch = response.get("session_epoch")
    signature = response.get("mac")
    if (not _valid_field(connection_id, 36) or not _valid_field(challenge, 32)
            or not isinstance(epoch, str) or re.fullmatch(r"[1-9][0-9]{0,18}", epoch) is None
            or not _positive(int(epoch)) or not isinstance(signature, str) or not signature.isascii()):
        return None
    expected = _mac(record.key, "claw4-learning-session-v1", record.device_id, record.child_id,
                    connection_id, challenge, epoch)
    if not hmac.compare_digest(expected, signature):
        return None
    return PairedLearningSession(record.device_id, record.child_id, int(epoch), record.key)


def verify_result(command, session, response, now_ms):
    if (not _positive(now_ms) or now_ms >= command.expires_at_ms or not session.valid()
            or command.session_epoch != session.session_epoch
            or not isinstance(response, dict) or response.get("command_id") != command.command_id):
        return None
    status, signature = response.get("status"), response.get("mac")
    if not isinstance(status, str) or status not in _STATUSES or not isinstance(signature, str):
        return None
    value, snapshot = response.get("value"), response.get("snapshot_epoch_ms")
    if status == "APPLIED" and command.kind == "QUERY_TODAY_TASKS":
        if type(value) is not int or not 0 <= value <= 256 or not _positive(snapshot) or snapshot > now_ms:
            return None
    elif status == "APPLIED" and command.kind == "QUERY_REMAINING_TIME":
        if type(value) is not int or not 0 <= value <= 86400 or snapshot is not None:
            return None
    elif value is not None or snapshot is not None:
        return None
    expected = _mac(session.key, "claw4-learning-result-v1", *command.fields(session), status,
                    "-" if value is None else str(value), "-" if snapshot is None else str(snapshot))
    if len(signature) != len(expected) or not signature.isascii() or not hmac.compare_digest(expected, signature):
        return None
    verified = {"command_id": command.command_id, "status": status}
    if value is not None:
        if command.kind == "QUERY_TODAY_TASKS":
            verified.update(task_count=value, snapshot_epoch_ms=snapshot)
        else:
            verified["remaining_seconds"] = value
    return verified
