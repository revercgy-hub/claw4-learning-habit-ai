"""Resolve a voice connection against the NAS-local device pairing file."""

from functools import lru_cache
import json
import os
from pathlib import Path
import re

from core.utils.claw4_learning_exchange import PairedLearningSession


_UUID4 = re.compile(r"[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}")
_MAC = re.compile(r"(?:[0-9a-f]{2}:){5}[0-9a-f]{2}")
_SECRET = re.compile(r"[0-9a-f]{64}")
_LIMIT = 32_768


def _unique_pairs(pairs):
    result = dict(pairs)
    if len(result) != len(pairs):
        raise ValueError("duplicate pairing field")
    return result


def _read_pairings(path):
    if not path:
        return {}
    try:
        with Path(path).open("rb") as stream:
            raw = stream.read(_LIMIT + 1)
        if len(raw) > _LIMIT:
            return {}
        entries = json.loads(raw, object_pairs_hook=_unique_pairs)
        if not isinstance(entries, dict) or len(entries) > 32:
            return {}
        pairings = {}
        seen_devices = set()
        for hardware, record in entries.items():
            if _MAC.fullmatch(hardware) is None or not isinstance(record, dict):
                return {}
            if record.keys() != {"device_id", "child_id", "device_secret"}:
                return {}
            secret = record["device_secret"]
            if not isinstance(secret, str) or _SECRET.fullmatch(secret) is None:
                return {}
            session = PairedLearningSession(record["device_id"], record["child_id"], 1,
                                            secret.encode("ascii"))
            if not session.valid() or session.device_id in seen_devices:
                return {}
            seen_devices.add(session.device_id)
            pairings[hardware] = session
        return pairings
    except (OSError, ValueError, RecursionError):
        return {}


@lru_cache(maxsize=1)
def _configured_pairings():
    return _read_pairings(os.environ.get("CLAW4_LEARNING_PAIRINGS_FILE"))


def pairing_resolver(conn):
    """The MAC selects a provisioned key. Signed responses prove possession."""
    hardware = getattr(conn, "device_id", None)
    session_id = getattr(conn, "session_id", None)
    if (not isinstance(hardware, str) or len(hardware) != 17
            or not isinstance(session_id, str) or _UUID4.fullmatch(session_id) is None):
        return None
    record = _configured_pairings().get(hardware.lower())
    if record is None:
        return None
    websocket = conn.websocket

    def current():
        if conn.websocket is not websocket or conn.session_id != session_id or conn.stop_event.is_set():
            return None
        return record

    return current
