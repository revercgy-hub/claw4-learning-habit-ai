import ast
from enum import Enum, auto
import hashlib
import importlib.util
from pathlib import Path
from queue import Queue
from tempfile import TemporaryDirectory
from threading import Event
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import uuid


DEPLOY = Path(__file__).resolve().parents[2] / "deploy/v6/xiaozhi-nas"


def load(name):
    spec = importlib.util.spec_from_file_location(name, DEPLOY / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


route = load("patch_learning_voice")
voice = load("claw4_learning_voice")
timer = load("claw4_timer_voice")
lookup = load("patch_live_lookup")
timer_patch = load("patch_timer_preview")


class SentenceType(Enum):
    FIRST = auto()
    MIDDLE = auto()
    LAST = auto()


class ContentType(Enum):
    ACTION = auto()
    TEXT = auto()


class RouteTests(unittest.TestCase):
    def setUp(self):
        self.history = []
        self.queue = Queue()
        self.voice_cache = []
        self.dispatched = []
        self.conn = SimpleNamespace(
            sentence_id=None, client_abort=False, stop_event=Event(),
            dialogue=SimpleNamespace(put=self.history.append),
            logger=SimpleNamespace(bind=lambda **_: SimpleNamespace(info=lambda _: None)),
            tts=SimpleNamespace(tts_text_queue=self.queue,
                                store_tts_text=lambda *values: self.voice_cache.append(values)),
            resolve_claw4_learning_pairing=lambda: None,
        )

    def install(self, answer=None):
        source = (
            lookup.IMPORT + "class ConnectionHandler:\n"
            "    def chat(self, query, depth=0):\n"
            '        if query is not None:\n            self.logger.bind(tag=TAG).info(f"大模型收到用户消息: {query}")\n'
            "        if depth == 0:\n"
            "            current_sentence_id = uuid.uuid4().hex\n"
            "            self.sentence_id = current_sentence_id\n"
            + route.USER_HISTORY +
            "            self.tts.tts_text_queue.put(TTSMessageDTO(sentence_id=current_sentence_id,\n"
            "                sentence_type=SentenceType.FIRST, content_type=ContentType.ACTION))\n"
            "        else:\n"
            "            current_sentence_id = self.sentence_id\n"
            + lookup.LOOKUP +
            "        return 'ordinary-chat'\n"
        )
        source = source.replace(lookup.IMPORT, lookup.IMPORT + "from core.utils.claw4_live_lookup import (\n"
                                "    answer as answer_live_lookup, classify as classify_live_lookup,\n)\n")
        source = source.replace(lookup.LOG, "        live_request = classify_live_lookup(query) if depth == 0 else None\n"
                                + lookup.LOG.replace("if query is not None:", "if query is not None and live_request is None:"))
        for before, after in timer_patch.REPLACEMENTS:
            self.assertEqual(source.count(before), 1)
            source = source.replace(before, after)
        with TemporaryDirectory() as folder:
            root = Path(folder)
            path = root / "core/connection.py"
            receiver = root / "core/handle/receiveAudioHandle.py"
            asr = root / "core/providers/asr/base.py"
            for file in (path, receiver, asr):
                file.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(source.encode("utf-8"))
            receiver.write_bytes(b'async def startToChat(conn: "ConnectionHandler", text):\n'
                                 b'    conn.executor.submit(conn.chat, actual_text)\n')
            asr.write_bytes(b'class Provider:\n    async def handle(self):\n'
                            b'        if True:\n            if True:\n'
                            b'                await startToChat(\n'
                            b'                    conn, _chat_text_for_known_sensevoice(self, raw_text, enhanced_text)\n'
                            b'                )\n')
            with self.assertRaises(ValueError):
                route.patch(root)
            with patch.multiple(route, CONNECTION_SHA256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                RECEIVER_SHA256=hashlib.sha256(receiver.read_bytes()).hexdigest(),
                                ASR_SHA256=hashlib.sha256(asr.read_bytes()).hexdigest()):
                route.patch(root)
            self.assertIn(b"asr_final=asr_final", receiver.read_bytes())
            self.assertIn(b"asr_final=True", asr.read_bytes())
            cls = next(node for node in ast.parse(path.read_bytes()).body if isinstance(node, ast.ClassDef))
        namespace = dict(
            uuid=uuid, classify_learning=voice.classify, classify_timer=timer.classify,
            classify_live_lookup=lambda query: query if query == "今天的新闻" else None,
            answer_live_lookup=lambda _: "新闻结果", resolve_timer_reply=lambda *_: "计时器结果",
            resolve_learning_reply=answer if answer else self.dispatch, TAG="test",
            TTSMessageDTO=SimpleNamespace, Message=SimpleNamespace,
            SentenceType=SentenceType, ContentType=ContentType,
        )
        exec(compile(ast.Module(body=[cls], type_ignores=[]), "<learning-route>", "exec"), namespace)
        return namespace["ConnectionHandler"].chat

    def dispatch(self, conn, kind, sentence_id, resolve_pairing):
        self.assertEqual((conn, sentence_id, resolve_pairing),
                         (self.conn, self.conn.sentence_id, self.conn.resolve_claw4_learning_pairing))
        self.dispatched.append(kind)
        return "请在学习页点确认。"

    def test_learning_turn_bypasses_chat_and_history(self):
        chat = self.install()
        self.assertIs(chat(self.conn, "完成当前任务", asr_final=True), True)
        self.assertEqual(self.dispatched, ["COMPLETE_REQUEST"])
        self.assertEqual(self.history, [])
        items = [self.queue.get_nowait() for _ in range(3)]
        self.assertEqual([item.sentence_type for item in items],
                         [SentenceType.FIRST, SentenceType.MIDDLE, SentenceType.LAST])
        self.assertEqual({item.sentence_id for item in items}, {self.conn.sentence_id})
        self.assertIn("点确认", items[1].content_detail)

    def test_normal_chat_timer_news_and_recursive_turns(self):
        chat = self.install()
        self.assertEqual(chat(self.conn, "一加一等于几"), "ordinary-chat")
        self.assertIs(chat(self.conn, "开始番茄钟"), True)
        self.assertIs(chat(self.conn, "今天的新闻"), True)
        self.assertEqual(chat(self.conn, "完成当前任务", depth=1, asr_final=True), "ordinary-chat")
        self.assertEqual(chat(self.conn, "完成当前任务"), "ordinary-chat")
        self.assertEqual(self.dispatched, [])
        del self.conn.resolve_claw4_learning_pairing
        self.assertEqual(chat(self.conn, "完成当前任务", asr_final=True), "ordinary-chat")

    def test_stale_turn_drops_learning_speech(self):
        def supersede(*_):
            self.conn.sentence_id = "new"
            return "旧答复"

        chat = self.install(supersede)
        self.assertIs(chat(self.conn, "完成当前任务", asr_final=True), True)
        self.assertEqual(self.queue.get_nowait().sentence_type, SentenceType.FIRST)
        self.assertTrue(self.queue.empty())
        self.assertEqual(self.voice_cache, [])
