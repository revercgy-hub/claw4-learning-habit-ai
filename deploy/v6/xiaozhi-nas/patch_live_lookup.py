#!/usr/bin/env python3
"""Add the private public-data reply path to the reviewed prewarm image."""

import ast
import hashlib
from pathlib import Path
import sys

PREWARM_CONNECTION_SHA256 = "9eee754ea95cb47be55c8e67d579609999d7d67fa91b4cc197a70233d4ef0633"

IMPORT = "from core.providers.tts.dto.dto import ContentType, TTSMessageDTO, SentenceType\n"
LOG = '        if query is not None:\n            self.logger.bind(tag=TAG).info(f"大模型收到用户消息: {query}")\n'
BEFORE_LLM = "        # 设置最大递归深度，避免无限循环，可根据实际需求调整\n"

LOOKUP = '''        if live_request is not None:
            if self.client_abort or self.stop_event.is_set():
                return True
            reply = answer_live_lookup(live_request)
            if (
                self.client_abort
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
            self.dialogue.put(Message(role="assistant", content=reply))
            self.tts.tts_text_queue.put(
                TTSMessageDTO(
                    sentence_id=current_sentence_id,
                    sentence_type=SentenceType.LAST,
                    content_type=ContentType.ACTION,
                )
            )
            return True

'''


def patch(path: Path) -> None:
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != PREWARM_CONNECTION_SHA256:
        raise ValueError("prewarm connection source SHA-256 mismatch")
    source = data.decode("utf-8")
    replacements = (
        (
            IMPORT,
            IMPORT + "from core.utils.claw4_live_lookup import (\n"
            "    answer as answer_live_lookup, classify as classify_live_lookup,\n"
            ")\n",
        ),
        (
            LOG,
            "        live_request = classify_live_lookup(query) if depth == 0 else None\n"
            + LOG.replace("if query is not None:", "if query is not None and live_request is None:"),
        ),
        (BEFORE_LLM, LOOKUP + BEFORE_LLM),
    )
    for anchor, replacement in replacements:
        if source.count(anchor) != 1:
            raise ValueError("live lookup connection anchor mismatch")
        source = source.replace(anchor, replacement, 1)
    ast.parse(source)
    path.write_bytes(source.encode("utf-8"))


if __name__ == "__main__":
    patch(Path(sys.argv[1]))
