import importlib.util
from pathlib import Path
import unittest


MODULE_PATH = (
    Path(__file__).resolve().parents[2]
    / "deploy"
    / "v6"
    / "xiaozhi-nas"
    / "claw4_timer_voice.py"
)
SPEC = importlib.util.spec_from_file_location("claw4_timer_voice", MODULE_PATH)
timer_voice = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(timer_voice)


class ClassifyTests(unittest.TestCase):
    def test_focus_start_and_control(self):
        self.assertEqual(timer_voice.classify("开始25分钟番茄钟"), {"action": 0, "minutes": 25})
        self.assertEqual(timer_voice.classify("开始番茄钟"), {"action": 0, "minutes": 25})
        self.assertEqual(timer_voice.classify("开始一百二十分钟的番茄钟"), {"action": 0, "minutes": 120})
        self.assertEqual(timer_voice.classify("暂停番茄钟"), {"action": 1})
        self.assertEqual(timer_voice.classify("继续番茄钟"), {"action": 2})
        self.assertEqual(timer_voice.classify("取消番茄钟"), {"action": 3})
        self.assertEqual(timer_voice.classify("番茄钟还剩多久"), {"action": 6})

    def test_timer_homophone_only_matches_full_commands(self):
        self.assertEqual(timer_voice.classify("开始一分钟番茄中。"), {"action": 0, "minutes": 1})
        self.assertEqual(timer_voice.classify("暂停番茄中"), {"action": 1})
        self.assertIsNone(timer_voice.classify("番茄中含有维生素吗"))
        self.assertIsNone(timer_voice.classify("不要开始一分钟番茄中"))

    def test_alarm_explicit_time_and_half(self):
        self.assertEqual(
            timer_voice.classify("设置明天早上七点的闹钟"),
            {"action": 4, "hour": 7, "minute": 0, "day_offset": 1},
        )
        self.assertEqual(
            timer_voice.classify("明早七点半叫我"),
            {"action": 4, "hour": 7, "minute": 30, "day_offset": 1},
        )
        self.assertEqual(
            timer_voice.classify("设一个明早七点的闹钟"),
            {"action": 4, "hour": 7, "minute": 0, "day_offset": 1},
        )
        self.assertEqual(
            timer_voice.classify("今晚8点15分叫我"),
            {"action": 4, "hour": 20, "minute": 15, "day_offset": 0},
        )
        self.assertEqual(timer_voice.classify("取消闹钟"), {"action": 5})

    def test_clarifies_invalid_or_ambiguous_targeted_requests(self):
        self.assertIn("clarify", timer_voice.classify("开始121分钟番茄钟"))
        self.assertIn("clarify", timer_voice.classify("设置明天七点的闹钟"))
        self.assertIn("clarify", timer_voice.classify("设置明早二十五点的闹钟"))
        self.assertIn("clarify", timer_voice.classify("取消明天的闹钟"))
        self.assertEqual(
            timer_voice.reply(timer_voice.classify("开始121分钟番茄钟"), None),
            "番茄钟时长请说1到120分钟。",
        )

    def test_unrelated_queries_are_not_routed(self):
        for query in ("帮我安排今天的学习计划", "明天七点有数学课", "我想吃番茄", "暂停一下"):
            self.assertIsNone(timer_voice.classify(query), query)


class ReplyTests(unittest.TestCase):
    def test_fixed_reply_after_verified_success(self):
        self.assertEqual(timer_voice.reply({"action": 0}, {"status": "ok"}), "番茄钟已开始。")
        self.assertEqual(
            timer_voice.reply(
                {"action": 4, "hour": 7, "minute": 5},
                {"status": "ok", "alarm_hour": 7, "alarm_minute": 5},
            ),
            "闹钟已设置为07:05。",
        )
        self.assertIn(
            "尚未确认",
            timer_voice.reply(
                {"action": 4, "hour": 8, "minute": 5},
                {"status": "ok", "alarm_hour": 7, "alarm_minute": 5},
            ),
        )
        self.assertEqual(
            timer_voice.reply({"action": 6}, {"status": "ok", "remaining_seconds": 61}),
            "番茄钟还剩2分钟。",
        )

    def test_errors_and_malformed_results_never_claim_success(self):
        self.assertIn("尚未确认", timer_voice.reply({"action": 0}, None))
        self.assertIn("尚未确认", timer_voice.reply({"action": 0}, {"status": "unexpected", "message": "OK"}))
        self.assertIn("没有正在运行", timer_voice.reply({"action": 6}, {"status": "inactive"}))
        self.assertIn("尚未确认", timer_voice.reply({"action": 4}, {"status": "ok", "alarm_hour": "7", "alarm_minute": 0}))


if __name__ == "__main__":
    unittest.main()
