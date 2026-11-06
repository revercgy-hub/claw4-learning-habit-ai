#!/usr/bin/env python3
"""Apply the reviewed private connection-time preload hook to XiaoZhi v0.9.6."""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys

EXPECTED_SHA256 = "b1365d1445a0f3c3fe4fe8ef6a22f3d76b8a2ae7098cd57c97869fcb9d05b53d"
ANCHOR = (
    "            if self.need_bind:\n"
    "                self.bind_completed_event.set()\n"
    "                return\n"
    "            self.selected_module_str = build_module_string(\n"
)
REPLACEMENT = (
    "            if self.need_bind:\n"
    "                self.bind_completed_event.set()\n"
    "                return\n"
    "            try:\n"
    "                from core.utils.claw4_ollama_prewarm import schedule_ollama_prewarm\n"
    "                schedule_ollama_prewarm(self.llm)\n"
    "            except Exception:\n"
    "                pass  # Preload never aborts component initialization.\n"
    "            self.selected_module_str = build_module_string(\n"
)


def patch(path: Path) -> None:
    original = path.read_bytes()
    if hashlib.sha256(original).hexdigest() != EXPECTED_SHA256:
        raise ValueError("connection source SHA-256 mismatch")
    source = original.decode("utf-8")
    newline = "\r\n" if b"\r\n" in original else "\n"
    normalized = source.replace("\r\n", "\n")
    if normalized.count(ANCHOR) != 1:
        raise ValueError("connection anchor mismatch")
    patched = normalized.replace(ANCHOR, REPLACEMENT, 1)
    path.write_bytes(patched.replace("\n", newline).encode("utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("connection", type=Path)
    args = parser.parse_args()
    try:
        patch(args.connection)
    except (OSError, UnicodeError, ValueError) as exc:
        print(f"Prewarm patch failed: {type(exc).__name__}", file=sys.stderr)
        return 1
    print("Prewarm patch applied")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
