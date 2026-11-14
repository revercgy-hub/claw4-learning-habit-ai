import importlib.util
from pathlib import Path
import unittest


SPEC = importlib.util.spec_from_file_location(
    "claw4_learning_voice",
    Path(__file__).resolve().parents[2] / "deploy/v6/xiaozhi-nas/claw4_learning_voice.py",
)
voice = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(voice)


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
