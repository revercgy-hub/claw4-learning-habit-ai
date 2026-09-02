# claw4/backend/app/store.py
# In-memory store for the contract mock backend.
# No real database, NAS, child accounts or external services.
"""In-memory store for the contract mock backend.

Everything lives in process memory; no real database, NAS, child accounts or
external services are ever contacted (WB-STREAM-001 CP2 hard gates).
"""

from __future__ import annotations

import hashlib
import json
import secrets
import threading
import time
import uuid
from dataclasses import dataclass, field
from typing import Dict, List, Optional


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


class Store:
    """Thread-safe in-memory store with per-device event-batch serialization."""

    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._device_batch_locks: Dict[str, threading.RLock] = {}
        self.devices: Dict[str, Device] = {}
        self.challenges: Dict[str, Challenge] = {}
        # event registry: device_id -> event_id -> StoredEvent
        self.events: Dict[str, Dict[str, StoredEvent]] = {}
        self.parents: Dict[str, Parent] = {}
        self.tasks_by_child: Dict[str, list] = {}

    # --- seed data (mock only, no real child accounts) ---
    def seed(self, parent: Parent, tasks_by_child: Dict[str, list]) -> None:
        with self._lock:
            self.parents[parent.parent_id] = parent
            for child_id, tasks in tasks_by_child.items():
                self.tasks_by_child[child_id] = tasks

    # --- register ---
    def create_device(self, installation_id: str, model: str, fw_version: str) -> Device:
        device_id = str(uuid.uuid4())
        device_secret = secrets.token_hex(32)
        pairing_code = secrets.token_hex(3).upper()
        dev = Device(
            device_id=device_id,
            device_secret=device_secret,
            installation_id=installation_id,
            model=model,
            fw_version=fw_version,
            pairing_code=pairing_code,
            pairing_code_expires_at=time.time() + 900,
        )
        with self._lock:
            self.devices[device_id] = dev
            self.events[device_id] = {}
            self._device_batch_locks[device_id] = threading.RLock()
        return dev

    # --- challenge ---
    def create_challenge(self, device_id: str, ttl: float = 300.0) -> Challenge:
        ch = Challenge(
            challenge_id=str(uuid.uuid4()),
            device_id=device_id,
            nonce=secrets.token_hex(16),
            expires_at=time.time() + ttl,
        )
        with self._lock:
            self.challenges[ch.challenge_id] = ch
        return ch

    def get_active_challenge(self, challenge_id: str, device_id: str,
                             nonce: str) -> Optional[Challenge]:
        """Returns an active challenge for signature verification, without consuming it."""
        with self._lock:
            ch = self.challenges.get(challenge_id)
            if ch is None or ch.used or ch.expires_at < time.time():
                return None
            if ch.device_id != device_id or not secrets.compare_digest(ch.nonce, nonce):
                return None
            return ch

    def consume_challenge(self, challenge_id: str, device_id: str,
                          nonce: str) -> Optional[Challenge]:
        """Atomically consumes a verified challenge; concurrent replays lose the race."""
        with self._lock:
            ch = self.challenges.get(challenge_id)
            if ch is None or ch.used or ch.expires_at < time.time():
                return None
            if ch.device_id != device_id or not secrets.compare_digest(ch.nonce, nonce):
                return None
            ch.used = True
            return ch

    # --- claim ---
    def pair_device(self, pairing_code: str, parent_id: str, child_id: str) -> Optional[Device]:
        with self._lock:
            for dev in self.devices.values():
                if not secrets.compare_digest(dev.pairing_code, pairing_code):
                    continue
                if dev.pairing_code_expires_at < time.time() or dev.paired_parent_id is not None:
                    return None
                dev.paired_parent_id = parent_id
                dev.paired_child_ids.append(child_id)
                # pairing code is single-use
                dev.pairing_code = ""
                return dev
        return None

    # --- events / ACK ---
    def get_device(self, device_id: str) -> Optional[Device]:
        with self._lock:
            return self.devices.get(device_id)

    def device_batch_lock(self, device_id: str) -> threading.RLock:
        """Returns the stable per-device lock used to serialize complete event batches."""
        with self._lock:
            lock = self._device_batch_locks.get(device_id)
            if lock is None:
                lock = threading.RLock()
                self._device_batch_locks[device_id] = lock
            return lock

    def compute_digest(self, device_id: str, event_id: str, child_id: str,
                       sequence: int, timestamp: int, timestamp_source: str,
                       type_: str, version: int, payload: dict) -> str:
        canonical = json.dumps(payload, sort_keys=True, ensure_ascii=False)
        # A replay is a duplicate only when the complete immutable envelope is
        # unchanged. In particular, an event_id cannot be moved to another
        # child or schema version and still receive success semantics.
        raw = (f"{device_id}|{event_id}|{child_id}|{sequence}|{timestamp}|"
               f"{timestamp_source}|{type_}|{version}|{canonical}")
        return hashlib.sha256(raw.encode("utf-8")).hexdigest()

    def lookup_event(self, device_id: str, event_id: str) -> Optional[StoredEvent]:
        with self._lock:
            return self.events.get(device_id, {}).get(event_id)

    def store_event(self, dev: Device, event_id: str, child_id: str, sequence: int,
                    type_: str, digest: str) -> StoredEvent:
        ev = StoredEvent(
            event_id=event_id,
            device_id=dev.device_id,
            child_id=child_id,
            sequence=sequence,
            type=type_,
            payload_digest=digest,
            received_at=time.time(),
        )
        with self._lock:
            self.events[dev.device_id][event_id] = ev
        return ev

    def child_ids_of(self, device_id: str) -> List[str]:
        with self._lock:
            dev = self.devices.get(device_id)
            return list(dev.paired_child_ids) if dev else []

    def parent_child_ids(self, parent_id: str) -> List[str]:
        with self._lock:
            p = self.parents.get(parent_id)
            return list(p.child_ids) if p else []

    def parent_id_for_token(self, parent_token: str) -> Optional[str]:
        with self._lock:
            for parent in self.parents.values():
                if secrets.compare_digest(parent.token, parent_token):
                    return parent.parent_id
            return None

    def tasks_for_child(self, child_id: str) -> List[dict]:
        with self._lock:
            return list(self.tasks_by_child.get(child_id, []))

    def revoke_child_binding(self, device_id: str, child_id: str) -> bool:
        """Mock administration helper used to verify per-request binding revalidation."""
        with self._lock:
            dev = self.devices.get(device_id)
            if dev is None or child_id not in dev.paired_child_ids:
                return False
            dev.paired_child_ids.remove(child_id)
            return True
