from __future__ import annotations

import ast
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import threading
import types
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
DEPLOY = ROOT / "deploy/v6/xiaozhi-nas"
FIXTURE = ROOT / "tools/v6/fixtures/connection_v096.py"


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


helper = load("claw4_ollama_prewarm", DEPLOY / "claw4_ollama_prewarm.py")
patcher = load("patch_ollama_prewarm", DEPLOY / "patch_ollama_prewarm.py")


class FakeLogger:
    def bind(self, **_): return self
    def info(self, *_): pass
    def error(self, *_): pass


class PrewarmTests(unittest.TestCase):
    def test_fixture_hash_patch_and_fail_closed(self):
        self.assertEqual(hashlib.sha256(FIXTURE.read_bytes()).hexdigest(), patcher.EXPECTED_SHA256)
        with tempfile.TemporaryDirectory() as td:
            target = Path(td) / "connection.py"
            target.write_bytes(FIXTURE.read_bytes())
            patcher.patch(target)
            self.assertEqual(target.read_text(encoding="utf-8").count("schedule_ollama_prewarm(self.llm)"), 1)
            once = target.read_bytes()
            with self.assertRaises(ValueError): patcher.patch(target)
            self.assertEqual(target.read_bytes(), once)
            target.write_bytes(FIXTURE.read_bytes() + b"# changed\n")
            before = target.read_bytes()
            with self.assertRaises(ValueError): patcher.patch(target)
            self.assertEqual(target.read_bytes(), before)

    def _patched_init(self):
        with tempfile.TemporaryDirectory() as td:
            target = Path(td) / "connection.py"
            target.write_bytes(FIXTURE.read_bytes())
            patcher.patch(target)
            tree = ast.parse(target.read_text(encoding="utf-8"))
        cls = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == "ConnectionHandler")
        method = next(n for n in cls.body if isinstance(n, ast.FunctionDef) and n.name == "_initialize_components")
        namespace = {"TAG": "test", "build_module_string": lambda *_: "modules",
                     "create_connection_logger": lambda *_: FakeLogger(),
                     "asyncio": types.SimpleNamespace(run_coroutine_threadsafe=lambda *_: None)}
        exec(compile(ast.Module(body=[method], type_ignores=[]), "patched_connection", "exec"), namespace)
        return namespace[method.name]

    def _connection(self, need_bind=False):
        class Conn:
            pass
        c = Conn()
        c.tts = types.SimpleNamespace(open_audio_channels=lambda *_: None)
        c.asr = types.SimpleNamespace(open_audio_channels=lambda *_: None)
        c.llm = object(); c.loop = object(); c.need_bind = need_bind
        c.bind_completed_event = types.SimpleNamespace(set=lambda: setattr(c, "bound", True))
        c.logger = FakeLogger(); c.config = {"selected_module": {}, "prompt": None}
        c.vad = object(); c._vad = object()
        for name in ("_initialize_voiceprint", "_initialize_memory", "_initialize_intent",
                     "_init_report_threads", "_init_prompt_enhancement", "_inject_tool_call_fewshot"):
            setattr(c, name, lambda: None)
        return c

    def test_real_patched_initialization_bind_and_failure_isolation(self):
        init = self._patched_init()
        mod = types.ModuleType("core.utils.claw4_ollama_prewarm")
        calls = []
        mod.schedule_ollama_prewarm = lambda provider: calls.append(provider)
        with patch.dict(sys.modules, {"core": types.ModuleType("core"),
                                      "core.utils": types.ModuleType("core.utils"),
                                      "core.utils.claw4_ollama_prewarm": mod}):
            bind = self._connection(True); init(bind)
            self.assertTrue(bind.bound); self.assertEqual(calls, [])
            self.assertFalse(hasattr(bind, "selected_module_str"))
            regular = self._connection(); init(regular)
            self.assertEqual(calls, [regular.llm]); self.assertEqual(regular.selected_module_str, "modules")
            mod.schedule_ollama_prewarm = lambda *_: (_ for _ in ()).throw(RuntimeError("private"))
            failed = self._connection(); init(failed)
            self.assertEqual(failed.selected_module_str, "modules")

    def test_target_validation_and_unknown_provider(self):
        self.assertEqual(helper._native_base("http://host.docker.internal:11434/v1"),
                         "http://host.docker.internal:11434")
        for raw in ("http://foreign:11434/v1", "http://host.docker.internal:11435/v1",
                    "http://u:p@host.docker.internal:11434/v1", "http://host.docker.internal:11434/x",
                    "http://host.docker.internal:11434/v1?x=1",
                    "http://host.docker.internal:11434/v1#x", "https://host.docker.internal:11434"):
            self.assertIsNone(helper._native_base(raw))
        with patch.object(helper.threading, "Thread") as thread:
            self.assertFalse(helper.schedule_ollama_prewarm(object()))
            self.assertFalse(helper.schedule_ollama_prewarm(types.SimpleNamespace(model_name=helper.MODEL,
                                                      base_url="http://foreign:11434/v1")))
            Provider = type("LLMProvider", (), {"__module__":"core.providers.llm.ollama.ollama"})
            wrong = Provider(); wrong.model_name = "other:2b"
            wrong.base_url = "http://host.docker.internal:11434/v1"
            self.assertFalse(helper.schedule_ollama_prewarm(wrong))
            thread.assert_not_called()

    def test_resident_missing_digest_and_exact_empty_request(self):
        records = []
        def stage(*args, **kwargs): records.append((args[0], kwargs))
        with patch.object(helper, "_stage", side_effect=stage):
            with patch.object(helper, "_json_request", return_value={"models":[{"name":helper.MODEL}]}) as request:
                helper._work("http://host.docker.internal:11434", helper.MODEL)
                self.assertEqual(request.call_count,1); self.assertEqual(records[-1][0],"resident")
            with patch.object(helper, "_json_request", side_effect=[{"models":[]},{"models":[]}]) as request:
                helper._work("http://host.docker.internal:11434", helper.MODEL)
                self.assertEqual(request.call_count,2); self.assertEqual(records[-1][0],"model_unavailable")
            with patch.object(helper, "_json_request", side_effect=[{"models":[]},
                         {"models":[{"name":helper.MODEL,"digest":"wrong"}]}]) as request:
                helper._work("http://host.docker.internal:11434", helper.MODEL)
                self.assertEqual(request.call_count,2); self.assertEqual(records[-1][0],"model_unavailable")
            with patch.object(helper, "_json_request", side_effect=[{"models":[]},
                         {"models":[{"name":helper.MODEL,"digest":helper.MODEL_DIGEST}]},
                         {"done":True,"load_duration":2_000_000}]) as request:
                helper._work("http://host.docker.internal:11434", helper.MODEL)
                self.assertEqual(request.call_args.args[1:],("/api/generate",
                                 {"model":helper.MODEL,"prompt":"","stream":False}))
                self.assertEqual(records[-1],("preloaded",{"load_ms":2.0}))

    def test_singleflight_nonblocking_and_release_after_error(self):
        Provider = type("LLMProvider", (), {"__module__":"core.providers.llm.ollama.ollama"})
        provider = Provider(); provider.model_name = helper.MODEL
        provider.base_url = "http://host.docker.internal:11434/v1"
        started = threading.Event(); release = threading.Event()
        def stalled(*_): started.set(); release.wait(2); raise RuntimeError("private token")
        statuses=[]
        with patch.object(helper,"_json_request",side_effect=stalled), \
             patch.object(helper,"_stage",side_effect=lambda *args,**kw: statuses.append((args,kw))):
            self.assertTrue(helper.schedule_ollama_prewarm(provider))
            self.assertTrue(started.wait(1))
            self.assertFalse(helper.schedule_ollama_prewarm(provider))
            release.set()
            for _ in range(100):
                if not helper._in_flight: break
                threading.Event().wait(.01)
            self.assertFalse(helper._in_flight)
            self.assertEqual(statuses[-1][0][0],"error")
            self.assertEqual(statuses[-1][1]["error_class"],"RuntimeError")
            self.assertNotIn("private token",str(statuses))
            self.assertTrue(helper.schedule_ollama_prewarm(provider))
            for _ in range(100):
                if not helper._in_flight: break
                threading.Event().wait(.01)
            self.assertFalse(helper._in_flight)

    def test_bounded_response_and_no_redirect(self):
        class Response:
            status = 200
            def __enter__(self): return self
            def __exit__(self, *_): pass
            def read(self, count):
                self.assert_count = count
                return b"x" * count
        response=Response()
        with patch.object(helper._opener,"open",return_value=response) as request:
            with self.assertRaises(ValueError): helper._json_request("http://host.docker.internal:11434","/api/ps")
            self.assertEqual(response.assert_count,helper.LIMIT+1)
            self.assertEqual(request.call_args.kwargs["timeout"],helper.PROBE_TIMEOUT)
        self.assertIsNone(helper._NoRedirect().redirect_request(None,None,302,"",{},"http://foreign"))

    def test_valid_cold_preload_uses_longer_transport_timeout(self):
        class Response:
            status = 200
            def __init__(self, data): self.data = data
            def __enter__(self): return self
            def __exit__(self, *_): pass
            def read(self, size):
                self_outer.assertEqual(size, helper.LIMIT + 1)
                return json.dumps(self.data).encode("utf-8")
        self_outer = self
        calls = []
        def open_request(request, timeout):
            path = request.full_url.rsplit("/api/",1)[-1]
            calls.append((path, timeout))
            if path == "ps": return Response({"models":[]})
            if path == "tags":
                return Response({"models":[{"name":helper.MODEL,"digest":helper.MODEL_DIGEST}]})
            self.assertEqual(path,"generate")
            if timeout < 5.6: raise TimeoutError("valid cold load exceeded timeout")
            self.assertEqual(json.loads(request.data),
                             {"model":helper.MODEL,"prompt":"","stream":False})
            return Response({"done":True,"load_duration":4_962_000_000})
        statuses=[]
        with patch.object(helper._opener,"open",side_effect=open_request), \
             patch.object(helper,"_stage",side_effect=lambda *args,**kw: statuses.append((args[0],kw))):
            helper._work("http://host.docker.internal:11434",helper.MODEL)
        self.assertEqual(calls,[("ps",3.0),("tags",3.0),("generate",15.0)])
        self.assertEqual(statuses,[("preloaded",{"load_ms":4962.0})])
        self.assertFalse(helper._in_flight)

    def test_status_logging_is_metadata_only(self):
        messages=[]
        with patch.object(helper._log,"info",side_effect=lambda *args:messages.append(args)):
            helper._stage("error",helper.time.monotonic(),error_class="RuntimeError")
        self.assertEqual(len(messages),1)
        self.assertIn('"status":"error"',messages[0][1])
        self.assertNotIn("host.docker.internal",str(messages))

    def test_deployment_context_and_overlay(self):
        expected=["**","!Dockerfile.edge-tts","!Dockerfile.voice","!Dockerfile.asr-voice",
                  "!edge-tts.requirements.txt","!patch_ollama_voice.py","!patch_asr_voice.py",
                  "!claw4-m1-voice-prompt.txt","!Dockerfile.prewarm","!patch_ollama_prewarm.py",
                  "!claw4_ollama_prewarm.py"]
        self.assertEqual((DEPLOY/".dockerignore").read_text().splitlines(),expected)
        docker=(DEPLOY/"Dockerfile.prewarm").read_text()
        for item in ("server_0.9.6@sha256:9cf52d6b79c9d157356aba2c8bf1db645c2d2db4f14720c28128920020addccc",
                     "--require-hashes","patch_ollama_voice.py","patch_asr_voice.py",
                     "patch_ollama_prewarm.py","claw4_ollama_prewarm.py","claw4-m1-voice-prompt.txt"):
            self.assertIn(item,docker)
        compose=(DEPLOY/"compose.prewarm.yaml").read_text()
        self.assertIn("image: claw4-xiaozhi-v6:voice-prewarm-20261106-01",compose)
        self.assertIn("dockerfile: Dockerfile.prewarm",compose)
        self.assertNotIn("extra_hosts",compose)
        self.assertNotIn("ports:",compose)


if __name__ == "__main__": unittest.main()
