import json
import importlib.util
from pathlib import Path
import unittest
from datetime import datetime
from unittest.mock import patch

MODULE_PATH = Path(__file__).resolve().parents[2] / "deploy" / "v6" / "xiaozhi-nas" / "claw4_live_lookup.py"
SPEC = importlib.util.spec_from_file_location("claw4_live_lookup", MODULE_PATH)
lookup = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(lookup)


class ClassificationTests(unittest.TestCase):
    def test_weather_intents(self):
        self.assertEqual(lookup.classify("北京今天的天气怎么样"), {"kind": "weather", "city": "北京", "day": "today"})
        self.assertEqual(lookup.classify("今天北京天气怎么样"), {"kind": "weather", "city": "北京", "day": "today"})
        self.assertEqual(lookup.classify("上海明天天气"), {"kind": "weather", "city": "上海", "day": "tomorrow"})
        self.assertEqual(lookup.classify("今天天气怎么样"), {"kind": "weather", "city": None, "day": "today"})
        self.assertEqual(lookup.classify("北京现在多少度"), {"kind": "weather", "city": "北京", "day": "current"})

    def test_news_and_unrelated_queries(self):
        for query in ("今天有什么新闻", "今天的新闻", "最新新闻"):
            self.assertEqual(lookup.classify(query), {"kind": "news"})
        for query in ("什么是天气预报", "写一篇新闻", "你好", "天气相关的作文", "天气预报怎么形成",
                      "我今天看了新闻，天气也很好", "昨天新闻"):
            self.assertIsNone(lookup.classify(query))

    def test_common_spoken_forms_and_unknown_city(self):
        self.assertEqual(lookup.classify("帮我查一下北京市今天的天气。")["city"], "北京")
        self.assertEqual(lookup.classify("现在多少度？")["day"], "current")
        self.assertEqual(lookup.classify("请播报今天的新闻。"), {"kind": "news"})
        self.assertIsNone(lookup.classify("请写一篇新闻"))
        self.assertIsNone(lookup.classify("牡丹江今天的天气")["city"])

    def test_ambiguous_city_requests_city_clarification(self):
        self.assertEqual(lookup.classify("北京和上海今天的天气怎么样"),
                         {"kind": "weather", "city": None, "day": "today"})
        self.assertIn("告诉我城市", lookup.answer(lookup.classify("北京和上海今天的天气怎么样")))

    def test_transcript_is_not_returned(self):
        request = lookup.classify("北京今天的天气怎么样，家里有点冷")
        self.assertNotIn("家里", repr(request))


class AnswerTests(unittest.TestCase):
    def test_weather_url_uses_fixed_city_coordinates(self):
        today = datetime.now(lookup.CHINA_TZ).date().isoformat()
        tomorrow = (datetime.now(lookup.CHINA_TZ).date() + lookup.timedelta(days=1)).isoformat()
        payload = {"daily": {"time": [today, tomorrow],
                             "temperature_2m_max": [18, 19], "temperature_2m_min": [8, 9],
                             "weather_code": [0, 3]}}
        with patch.object(lookup, "_fetch", return_value=json.dumps(payload).encode()) as fetch:
            result = lookup.answer({"kind": "weather", "city": "北京", "day": "today"})
        self.assertEqual(result, "北京今天，Open-Meteo预报晴，气温8到18摄氏度。")
        self.assertIn("latitude=39.9042&longitude=116.4074", fetch.call_args.args[0])
        self.assertNotIn("家里", fetch.call_args.args[0])

    def test_missing_city_and_outage(self):
        self.assertIn("告诉我城市名称", lookup.answer({"kind": "weather", "city": None, "day": "today"}))
        with patch.object(lookup, "_fetch", side_effect=OSError("secret provider body")):
            self.assertEqual(lookup.answer({"kind": "weather", "city": "北京", "day": "today"}),
                             "暂时无法获取北京的天气，请稍后再试。")

    def test_current_weather_rejects_stale_observation(self):
        old = "2000-01-01T00:00"
        payload = {"current": {"time": old, "temperature_2m": 12.0, "weather_code": 0}}
        with patch.object(lookup, "_fetch", return_value=json.dumps(payload).encode()):
            self.assertEqual(lookup.answer({"kind": "weather", "city": "北京", "day": "current"}),
                             "暂时无法获取北京的天气，请稍后再试。")

    def test_daily_weather_rejects_wrong_date_and_inverted_range(self):
        payload = {"daily": {"time": ["2000-01-01", "2000-01-02"],
                             "temperature_2m_max": [5, 5], "temperature_2m_min": [10, 10],
                             "weather_code": [0, 0]}}
        with patch.object(lookup, "_fetch", return_value=json.dumps(payload).encode()):
            self.assertIn("暂时无法获取", lookup.answer({"kind": "weather", "city": "北京", "day": "today"}))

    def test_news_only_speaks_fresh_bounded_headlines(self):
        from datetime import timedelta
        today = (datetime.now(lookup.CHINA_TZ) - timedelta(minutes=2)).strftime("%a, %d %b %Y %H:%M:%S +0800")
        old = "Mon, 01 Jan 2001 00:00:00 +0800"
        later = (datetime.now(lookup.CHINA_TZ) + timedelta(minutes=2)).strftime("%a, %d %b %Y %H:%M:%S +0800")
        xml = f"""<rss><channel><item><title>较早要闻</title><pubDate>{today}</pubDate></item>
        <item><title>较新要闻</title><pubDate>{(datetime.now(lookup.CHINA_TZ) - timedelta(minutes=1)).strftime('%a, %d %b %Y %H:%M:%S +0800')}</pubDate></item>
        <item><title>未来消息</title><pubDate>{later}</pubDate></item>
        <item><title>旧消息</title><pubDate>{old}</pubDate></item></channel></rss>""".encode()
        with patch.object(lookup, "_fetch", return_value=xml):
            result = lookup.answer({"kind": "news"})
        self.assertEqual(result, "中新网今日新闻：较新要闻；较早要闻。")
        with patch.object(lookup, "_fetch", return_value=b"<rss><channel/></rss>"):
            self.assertEqual(lookup.answer({"kind": "news"}), "暂时没有找到今天发布的新闻。")

    def test_malformed_provider_and_request_are_safe(self):
        with patch.object(lookup, "_fetch", return_value=b"not json"):
            self.assertEqual(lookup.answer({"kind": "weather", "city": "北京", "day": "today"}),
                             "暂时无法获取北京的天气，请稍后再试。")
        self.assertEqual(lookup.answer(None), "我暂时无法查询，请稍后再试。")


if __name__ == "__main__":
    unittest.main()
