"""Run a classified final-utterance command on the current paired device."""

import time

from core.utils.claw4_learning_exchange import new_command, verify_result
from core.utils.claw4_learning_voice import reply
from core.utils.claw4_timer_bridge import call_device_tool


TOOL = "self.claw4.learning"
ALIAS = "self_claw4_learning"


def resolve_learning_reply(conn, kind, sentence_id, paired_session):
    """The connection owner supplies its current trusted pairing, if any."""
    if conn.sentence_id != sentence_id or conn.client_abort or conn.stop_event.is_set():
        return None
    command = new_command(kind, paired_session, time.time_ns() // 1_000_000)
    if command is None:
        return reply(kind, None, None)
    response = call_device_tool(conn, command.arguments(paired_session), sentence_id, TOOL, ALIAS)
    if conn.sentence_id != sentence_id or conn.client_abort or conn.stop_event.is_set():
        return None
    verified = verify_result(command, paired_session, response, time.time_ns() // 1_000_000)
    return reply(kind, command.command_id, verified)
