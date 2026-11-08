#!/usr/bin/env python3
"""Add the opt-in device timer request path to the live lookup image."""

import ast
import hashlib
from pathlib import Path
import sys

LIVE_LOOKUP_CONNECTION_SHA256 = "68e41c49456331c7435eef0c808322d9fedbffbe52705addf9db6ca656f99441"
REPLACEMENTS = (
    (
        "from core.utils.claw4_live_lookup import (",
        "from core.utils.claw4_timer_bridge import resolve_timer_reply\n"
        "from core.utils.claw4_timer_voice import classify as classify_timer\n"
        "from core.utils.claw4_live_lookup import (",
    ),
    (
        "        live_request = classify_live_lookup(query) if depth == 0 else None\n"
        "        if query is not None and live_request is None:",
        "        timer_request = classify_timer(query) if depth == 0 else None\n"
        "        live_request = classify_live_lookup(query) if depth == 0 and timer_request is None else None\n"
        "        if query is not None and live_request is None and timer_request is None:",
    ),
    (
        "        if live_request is not None:\n",
        "        if live_request is not None or timer_request is not None:\n",
    ),
    (
        "            reply = answer_live_lookup(live_request)\n",
        "            if timer_request is not None:\n"
        "                reply = resolve_timer_reply(self, timer_request, current_sentence_id)\n"
        "            else:\n"
        "                reply = answer_live_lookup(live_request)\n",
    ),
)


def patch(path):
    original = path.read_bytes()
    if hashlib.sha256(original).hexdigest() != LIVE_LOOKUP_CONNECTION_SHA256:
        raise ValueError("live lookup connection source SHA-256 mismatch")
    source = original.decode("utf-8")
    for before, after in REPLACEMENTS:
        if source.count(before) != 1:
            raise ValueError("timer preview connection anchor mismatch")
        source = source.replace(before, after, 1)
    ast.parse(source)
    path.write_bytes(source.encode("utf-8"))


if __name__ == "__main__":
    patch(Path(sys.argv[1]))
