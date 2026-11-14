"""Send a bounded MCP call on the current device connection."""

import asyncio
import json

from core.utils.claw4_timer_voice import reply

TOOL = "self.claw4.timer_preview"
ALIAS = "self_claw4_timer_preview"


async def _exchange(conn, request, sentence_id, tool=TOOL, alias=ALIAS):
    from core.providers.tools.device_mcp import send_mcp_message

    client = getattr(conn, "mcp_client", None)
    if client is None or not await client.is_ready() or client.name_mapping.get(alias) != tool:
        return None
    if conn.client_abort or conn.stop_event.is_set() or conn.sentence_id != sentence_id:
        return None
    call_id = await client.get_next_id()
    result = asyncio.get_running_loop().create_future()
    await client.register_call_result_future(call_id, result)
    try:
        await send_mcp_message(conn, {
            "jsonrpc": "2.0",
            "id": call_id,
            "method": "tools/call",
            "params": {"name": tool, "arguments": request},
        })
        payload = await result
        if not isinstance(payload, dict) or payload.get("isError") is not False:
            return None
        content = payload.get("content")
        if not isinstance(content, list) or len(content) != 1 or not isinstance(content[0], dict):
            return None
        text = content[0].get("text")
        if content[0].get("type") != "text" or not isinstance(text, str) or len(text) > 512:
            return None
        return json.loads(text)
    finally:
        await client.cleanup_call_result(call_id)


def resolve_timer_reply(conn, request, sentence_id):
    if "clarify" in request:
        return reply(request, None)
    return reply(request, call_device_tool(conn, request, sentence_id, TOOL, ALIAS))


def call_device_tool(conn, request, sentence_id, tool, alias):
    exchange = _exchange_with_deadline(conn, request, sentence_id, tool, alias)
    try:
        future = asyncio.run_coroutine_threadsafe(exchange, conn.loop)
    except RuntimeError:
        exchange.close()
        return None
    try:
        return future.result(timeout=3)
    except Exception:
        future.cancel()
        return None


async def _exchange_with_deadline(conn, request, sentence_id, tool=TOOL, alias=ALIAS):
    return await asyncio.wait_for(_exchange(conn, request, sentence_id, tool, alias), timeout=2.5)
