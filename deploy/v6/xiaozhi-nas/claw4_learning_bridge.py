"""Run a classified final-utterance command on the current paired device."""

import time

from core.utils.claw4_learning_exchange import new_command, session_challenge, verify_result, verify_session
from core.utils.claw4_learning_voice import reply
from core.utils.claw4_timer_bridge import call_device_tool


TOOL = "self.claw4.learning"
ALIAS = "self_claw4_learning"
SESSION_TOOL = "self.claw4.learning_session"
SESSION_ALIAS = "self_claw4_learning_session"


def resolve_learning_reply(conn, kind, sentence_id, resolve_pairing):
    """The connection owner supplies its current immutable pairing lease."""
    if conn.sentence_id != sentence_id or conn.client_abort or conn.stop_event.is_set():
        return None
    record = resolve_pairing()
    if record is None:
        return reply(kind, None, None)
    challenge = session_challenge()
    result = call_device_tool(conn, {"challenge": challenge}, sentence_id, SESSION_TOOL, SESSION_ALIAS)
    if conn.sentence_id != sentence_id or conn.client_abort or conn.stop_event.is_set():
        return None
    paired_session = (verify_session(record, conn.session_id, challenge, result)
                      if resolve_pairing() is record else None)
    command = new_command(kind, paired_session, time.time_ns() // 1_000_000)
    if command is None:
        return reply(kind, None, None)
    response = call_device_tool(conn, command.arguments(paired_session), sentence_id, TOOL, ALIAS)
    if conn.sentence_id != sentence_id or conn.client_abort or conn.stop_event.is_set():
        return None
    if resolve_pairing() is not record:
        return reply(kind, command.command_id, None)
    verified = verify_result(command, paired_session, response, time.time_ns() // 1_000_000)
    return reply(kind, command.command_id, verified)
