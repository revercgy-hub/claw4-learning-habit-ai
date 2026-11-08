"""Narrow Mandarin intent parsing and deterministic replies for timer preview."""

import re


_DIGITS = {
    "零": 0,
    "〇": 0,
    "一": 1,
    "二": 2,
    "两": 2,
    "三": 3,
    "四": 4,
    "五": 5,
    "六": 6,
    "七": 7,
    "八": 8,
    "九": 9,
}


def _number(value):
    """Parse Arabic numbers or simple Chinese integers (0..999)."""
    if value.isdigit():
        return int(value)
    if not value:
        return None
    if value in _DIGITS:
        return _DIGITS[value]
    if "百" in value:
        if value.count("百") != 1:
            return None
        hundreds, rest = value.split("百")
        if hundreds not in _DIGITS or (rest and (rest.startswith("十") and len(rest) == 1)):
            if not (hundreds in _DIGITS and rest == "十"):
                return None
        tail = 0 if not rest else (10 if rest == "十" else _number(rest))
        return None if tail is None or tail > 99 else _DIGITS[hundreds] * 100 + tail
    if "十" not in value:
        digits = [_DIGITS.get(char) for char in value]
        if any(digit is None for digit in digits):
            return None
        return int("".join(str(digit) for digit in digits))
    if value.count("十") != 1:
        return None
    tens, ones = value.split("十")
    if tens and tens not in _DIGITS:
        return None
    if ones and ones not in _DIGITS:
        return None
    return (10 if not tens else _DIGITS[tens] * 10) + (0 if not ones else _DIGITS[ones])


# ASR can transcribe the final homophone as 中. Use it only in full command patterns.
_FOCUS_NAME = r"番茄[钟中]"


def _clarify(message):
    return {"clarify": message}


def classify(query):
    """Return a timer_preview request, or None for unrelated speech.

    The patterns intentionally cover only explicit, common Mandarin commands.
    """
    if not isinstance(query, str):
        return None
    text = re.sub(r"[\s，。！？!?、]+", "", query)

    # Control/status phrases are full matches to avoid stealing other requests.
    simple = (
        (rf"(?:暂停|停下)(?:一下)?(?:我的)?{_FOCUS_NAME}", {"action": 1}),
        (rf"(?:继续|恢复)(?:我的)?{_FOCUS_NAME}", {"action": 2}),
        (rf"取消(?:我的)?{_FOCUS_NAME}", {"action": 3}),
        (rf"(?:(?:我的)?{_FOCUS_NAME}还剩多久|{_FOCUS_NAME}状态|计时器状态)", {"action": 6}),
        (r"取消(?:的)?闹钟", {"action": 5}),
    )
    for pattern, result in simple:
        if re.fullmatch(pattern, text):
            return result.copy()

    match = re.fullmatch(rf"开始(?:(?P<minutes>[0-9零〇一二两三四五六七八九十百]+)分钟)?(?:的)?{_FOCUS_NAME}", text)
    if match:
        raw = match.group("minutes")
        if raw is None:
            return {"action": 0, "minutes": 25}
        minutes = _number(raw)
        if minutes is None or not 1 <= minutes <= 120:
            return _clarify("番茄钟时长请说1到120分钟。")
        return {"action": 0, "minutes": minutes}

    # Alarm time must state morning/evening/day part; no implicit AM/PM guesses.
    alarm_patterns = (
        r"(?:(?:设置|设)(?:一个|个)?)?(?P<day>明天|明早|明晚|今天|今晚)(?P<part>早上|上午|下午|晚上)?(?P<hour>[0-9零〇一二两三四五六七八九十]+)点(?:(?P<half>半)|(?P<minute>[0-9零〇一二两三四五六七八九十]+)分)?(?:钟)?(?:的)?(?:闹钟|叫我)",
        r"(?:设置|设)(?:一个|个)?(?P<day>明天|今天)(?P<part>早上|上午|下午|晚上)(?P<hour>[0-9零〇一二两三四五六七八九十]+)点(?:(?P<half>半)|(?P<minute>[0-9零〇一二两三四五六七八九十]+)分)?(?:钟)?(?:的)?闹钟",
    )
    for pattern in alarm_patterns:
        match = re.fullmatch(pattern, text)
        if not match:
            continue
        day = match.group("day")
        hour = _number(match.group("hour"))
        minute = 30 if match.groupdict().get("half") else _number(match.group("minute") or "0")
        part = match.groupdict().get("part")
        if day in ("明天", "今天") and not part:
            return _clarify("请说明闹钟是早上还是晚上。")
        if hour is None or minute is None or not 0 <= hour <= 23 or not 0 <= minute <= 59:
            return _clarify("请说清楚闹钟的小时和分钟。")
        if part in ("下午", "晚上") or day == "明晚" or day == "今晚":
            if hour < 1 or hour > 12:
                return _clarify("请说清楚闹钟的时间。")
            hour = hour % 12 + (12 if part in ("下午", "晚上") or day in ("明晚", "今晚") else 0)
        offset = 1 if day in ("明天", "明早", "明晚") else 0
        return {"action": 4, "hour": hour, "minute": minute, "day_offset": offset}

    # Recognized command stems with malformed/missing values need clarification.
    if re.fullmatch(rf"(?:开始|启动).*(?:{_FOCUS_NAME}|计时)", text):
        return _clarify("请说番茄钟时长，例如：开始25分钟番茄钟。")
    if re.fullmatch(r"取消(?:今天|明天|明早|明晚|今晚).*(?:闹钟)", text):
        return _clarify("设备只能取消当前闹钟，请直接说：取消闹钟。")
    if re.fullmatch(r"(?:设置|设|明早|明天|今晚|明晚).*(?:闹钟|叫我)", text):
        return _clarify("请说清楚闹钟时间，例如：明早七点叫我。")
    return None


def reply(request, result):
    """Speak only fixed text backed by a recognized device result."""
    if isinstance(request, dict) and isinstance(request.get("clarify"), str):
        return request["clarify"]
    if not isinstance(request, dict) or not isinstance(result, dict):
        return "设备结果尚未确认，暂不能报告计时器状态。"
    status = result.get("status")
    if status not in {"ok", "busy", "inactive", "clock_not_ready", "invalid_time", "not_saved"}:
        return "设备结果尚未确认，暂不能报告计时器状态。"

    if status == "busy":
        return "设备正在处理其他计时器操作，请稍后再试。"
    if status == "inactive":
        return "当前没有正在运行的番茄钟。"
    if status == "clock_not_ready":
        return "设备时钟尚未就绪，暂时无法设置闹钟。"
    if status == "invalid_time":
        return "设备未接受这个闹钟时间，请重新设置。"
    if status == "not_saved":
        return "闹钟没有保存成功。"

    action = request.get("action")
    if action == 0:
        return "番茄钟已开始。"
    if action == 1:
        return "番茄钟已暂停。"
    if action == 2:
        return "番茄钟已继续。"
    if action == 3:
        return "番茄钟已取消。"
    if action == 4:
        hour, minute = result.get("alarm_hour"), result.get("alarm_minute")
        if not isinstance(hour, int) or isinstance(hour, bool) or not 0 <= hour <= 23:
            return "设备结果尚未确认，暂不能报告闹钟已设置。"
        if not isinstance(minute, int) or isinstance(minute, bool) or not 0 <= minute <= 59:
            return "设备结果尚未确认，暂不能报告闹钟已设置。"
        if hour != request.get("hour") or minute != request.get("minute"):
            return "设备结果尚未确认，暂不能报告闹钟已设置。"
        return "闹钟已设置为%02d:%02d。" % (hour, minute)
    if action == 5:
        return "闹钟已取消。"
    if action == 6:
        seconds = result.get("remaining_seconds")
        if not isinstance(seconds, int) or isinstance(seconds, bool) or seconds < 0:
            return "设备结果尚未确认，暂不能报告剩余时间。"
        minutes = (seconds + 59) // 60
        return "番茄钟还剩%d分钟。" % minutes
    return "设备结果尚未确认，暂不能报告计时器状态。"
