"""Optional credential file for the isolated family backend."""

import json
import os
from pathlib import Path
import re


_HEX_SECRET = re.compile(r"[0-9a-f]{64}")


def load_credentials(path: str | None) -> tuple[str, str, str]:
    if path is None:
        return "wb-stream-001-mock-only-secret", "mock-parent-token-1", "mock-parent-token-2"
    try:
        with Path(path).open("rb") as stream:
            raw = stream.read(4097)
        if len(raw) > 4096:
            raise ValueError
        values = json.loads(raw)
        if not isinstance(values, dict) or set(values) != {"device_token_key", "parent_1", "parent_2"}:
            raise ValueError
        keys = (values["device_token_key"], values["parent_1"], values["parent_2"])
        if any(not isinstance(key, str) or _HEX_SECRET.fullmatch(key) is None for key in keys):
            raise ValueError
        if len(set(keys)) != len(keys):
            raise ValueError
        return keys
    except (OSError, ValueError, RecursionError):
        raise RuntimeError("Invalid Claw4 backend credential file") from None


DEVICE_TOKEN_KEY, PARENT_1_TOKEN, PARENT_2_TOKEN = load_credentials(
    os.environ.get("CLAW4_BACKEND_CREDENTIAL_FILE")
)
