"""Small, fixed-source weather and headline lookup for the Claw4 NAS voice flow."""

from __future__ import annotations

from datetime import datetime, timedelta, timezone
from email.utils import parsedate_to_datetime
from html import unescape
import json
import re
import xml.etree.ElementTree as ET
import asyncio


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

_CITY_ALTERNATION = "|".join(re.escape(name) for name in sorted(CITIES, key=len, reverse=True))
_CITIES_FORM = rf"(?P<city>(?:{_CITY_ALTERNATION})(?:和(?:{_CITY_ALTERNATION}))*)?"
_WEATHER_FORMS = (
    re.compile(rf"{_CITIES_FORM}(?P<day>今天|今日|明天|明日)?的?天气(?:怎么样|如何|预报)?"),
    re.compile(rf"(?P<day>今天|今日|明天|明日){_CITIES_FORM}的?天气(?:怎么样|如何|预报)?"),
    re.compile(rf"{_CITIES_FORM}(?P<day>今天|今日|明天|明日)天气"),
    re.compile(rf"(?P<city>{_CITY_ALTERNATION})(?:现在|当前|此刻)(?:的)?天气(?:怎么样|如何)?"),
    re.compile(rf"(?P<city>{_CITY_ALTERNATION})(?:现在|当前|此刻)(?:有)?多少度"),
)
_NEWS_FORMS = tuple(re.compile(pattern) for pattern in (
    r"今天有什么新闻", r"今日有什么新闻", r"今天的新闻", r"今日新闻", r"最新新闻", r"今天新闻",
))


def classify(query: str):
    """Return a bounded intent dict or None; never include the transcript."""
    if not isinstance(query, str) or not query.strip():
        return None
    text = re.sub(r"[\s，。？！!?、]+$", "", query.strip())
    if any(pattern.fullmatch(text) for pattern in _NEWS_FORMS):
        return {"kind": "news"}
    for pattern in _WEATHER_FORMS:
        match = pattern.fullmatch(text)
        if not match:
            continue
        city_text = match.groupdict().get("city") or ""
        city_hits = [name for name in CITIES if name in city_text]
        city = city_hits[0] if len(city_hits) == 1 and city_text.replace(city_hits[0], "") in ("", "的") else None
        day_text = match.groupdict().get("day")
        day = "current" if "现在" in text or "当前" in text or "此刻" in text else (
            "tomorrow" if day_text in ("明天", "明日") else "today")
        return {"kind": "weather", "city": city, "day": day}
    return None


def _fetch(url: str) -> bytes:
    async def fetch() -> bytes:
        import aiohttp
        timeout = aiohttp.ClientTimeout(total=_TIMEOUT_SECONDS)
        async with aiohttp.ClientSession(timeout=timeout, headers={"User-Agent": "Claw4LiveLookup/1.0"}) as session:
            async with session.get(url, allow_redirects=False) as response:
                response.raise_for_status()
                chunks, size = [], 0
                async for chunk in response.content.iter_chunked(8192):
                    size += len(chunk)
                    if size > _MAX_RESPONSE:
                        raise ValueError("response too large")
                    chunks.append(chunk)
                return b"".join(chunks)
    return asyncio.run(fetch())


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
            age_seconds = (now - current_time).total_seconds()
            if age_seconds > 3 * 3600 or age_seconds < -300 or not -80 <= temp <= 60:
                raise ValueError("stale or invalid current observation")
            return f"{city}截至{current_time:%H:%M}，Open-Meteo显示{_weather_word(code)}，气温{temp:g}摄氏度。"
        index = 0 if day == "today" else 1
        dates = daily["time"]
        highs, lows, codes = daily["temperature_2m_max"], daily["temperature_2m_min"], daily["weather_code"]
        if not (len(dates) > index and len(highs) > index and len(lows) > index and len(codes) > index):
            raise ValueError("missing forecast")
        high, low, code = float(highs[index]), float(lows[index]), int(codes[index])
        expected_date = datetime.now(CHINA_TZ).date() + timedelta(days=index)
        if (dates[index] != expected_date.isoformat() or not (-80 <= low <= 60 and -80 <= high <= 60)
                or high < low or not -80 <= code <= 1000):
            raise ValueError("invalid forecast")
        label = "今天" if day == "today" else "明天"
        return f"{city}{label}，Open-Meteo预报{_weather_word(code)}，气温{low:g}到{high:g}摄氏度。"
    except Exception:
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
        dated_headlines = []
        for item in channel.findall("item"):
            title_node, date_node = item.find("title"), item.find("pubDate")
            if title_node is None or date_node is None or not title_node.text or not date_node.text:
                continue
            try:
                published = parsedate_to_datetime(date_node.text.strip())
                if published.tzinfo is None:
                    published = published.replace(tzinfo=timezone.utc)
                local_time = published.astimezone(CHINA_TZ)
                if local_time.date() != today or local_time > datetime.now(CHINA_TZ):
                    continue
            except (TypeError, ValueError, OverflowError):
                continue
            title = re.sub(r"\s+", " ", unescape(re.sub(r"<[^>]*>", " ", title_node.text))).strip()
            title = "".join(ch for ch in title if ch.isprintable())[:100]
            if title:
                dated_headlines.append((local_time, title))
        headlines = [title for _, title in sorted(dated_headlines, key=lambda row: row[0], reverse=True)[:3]]
        if not headlines:
            return "暂时没有找到今天发布的新闻。"
        return "中新网今日新闻：" + "；".join(headlines) + "。"
    except Exception:
        return "暂时无法获取新闻，请稍后再试。"
