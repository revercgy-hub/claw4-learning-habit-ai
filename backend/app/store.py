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
    """Thread-safe-by-convention in-memory store (single process, MVP mock)."""

    def __init__(self) -> None:
        self.devices: Dict[str, Device] = {}
        self.challenges: Dict[str, Challenge] = {}
        # event registry: device_id -> event_id -> StoredEvent
        self.events: Dict[str, Dict[str, StoredEvent]] = {}
        self.parents: Dict[str, Parent] = {}
        self.tasks_by_child: Dict[str, list] = {}

    # --- seed data (mock only, no real child accounts) ---
    def seed(self, parent: Parent, tasks_by_child: Dict[str, list]) -> None:
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
        self.devices[device_id] = dev
        self.events[device_id] = {}
        return dev

    # --- challenge ---
    def create_challenge(self, device_id: str, ttl: float = 300.0) -> Challenge:
        ch = Challenge(
            challenge_id=str(uuid.uuid4()),
            device_id=device_id,
            nonce=secrets.token_hex(16),
            expires_at=time.time() + ttl,
        )
        self.challenges[ch.challenge_id] = ch
        return ch

    def consume_challenge(self, challenge_id: str) -> Optional[Challenge]:
        ch = self.challenges.get(challenge_id)
        if ch is None or ch.used or ch.expires_at < time.time():
            return None
        ch.used = True
        return ch

    # --- claim ---
    def pair_device(self, pairing_code: str, parent_id: str, child_id: str) -> Optional[Device]:
        for dev in self.devices.values():
            if dev.pairing_code != pairing_code:
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
        return self.devices.get(device_id)

    def compute_digest(self, device_id: str, event_id: str, sequence: int,
                       type_: str, payload: dict) -> str:
        canonical = json.dumps(payload, sort_keys=True, ensure_ascii=False)
        raw = f"{device_id}|{event_id}|{sequence}|{type_}|{canonical}"
        return hashlib.sha256(raw.encode("utf-8")).hexdigest()

    def lookup_event(self, device_id: str, event_id: str) -> Optional[StoredEvent]:
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
        self.events[dev.device_id][event_id] = ev
        return ev

    def child_ids_of(self, device_id: str) -> List[str]:
        dev = self.devices.get(device_id)
        return list(dev.paired_child_ids) if dev else []

    def parent_child_ids(self, parent_id: str) -> List[str]:
        p = self.parents.get(parent_id)
        return list(p.child_ids) if p else []
