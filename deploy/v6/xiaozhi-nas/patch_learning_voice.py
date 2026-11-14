#!/usr/bin/env python3
"""Install the opt-in learning branch in the pinned XiaoZhi chat entry."""

import ast
import hashlib
from pathlib import Path
import sys


CONNECTION_SHA256 = "c8e8e3df56e6cff98ddcb6647a96b039e4d45dc001731eba16a3e5878f315dc5"
RECEIVER_SHA256 = "4493dfd77c1bfa7be1a79bb293dbdbf8cad64a6eb2bc53efca3e30efefcb47f1"
ASR_SHA256 = "af8c17ebc6919582853f8115e4f9ef6de03882db42c341f1fc7965795c62aaf5"
IMPORT = "from core.utils.claw4_timer_bridge import resolve_timer_reply\n"
TIMER = "        timer_request = classify_timer(query) if depth == 0 else None\n"
LIVE = "        live_request = classify_live_lookup(query) if depth == 0 and timer_request is None else None\n"
USER_HISTORY = '            self.dialogue.put(Message(role="user", content=query))\n'
BEFORE_FIXED = "        if live_request is not None or timer_request is not None:\n"
LEARNING_BRANCH = '''        if learning_request is not None:
            reply = resolve_learning_reply(self, learning_request, current_sentence_id, resolve_pairing)
            if (
                reply is None
                or self.client_abort
                or self.stop_event.is_set()
                or self.sentence_id != current_sentence_id
            ):
                return True
            self.tts.tts_text_queue.put(
                TTSMessageDTO(
                    sentence_id=current_sentence_id,
                    sentence_type=SentenceType.MIDDLE,
                    content_type=ContentType.TEXT,
                    content_detail=reply,
                )
            )
            self.tts.store_tts_text(current_sentence_id, reply)
            self.tts.tts_text_queue.put(
                TTSMessageDTO(
                    sentence_id=current_sentence_id,
                    sentence_type=SentenceType.LAST,
                    content_type=ContentType.ACTION,
                )
            )
            return True

'''


def _render(path, digest, replacements):
    original = path.read_bytes()
    if hashlib.sha256(original).hexdigest() != digest:
        raise ValueError(f"learning voice source SHA-256 mismatch: {path.name}")
    source = original.decode("utf-8")
    for before, after in replacements:
        if source.count(before) != 1:
            raise ValueError(f"learning voice source anchor mismatch: {path.name}")
        source = source.replace(before, after, 1)
    ast.parse(source)
    return source.encode("utf-8")


def patch(root):
    connection = root / "core/connection.py"
    receiver = root / "core/handle/receiveAudioHandle.py"
    asr = root / "core/providers/asr/base.py"
    replacements = (
        ("            self.websocket = ws\n",
         "            self.websocket = ws\n"
         "            self.resolve_claw4_learning_pairing = pairing_resolver(self)\n"),
        ("    def chat(self, query, depth=0):\n",
         "    def chat(self, query, depth=0, *, asr_final=False):\n"),
        (IMPORT, IMPORT + "from core.utils.claw4_learning_bridge import resolve_learning_reply\n"
         "from core.utils.claw4_learning_voice import classify as classify_learning\n"
         "from core.utils.claw4_learning_pairing import pairing_resolver\n"),
        (TIMER,
         '        resolve_pairing = getattr(self, "resolve_claw4_learning_pairing", None)\n'
         '        learning_request = classify_learning(query) if depth == 0 and asr_final and callable(resolve_pairing) else None\n'
         '        timer_request = classify_timer(query) if depth == 0 and learning_request is None else None\n'),
        (LIVE, "        live_request = (classify_live_lookup(query)\n"
               "                        if depth == 0 and learning_request is None and timer_request is None else None)\n"),
        ("        if query is not None and live_request is None and timer_request is None:\n",
         "        if query is not None and live_request is None and timer_request is None and learning_request is None:\n"),
        (USER_HISTORY, "            if learning_request is None:\n    " + USER_HISTORY),
        (BEFORE_FIXED, LEARNING_BRANCH + BEFORE_FIXED),
    )
    receiver_edits = (
        ("async def startToChat(conn: \"ConnectionHandler\", text):\n",
         "async def startToChat(conn: \"ConnectionHandler\", text, *, asr_final=False):\n"),
        ("    conn.executor.submit(conn.chat, actual_text)\n",
         "    conn.executor.submit(conn.chat, actual_text, asr_final=asr_final)\n"),
    )
    asr_call = (
        "                await startToChat(\n"
        "                    conn, _chat_text_for_known_sensevoice(self, raw_text, enhanced_text)\n"
        "                )\n"
    )
    pending = {
        connection: _render(connection, CONNECTION_SHA256, replacements),
        receiver: _render(receiver, RECEIVER_SHA256, receiver_edits),
        asr: _render(asr, ASR_SHA256, (
            (asr_call, asr_call.replace("enhanced_text)\n", "enhanced_text), asr_final=True\n")),
        )),
    }
    for path, data in pending.items():
        path.write_bytes(data)


if __name__ == "__main__":
    patch(Path(sys.argv[1]))
