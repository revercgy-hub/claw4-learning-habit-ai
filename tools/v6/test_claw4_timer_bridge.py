import asyncio
import importlib.util
from pathlib import Path
import sys
from threading import Event
from types import ModuleType, SimpleNamespace
import unittest
from unittest.mock import patch

BRIDGE = Path(__file__).resolve().parents[2] / "deploy/v6/xiaozhi-nas/claw4_timer_bridge.py"
MODULE = "core.providers.tools.device_mcp"


class PreviewBridgeTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        voice = ModuleType("core.utils.claw4_timer_voice")
        voice.reply = lambda request, result: str(result)
        wire = ModuleType(MODULE)
        wire.send_mcp_message = self.send
        modules = patch.dict(sys.modules, {voice.__name__: voice, MODULE: wire})
        self.addCleanup(modules.stop)
        modules.start()
        spec = importlib.util.spec_from_file_location("timer_bridge", BRIDGE)
        self.bridge = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.bridge)
        self.sent = []
        self.pending = {}
        self.response = {"isError": False, "content": [{"type": "text", "text": '{"status":"ok"}'}]}
        self.reply_enabled = True
        self.client = SimpleNamespace(
            is_ready=self.is_ready,
            name_mapping={self.bridge.ALIAS: self.bridge.TOOL},
            get_next_id=self.get_next_id,
            register_call_result_future=self.register,
            cleanup_call_result=self.cleanup,
        )
        self.conn = SimpleNamespace(
            mcp_client=self.client, client_abort=False, stop_event=Event(), sentence_id="current",
        )

    async def is_ready(self):
        return True

    async def get_next_id(self):
        return 29

    async def register(self, call_id, future):
        self.pending[call_id] = future

    async def cleanup(self, call_id):
        self.pending.pop(call_id, None)

    async def send(self, conn, payload):
        self.assertIs(conn, self.conn)
        self.sent.append(payload)
        if self.reply_enabled:
            self.pending[payload["id"]].set_result(self.response)

    async def test_only_the_advertised_tool_receives_the_structured_request(self):
        request = {"action": 0, "minutes": 25}
        self.assertEqual(await self.bridge._exchange(self.conn, request, "current"), {"status": "ok"})
        self.assertEqual(self.sent[0]["method"], "tools/call")
        self.assertEqual(self.sent[0]["params"], {"name": self.bridge.TOOL, "arguments": request})
        self.assertFalse(self.pending)

    async def test_unsupported_and_superseded_turns_do_not_send(self):
        self.assertIsNone(await self.bridge._exchange(self.conn, {"action": 6}, "prior"))
        self.client.name_mapping = {self.bridge.ALIAS: "some.other.tool"}
        self.assertIsNone(await self.bridge._exchange(self.conn, {"action": 6}, "current"))
        self.assertEqual(self.sent, [])

    async def test_tool_error_cannot_confirm_success(self):
        self.response["isError"] = True
        self.assertIsNone(await self.bridge._exchange(self.conn, {"action": 4}, "current"))
        self.assertFalse(self.pending)

    async def test_cancelled_exchange_cleans_pending_response(self):
        self.reply_enabled = False
        with self.assertRaises(asyncio.TimeoutError):
            await asyncio.wait_for(self.bridge._exchange(self.conn, {"action": 6}, "current"), 0.02)
        self.assertFalse(self.pending)
