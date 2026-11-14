import importlib.util
import json
import sys
from pathlib import Path
from tempfile import TemporaryDirectory
from threading import Event
from types import ModuleType, SimpleNamespace
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location(
    "claw4_learning_voice",
    Path(__file__).resolve().parents[2] / "deploy/v6/xiaozhi-nas/claw4_learning_voice.py",
)
voice = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(voice)
EXCHANGE_SPEC = importlib.util.spec_from_file_location(
    "claw4_learning_exchange", Path(SPEC.origin).with_name("claw4_learning_exchange.py"),
)
exchange = importlib.util.module_from_spec(EXCHANGE_SPEC)
sys.modules[EXCHANGE_SPEC.name] = exchange
EXCHANGE_SPEC.loader.exec_module(exchange)


class LearningVoiceTests(unittest.TestCase):
    def test_all_seven_explicit_commands(self):
        cases = {
            "请查看今天的任务。": "QUERY_TODAY_TASKS",
            "开始学习任务": "START_TASK",
            "暂停当前任务": "PAUSE_TASK",
            "继续学习任务": "RESUME_TASK",
            "完成当前任务": "COMPLETE_REQUEST",
            "当前任务还剩多久？": "QUERY_REMAINING_TIME",
            "延后学习提醒": "SNOOZE_REMINDER",
        }
        for query, kind in cases.items():
            with self.subTest(query=query):
                self.assertEqual(voice.classify(query), kind)

    def test_other_utterances_stay_out_of_learning(self):
        for query in ("暂停番茄钟", "继续", "不要完成当前任务", "怎么开始学习任务",
                      "一加一等于几？", "查看今天的新闻", "开始学习任务然后结束", "延后闹钟"):
            with self.subTest(query=query):
                self.assertIsNone(voice.classify(query))

    def test_ignores_unbound_results_and_refuses_completion_success(self):
        for response in (None, {"command_id": "old", "status": "APPLIED"},
                         {"command_id": "new", "status": ["APPLIED"]}):
            self.assertIn("还没有收到设备确认", voice.reply("START_TASK", "new", response))
        self.assertIn("还没有收到设备确认", voice.reply(
            "COMPLETE_REQUEST", "new", {"command_id": "new", "status": "APPLIED"}))
        self.assertIn("点确认", voice.reply(
            "COMPLETE_REQUEST", "new", {"command_id": "new", "status": "CONFIRMATION_REQUIRED"}))

    def test_fixed_failure_and_checked_query_result(self):
        self.assertIn("没有保存", voice.reply(
            "START_TASK", "new", {"command_id": "new", "status": "STORAGE_ERROR"}))
        result = {"command_id": "new", "status": "APPLIED", "remaining_seconds": 65}
        self.assertEqual(voice.reply("QUERY_REMAINING_TIME", "new", result), "学习任务还剩1分5秒。")
        result["remaining_seconds"] = True
        self.assertIn("还没有收到设备确认", voice.reply("QUERY_REMAINING_TIME", "new", result))
        result.update(task_count=2, snapshot_epoch_ms=1_750_000_000_000)
        self.assertIn("上次同步的今日任务有2项", voice.reply("QUERY_TODAY_TASKS", "new", result))
        del result["snapshot_epoch_ms"]
        self.assertIn("还没有收到设备确认", voice.reply("QUERY_TODAY_TASKS", "new", result))


class LearningExchangeTests(unittest.TestCase):
    def setUp(self):
        self.now = 1756718530000
        self.session = exchange.PairedLearningSession(
            "dev-test", "child-test", 42, b"synthetic-tests-only-0123456789ab",
        )

    def test_shared_firmware_vector(self):
        command = exchange.LearningCommand("test-command", "START_TASK", 42, self.now + 30_000)
        self.assertEqual(command.arguments(self.session)["mac"],
                         "bSRk9yLJn86DkrYFg7MfntB+Ljcger4362hEMfP18Bw=")
        result = {"command_id": "test-command", "status": "APPLIED",
                  "mac": "5fCWQLY1rW6vym+iUCKYoZlxR+OD2PnN6KHSQVV+lkI="}
        self.assertEqual(voice.reply(command.kind, command.command_id,
                                     exchange.verify_result(command, self.session, result, self.now)),
                         "学习任务已开始。")
        for change in ({"status": "BUSY"}, {"command_id": "other"}, {"value": 1}, {"mac": "x" * 44}):
            with self.subTest(change=change):
                self.assertIsNone(exchange.verify_result(command, self.session,
                                                         result | change, self.now))
        unpaired = exchange.PairedLearningSession("dev-test", "wrong-child", 42, self.session.key)
        self.assertIsNone(exchange.verify_result(command, unpaired, result, self.now))
        self.assertIsNone(exchange.verify_result(command, self.session, result, command.expires_at_ms))

    def test_signs_only_recognized_commands_with_explicit_pairing(self):
        self.assertIsNone(exchange.new_command("START_TASK", None, self.now))
        weak = exchange.PairedLearningSession("dev-test", "child-test", 42, b"test")
        self.assertIsNone(exchange.new_command("START_TASK", weak, self.now))
        self.assertIsNone(exchange.new_command("COMPLETE", self.session, self.now))
        command = exchange.new_command("COMPLETE_REQUEST", self.session, self.now)
        self.assertEqual(command.expires_at_ms, self.now + 30_000)
        fields = command.arguments(self.session)
        self.assertEqual(set(fields), {"command_id", "kind", "session_epoch", "expires_at_ms", "mac"})
        self.assertNotIn(self.session.key.decode(), repr(self.session))

    def test_bridge_uses_current_turn_and_signed_device_reply(self):
        connection = SimpleNamespace(sentence_id="current", client_abort=False, stop_event=Event(),
                                     session_id="12345678-1234-4123-8123-123456789abc")
        calls = []

        def call_device_tool(conn, request, turn, tool, alias):
            calls.append((conn, request, turn, tool, alias))
            if tool == "self.claw4.learning_session":
                return {
                    "session_epoch": "42",
                    "mac": exchange._mac(self.session.key, "claw4-learning-session-v1",
                                         "dev-test", "child-test", conn.session_id,
                                         request["challenge"], "42"),
                }
            fields = ("dev-test", "child-test", "42", request["command_id"],
                      request["kind"], request["expires_at_ms"])
            return {
                "command_id": request["command_id"],
                "status": "CONFIRMATION_REQUIRED",
                "mac": exchange._mac(self.session.key, "claw4-learning-result-v1",
                                     *fields, "CONFIRMATION_REQUIRED", "-", "-"),
            }

        transport = ModuleType("core.utils.claw4_timer_bridge")
        transport.call_device_tool = call_device_tool
        bridge_spec = importlib.util.spec_from_file_location(
            "claw4_learning_bridge", Path(SPEC.origin).with_name("claw4_learning_bridge.py"))
        bridge = importlib.util.module_from_spec(bridge_spec)
        with patch.dict(sys.modules, {"core.utils.claw4_timer_bridge": transport,
                                      "core.utils.claw4_learning_voice": voice,
                                      "core.utils.claw4_learning_exchange": exchange}):
            bridge_spec.loader.exec_module(bridge)
        paired = lambda: self.session
        self.assertIsNone(bridge.resolve_learning_reply(connection, "COMPLETE_REQUEST", "old", paired))
        self.assertIn("还没有收到设备确认",
                      bridge.resolve_learning_reply(connection, "COMPLETE_REQUEST", "current", lambda: None))
        self.assertEqual(calls, [])
        text = bridge.resolve_learning_reply(connection, "COMPLETE_REQUEST", "current", paired)
        self.assertIn("点确认", text)
        self.assertEqual(calls[0][3:], ("self.claw4.learning_session", "self_claw4_learning_session"))
        conn, request, turn, tool, alias = calls[1]
        self.assertIs(conn, connection)
        self.assertEqual((turn, tool, alias), ("current", "self.claw4.learning", "self_claw4_learning"))
        self.assertEqual(request["kind"], "COMPLETE_REQUEST")
        self.assertNotIn("child_id", request)

        def abort_during_call(*args):
            result = call_device_tool(*args)
            connection.client_abort = True
            return result

        with patch.object(bridge, "call_device_tool", abort_during_call):
            self.assertIsNone(bridge.resolve_learning_reply(connection, "COMPLETE_REQUEST", "current", paired))
        connection.client_abort = False
        with patch.object(bridge, "call_device_tool", call_device_tool):
            leases = iter((self.session, None))
            result = bridge.resolve_learning_reply(connection, "COMPLETE_REQUEST", "current",
                                                   lambda: next(leases))
            self.assertIn("还没有收到设备确认", result)
        before = len(calls)
        with patch.object(bridge, "call_device_tool", return_value={"session_epoch": "42", "mac": "bad"}) as call:
            self.assertIn("还没有收到设备确认",
                          bridge.resolve_learning_reply(connection, "COMPLETE_REQUEST", "current", paired))
            self.assertEqual(call.call_count, 1)
        self.assertEqual(len(calls), before)

    def test_pairing_file_and_link_scope(self):
        spec = importlib.util.spec_from_file_location("claw4_learning_pairing",
                                                     Path(SPEC.origin).with_name("claw4_learning_pairing.py"))
        pairing = importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules, {"core.utils.claw4_learning_exchange": exchange}):
            spec.loader.exec_module(pairing)
        with TemporaryDirectory() as folder:
            path = Path(folder) / "pairings.json"
            hardware = "02:00:00:00:00:01"
            record = {"device_id": "test-device", "child_id": "test-child", "device_secret": "1a" * 32}
            path.write_text(json.dumps({hardware: record}), encoding="utf-8")
            parsed = pairing._read_pairings(path)
            self.assertEqual(set(parsed), {hardware})
            conn = SimpleNamespace(device_id=hardware, session_id="12345678-1234-4123-8123-123456789abc",
                                   websocket=object(), stop_event=Event())
            with patch.object(pairing, "_configured_pairings", return_value=parsed):
                resolver = pairing.pairing_resolver(conn)
                self.assertIs(resolver(), parsed[hardware])
                conn.websocket = object()
                self.assertIsNone(resolver())
                conn.device_id = "02:00:00:00:00:02"
                self.assertIsNone(pairing.pairing_resolver(conn))
            path.write_text('{"02:00:00:00:00:01":{},"02:00:00:00:00:01":{}}', encoding="utf-8")
            self.assertEqual(pairing._read_pairings(path), {})
