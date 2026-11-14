"""Deterministic Mandarin learning commands and device-result speech."""

import re


_COMMANDS = {
    "查看今日任务": "QUERY_TODAY_TASKS",
    "查看今天的任务": "QUERY_TODAY_TASKS",
    "今天有哪些学习任务": "QUERY_TODAY_TASKS",
    "开始学习任务": "START_TASK",
    "开始当前任务": "START_TASK",
    "暂停学习任务": "PAUSE_TASK",
    "暂停当前任务": "PAUSE_TASK",
    "继续学习任务": "RESUME_TASK",
    "继续当前任务": "RESUME_TASK",
    "完成学习任务": "COMPLETE_REQUEST",
    "完成当前任务": "COMPLETE_REQUEST",
    "查看任务剩余时间": "QUERY_REMAINING_TIME",
    "当前任务还剩多久": "QUERY_REMAINING_TIME",
    "延后学习提醒": "SNOOZE_REMINDER",
    "稍后提醒我学习": "SNOOZE_REMINDER",
}
_APPLIED = {
    "START_TASK": "学习任务已开始。",
    "PAUSE_TASK": "学习任务已暂停。",
    "RESUME_TASK": "学习任务已继续。",
    "SNOOZE_REMINDER": "学习提醒已延后。",
}
_REJECTIONS = {
    "AMBIGUOUS_TASK": "有多个可开始的任务，请在学习页选择一项。",
    "INVALID_STATE": "当前没有可执行这项操作的学习任务或提醒。",
    "STALE": "学习状态已更新，请重新发出指令。",
    "BUSY": "设备暂时无法执行这项学习操作。",
    "STORAGE_ERROR": "设备没有保存这项学习操作，请稍后再试。",
}
_UNCONFIRMED = "还没有收到设备确认，请查看学习页。"


def classify(query):
    """Recognize whole positive requests before ordinary chat routing."""
    if not isinstance(query, str) or len(query) > 64:
        return None
    text = re.sub(r"\s+", "", query).strip("，。！？!?")
    if text.startswith("请"):
        text = text[1:]
    return _COMMANDS.get(text)


def reply(kind, command_id, result):
    """Speak a result only when it is correlated to the current request."""
    if not command_id or not isinstance(result, dict) or result.get("command_id") != command_id:
        return _UNCONFIRMED
    status = result.get("status")
    if not isinstance(status, str):
        return _UNCONFIRMED
    if status in _REJECTIONS:
        return _REJECTIONS[status]
    if kind == "COMPLETE_REQUEST" and status == "CONFIRMATION_REQUIRED":
        return "请在学习页点确认，完成记录才会保存。"
    if status != "APPLIED":
        return _UNCONFIRMED
    if kind in _APPLIED:
        return _APPLIED[kind]
    if kind == "QUERY_REMAINING_TIME":
        seconds = result.get("remaining_seconds")
        if type(seconds) is int and 0 <= seconds <= 24 * 3600:
            minutes, seconds = divmod(seconds, 60)
            return f"学习任务还剩{minutes}分{seconds}秒。"
    if kind == "QUERY_TODAY_TASKS":
        count, snapshot = result.get("task_count"), result.get("snapshot_epoch_ms")
        if type(count) is int and 0 <= count <= 256 and type(snapshot) is int and snapshot > 0:
            return f"设备上次同步的今日任务有{count}项，详情请看学习页。"
    return _UNCONFIRMED
