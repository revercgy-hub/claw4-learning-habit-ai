# claw4/backend/app/security.py
# HMAC signing, short-lived device token and challenge/nonce verification.
"""Security helpers for the contract mock backend.

Implements the ARCHITECTURE.md §6.4 contract: per-device secret, two-phase
challenge (challenge_id + nonce + expires_at), HMAC-SHA256 signature bound to
device_id/challenge_id/nonce, single-use nonce, and short-lived device tokens
that carry the allowed device_id + child_id bindings.

Token format (self-contained, mock-grade): base64url(claims) "." base64url(HMAC).
No third-party JWT library is required.
"""

from __future__ import annotations

import base64
import hashlib
import hmac
import json
import secrets
import time

from app.store import Store

# Mock-only static secret used to sign device tokens (never a production key).
MOCK_TOKEN_SECRET = "wb-stream-001-mock-only-secret"
TOKEN_TTL_SECONDS = 24 * 3600


def _b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def _unb64url(s: str) -> bytes:
    pad = "=" * (-len(s) % 4)
    return base64.urlsafe_b64decode((s + pad).encode("ascii"))


def sign_challenge(device_secret: str, device_id: str, challenge_id: str,
                   nonce: str) -> str:
    """challenge_signature = base64(HMAC-SHA256(device_secret, device_id|challenge_id|nonce)).

    ARCHITECTURE.md §6.4.3: the signature input is bound to device_id,
    challenge_id and nonce to prevent cross-device / cross-challenge reuse.
    """
    message = f"{device_id}|{challenge_id}|{nonce}".encode("utf-8")
    mac = hmac.new(device_secret.encode("utf-8"), message, hashlib.sha256).digest()
    return base64.b64encode(mac).decode("ascii")


def verify_challenge_signature(device_secret: str, device_id: str, challenge_id: str,
                               nonce: str, provided: str) -> bool:
    expected = sign_challenge(device_secret, device_id, challenge_id, nonce)
    return hmac.compare_digest(expected, provided)


def issue_device_token(device_id: str, child_ids: list[str]) -> str:
    """Issues a short-lived device token carrying device_id + child_id bindings.

    ARCHITECTURE.md §6.4.1: the token MUST carry the allowed device_id and
    child_id set; every request re-validates the binding.
    """
    now = int(time.time())
    claims = {
        "sub": device_id,
        "child_ids": child_ids,
        "iat": now,
        "exp": now + TOKEN_TTL_SECONDS,
        "scope": "device",
    }
    body = _b64url(json.dumps(claims, separators=(",", ":")).encode("utf-8"))
    mac = hmac.new(MOCK_TOKEN_SECRET.encode("utf-8"), body.encode("ascii"),
                   hashlib.sha256).digest()
    return f"{body}.{_b64url(mac)}"


def decode_device_token(token: str) -> dict:
    """Returns claims; raises ValueError when malformed, expired or tampered."""
    try:
        body, sig = token.split(".", 1)
        expected = _b64url(hmac.new(MOCK_TOKEN_SECRET.encode("utf-8"),
                                    body.encode("ascii"),
                                    hashlib.sha256).digest())
        if not hmac.compare_digest(expected, sig):
            raise ValueError("bad signature")
        claims = json.loads(_unb64url(body))
        if int(claims.get("exp", 0)) < int(time.time()):
            raise ValueError("token expired")
        if claims.get("scope") != "device":
            raise ValueError("bad scope")
        return claims
    except (ValueError, KeyError, json.JSONDecodeError) as exc:
        raise ValueError("invalid token") from exc


def parent_token_ok(parent_token: str, store: Store) -> str | None:
    """Resolves a parent bearer token to a parent_id, or None (mock auth)."""
    return store.parent_id_for_token(parent_token)


def generate_nonce() -> str:
    return secrets.token_hex(16)
