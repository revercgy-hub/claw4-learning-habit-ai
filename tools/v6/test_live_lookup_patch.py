from enum import Enum, auto
import importlib.util
from pathlib import Path
from queue import Queue
from types import SimpleNamespace
import unittest
from threading import Event

DEPLOY = Path(__file__).resolve().parents[2] / "deploy/v6/xiaozhi-nas"
SPEC = importlib.util.spec_from_file_location("patch_live_lookup", DEPLOY / "patch_live_lookup.py")
PATCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCH)


class SentenceType(Enum):
    MIDDLE = auto()
    LAST = auto()


class ContentType(Enum):
    TEXT = auto()
    ACTION = auto()


class LookupTurnTests(unittest.TestCase):
    def setUp(self):
        self.conn = SimpleNamespace(
            client_abort=False,
            stop_event=Event(),
            sentence_id="turn",
            tts=SimpleNamespace(tts_text_queue=Queue(), store_tts_text=lambda *args: None),
            dialogue=SimpleNamespace(put=lambda message: None),
        )

    def turn(self, request, answer):
        namespace = dict(
            answer_live_lookup=answer,
            TTSMessageDTO=SimpleNamespace,
            Message=SimpleNamespace,
            SentenceType=SentenceType,
            ContentType=ContentType,
        )
        exec("def run(self, live_request, current_sentence_id):\n" + PATCH.LOOKUP, namespace)
        return namespace["run"](self.conn, request, "turn")

    def test_non_lookup_falls_through(self):
        self.assertIsNone(self.turn(None, lambda _: self.fail("ordinary turn called lookup")))

    def test_lookup_queues_reply_and_end_on_same_turn(self):
        self.assertTrue(self.turn(object(), lambda _: "天气答复"))
        queue = self.conn.tts.tts_text_queue
        text, last = queue.get_nowait(), queue.get_nowait()
        self.assertEqual((text.sentence_type, last.sentence_type), (SentenceType.MIDDLE, SentenceType.LAST))
        self.assertEqual((text.sentence_id, last.sentence_id), ("turn", "turn"))
        self.assertEqual(text.content_detail, "天气答复")
        self.assertTrue(queue.empty())

    def test_abort_or_replacement_during_lookup_drops_reply(self):
        for attr, value in (("client_abort", True), ("sentence_id", "next")):
            with self.subTest(attr=attr):
                self.setUp()

                def abort(_):
                    setattr(self.conn, attr, value)
                    return "过期答复"

                self.assertTrue(self.turn(object(), abort))
                self.assertTrue(self.conn.tts.tts_text_queue.empty())
