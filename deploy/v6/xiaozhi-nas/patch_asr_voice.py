#!/usr/bin/env python3
"""Apply the bounded, source-hash-guarded XiaoZhi v0.9.6 ASR chat fix."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys


EXPECTED_SHA256 = "929532c379d18598eff6fbecb991089f25a0c62cd77f74a25a0d1a5b8d13a713"

HELPER_ANCHOR = "class ASRProviderBase(ABC):\n"
HELPER = '''def _chat_text_for_known_sensevoice(provider, raw_text, enhanced_text):
    """Keep the reporting envelope; unwrap only the known local ASR chat input."""
    if (
        type(provider).__module__ == "core.providers.asr.fun_local"
        and type(raw_text) is dict
        and raw_text.keys() == {"content", "language", "emotion"}
        and type(raw_text["content"]) is str
        and 0 < len(raw_text["content"]) <= 4096
        and type(raw_text["language"]) is str
        and type(raw_text["emotion"]) is str
    ):
        return raw_text["content"]
    return enhanced_text


'''
CHAT_ANCHOR = "                await startToChat(conn, enhanced_text)\n"
CHAT_REPLACEMENT = (
    "                await startToChat(\n"
    "                    conn, _chat_text_for_known_sensevoice(self, raw_text, enhanced_text)\n"
    "                )\n"
)


def patch(path: Path) -> None:
    original = path.read_bytes()
    actual_sha256 = hashlib.sha256(original).hexdigest()
    if actual_sha256 != EXPECTED_SHA256:
        raise ValueError(
            f"refusing ASR source: SHA-256 {actual_sha256} "
            f"does not match expected {EXPECTED_SHA256}"
        )

    source = original.decode("utf-8")
    newline = "\r\n" if b"\r\n" in original else "\n"
    source = source.replace("\r\n", "\n")
    for anchor in (HELPER_ANCHOR, CHAT_ANCHOR):
        count = source.count(anchor)
        if count != 1:
            raise ValueError(
                f"refusing unexpected ASR layout: anchor count {count}, expected 1"
            )
    patched = source.replace(HELPER_ANCHOR, HELPER + HELPER_ANCHOR, 1)
    patched = patched.replace(CHAT_ANCHOR, CHAT_REPLACEMENT, 1)
    path.write_bytes(patched.replace("\n", newline).encode("utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("asr_base", type=Path)
    args = parser.parse_args()
    try:
        patch(args.asr_base)
    except (OSError, UnicodeError, ValueError) as exc:
        print(f"ASR patch failed: {exc}", file=sys.stderr)
        return 1
    print(f"Patched {args.asr_base}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
