#!/usr/bin/env python3
"""Wire exact TTS recapture detection into the pinned NAS audio paths."""

import ast
import hashlib
from pathlib import Path
import sys

SENDER = "core/handle/sendAudioHandle.py"
ASR = "core/providers/asr/base.py"
HASHES = {
    SENDER: "6b610cc04bf05a75964f5fe668bfda7fd17b087ef8f2a938e1bc8ed9b0c52387",
    ASR: "293766383297444462464846dd6861053e7e9606454368febd778147df79c179",
}
REPLACEMENTS = {
    SENDER: (
        (
            "from core.utils.audioRateController import AudioRateController\n",
            "from core.utils.audioRateController import AudioRateController\n"
            "from core.utils.claw4_echo_guard import record_text, record_packet, finish_text\n",
        ),
        (
            "        await conn.websocket.send(opus_packet)\n",
            "        await conn.websocket.send(opus_packet)\n"
            "        record_packet(conn, flow_control.get(\"sentence_id\"))\n",
        ),
        (
            "    # 发送消息到客户端\n    await conn.websocket.send(json.dumps(message))\n",
            "    # 发送消息到客户端\n"
            "    sentence_id = conn.sentence_id\n"
            "    await conn.websocket.send(json.dumps(message))\n"
            "    if not conn.conn_from_mqtt_gateway:\n"
            "        if state == \"sentence_start\":\n"
            "            record_text(conn, sentence_id, message.get(\"text\"))\n"
            "        elif state == \"stop\":\n"
            "            finish_text(conn, sentence_id)\n",
        ),
    ),
    ASR: (
        (
            "from core.handle.reportHandle import enqueue_asr_report\n",
            "from core.handle.reportHandle import enqueue_asr_report\n"
            "from core.utils.claw4_echo_guard import snapshot_echoes, matches_echo\n",
        ),
        (
            "            # 预先准备WAV数据\n",
            "            echo_snapshot = snapshot_echoes(conn, total_start_time, len(combined_pcm_data))\n\n"
            "            # 预先准备WAV数据\n",
        ),
        (
            "            if text_len > 0:\n                audio_snapshot = asr_audio_task.copy()\n",
            "            if text_len > 0:\n"
            "                if matches_echo(content_for_length_check, echo_snapshot):\n"
            "                    logger.bind(tag=TAG).info(\"VOICE_ECHO_DROPPED\")\n"
            "                    return\n"
            "                audio_snapshot = asr_audio_task.copy()\n",
        ),
    ),
}


def patch(root):
    pending = {}
    for relative, edits in REPLACEMENTS.items():
        path = root / relative
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != HASHES[relative]:
            raise ValueError(f"echo guard source SHA-256 mismatch: {relative}")
        source = data.decode("utf-8")
        for before, after in edits:
            if source.count(before) != 1:
                raise ValueError(f"echo guard source anchor mismatch: {relative}")
            source = source.replace(before, after, 1)
        ast.parse(source)
        pending[path] = source.encode("utf-8")
    for path, data in pending.items():
        path.write_bytes(data)


if __name__ == "__main__":
    patch(Path(sys.argv[1]))
