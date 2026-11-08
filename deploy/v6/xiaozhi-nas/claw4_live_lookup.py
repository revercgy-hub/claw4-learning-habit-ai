"""Small, fixed-source weather and headline lookup for the Claw4 NAS voice flow."""

from __future__ import annotations

from datetime import date, datetime, timedelta, timezone
from email.utils import parsedate_to_datetime
from html import unescape
import json
import re
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET


FORECAST_URL = "https://api.open-meteo.com/v1/forecast"
NEWS_URL = "https://www.chinanews.com.cn/rss/scroll-news.xml"
CHINA_TZ = timezone(timedelta(hours=8))
_MAX_RESPONSE = 256 * 1024
_TIMEOUT_SECONDS = 5

# Approximate city-centre coordinates. Requests never control the URL or coordinates.
CITIES = {
    "北京": (39.9042, 116.4074), "上海": (31.2304, 121.4737),
    "广州": (23.1291, 113.2644), "深圳": (22.5431, 114.0579),
    "杭州": (30.2741, 120.1551), "成都": (30.5728, 104.0668),
    "天津": (39.0842, 117.2010), "重庆": (29.5630, 106.5516),
    "南京": (32.0603, 118.7969), "武汉": (30.5928, 114.3055),
    "西安": (34.3416, 108.9398), "郑州": (34.7466, 113.6254),
    "长沙": (28.2282, 112.9388), "沈阳": (41.8057, 123.4315),
    "哈尔滨": (45.8038, 126.5349), "长春": (43.8171, 125.3235),
    "济南": (36.6512, 117.1201), "青岛": (36.0671, 120.3826),
    "福州": (26.0745, 119.2965), "厦门": (24.4798, 118.0894),
    "南昌": (28.6820, 115.8579), "合肥": (31.8206, 117.2272),
    "昆明": (25.0389, 102.7183), "贵阳": (26.6470, 106.6302),
    "南宁": (22.8170, 108.3665), "海口": (20.0440, 110.1999),
    "兰州": (36.0611, 103.8343), "西宁": (36.6171, 101.7782),
    "银川": (38.4872, 106.2309), "乌鲁木齐": (43.8256, 87.6168),
    "石家庄": (38.0428, 114.5149), "太原": (37.8706, 112.5489),
    "呼和浩特": (40.8426, 111.7492), "拉萨": (29.6520, 91.1721),
}

_WEATHER_PATTERN = re.compile(r"天气|气温|温度|下雨|降雨|天气预报")
_EXPLAIN_WEATHER = re.compile(r"什么是|是什么意思|解释|介绍|写一篇|作文")
_NEWS_PATTERN = re.compile(r"新闻|头条")


def classify(query: str):
    """Return a bounded intent dict or None; never include the transcript."""
    if not isinstance(query, str) or not query.strip():
        return None
    text = query.strip()
    if _EXPLAIN_WEATHER.search(text) or "写一篇新闻" in text or "写新闻" in text:
        return None

    if _WEATHER_PATTERN.search(text):
        if re.search(r"现在|当前|此刻|多少度", text):
            day = "current"
        elif re.search(r"明天|明日", text):
            day = "tomorrow"
        elif re.search(r"今天|今日", text):
            day = "today"
        else:
            day = "today"
        city = next((name for name in sorted(CITIES, key=len, reverse=True) if name in text), None)
        return {"kind": "weather", "city": city, "day": day}
    if re.search(r"现在|当前|此刻", text) and re.search(r"多少度|几度|气温", text):
        city = next((name for name in sorted(CITIES, key=len, reverse=True) if name in text), None)
        return {"kind": "weather", "city": city, "day": "current"}

    if _NEWS_PATTERN.search(text) and not re.search(r"写|创作|编一", text):
        return {"kind": "news"}
    return None


class _NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


_OPENER = urllib.request.build_opener(_NoRedirect)


def _fetch(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "Claw4LiveLookup/1.0"})
    with _OPENER.open(request, timeout=_TIMEOUT_SECONDS) as response:
        data = response.read(_MAX_RESPONSE + 1)
    if len(data) > _MAX_RESPONSE:
        raise ValueError("response too large")
    return data


def answer(request) -> str:
    """Resolve a classified request into deterministic spoken Chinese."""
    if not isinstance(request, dict):
        return "我暂时无法查询，请稍后再试。"
    kind = request.get("kind")
    if kind == "weather":
        city = request.get("city")
        day = request.get("day")
        if not isinstance(city, str) or city not in CITIES:
            return "请告诉我城市名称和要查询的天气，比如北京今天的天气。"
        if day not in ("today", "tomorrow", "current"):
            day = "today"
        return _weather(city, day)
    if kind == "news":
        return _news()
    return "我暂时无法查询，请稍后再试。"


def _weather(city: str, day: str) -> str:
    lat, lon = CITIES[city]
    url = (f"{FORECAST_URL}?latitude={lat}&longitude={lon}"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min"
           "&forecast_days=2&timezone=Asia%2FShanghai"
           + ("&current=temperature_2m,weather_code" if day == "current" else ""))
    try:
        payload = json.loads(_fetch(url).decode("utf-8"))
        daily = payload["daily"]
        if day == "current":
            current = payload["current"]
            current_time = datetime.fromisoformat(current["time"])
            if current_time.tzinfo is None:
                current_time = current_time.replace(tzinfo=CHINA_TZ)
            current_time = current_time.astimezone(CHINA_TZ)
            now = datetime.now(CHINA_TZ)
            temp, code = float(current["temperature_2m"]), int(current["weather_code"])
            if abs((now - current_time).total_seconds()) > 3 * 3600 or not -80 <= temp <= 60:
                raise ValueError("stale or invalid current observation")
            return f"{city}现在{_weather_word(code)}，气温{temp:g}摄氏度。"
        index = 0 if day == "today" else 1
        dates = daily["time"]
        highs, lows, codes = daily["temperature_2m_max"], daily["temperature_2m_min"], daily["weather_code"]
        if not (len(dates) > index and len(highs) > index and len(lows) > index and len(codes) > index):
            raise ValueError("missing forecast")
        high, low, code = float(highs[index]), float(lows[index]), int(codes[index])
        if not (-80 <= low <= 60 and -80 <= high <= 60 and -80 <= code <= 1000):
            raise ValueError("invalid forecast")
        label = "今天" if day == "today" else "明天"
        return f"{city}{label}{_weather_word(code)}，气温{low:g}到{high:g}摄氏度。"
    except (OSError, urllib.error.URLError, TimeoutError, ValueError, KeyError, TypeError, IndexError, UnicodeError):
        return f"暂时无法获取{city}的天气，请稍后再试。"


def _weather_word(code: int) -> str:
    if code == 0:
        return "晴"
    if code in (1, 2, 3):
        return "多云"
    if code in (45, 48):
        return "有雾"
    if code in (51, 53, 55, 56, 57):
        return "有毛毛雨"
    if code in (61, 63, 65, 66, 67, 80, 81, 82):
        return "有雨"
    if code in (71, 73, 75, 77, 85, 86):
        return "有雪"
    if code in (95, 96, 99):
        return "有雷雨"
    return "天气情况暂不明确"


def _news() -> str:
    try:
        root = ET.fromstring(_fetch(NEWS_URL))
        channel = root.find("channel")
        if channel is None:
            raise ValueError("missing channel")
        today = datetime.now(CHINA_TZ).date()
        headlines = []
        for item in channel.findall("item"):
            title_node, date_node = item.find("title"), item.find("pubDate")
            if title_node is None or date_node is None or not title_node.text or not date_node.text:
                continue
            try:
                published = parsedate_to_datetime(date_node.text.strip())
                if published.tzinfo is None:
                    published = published.replace(tzinfo=timezone.utc)
                if published.astimezone(CHINA_TZ).date() != today:
                    continue
            except (TypeError, ValueError, OverflowError):
                continue
            title = re.sub(r"\s+", " ", unescape(re.sub(r"<[^>]*>", " ", title_node.text))).strip()
            title = "".join(ch for ch in title if ch.isprintable())[:100]
            if title:
                headlines.append(title)
            if len(headlines) == 3:
                break
        if not headlines:
            return "暂时没有找到今天发布的新闻。"
        return "中新网今日新闻：" + "；".join(headlines) + "。"
    except (OSError, urllib.error.URLError, TimeoutError, ValueError, ET.ParseError, UnicodeError):
        return "暂时无法获取新闻，请稍后再试。"
