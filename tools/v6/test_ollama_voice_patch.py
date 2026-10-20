from __future__ import annotations

import hashlib
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import types
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
PATCH_SCRIPT = ROOT / "deploy/v6/xiaozhi-nas/patch_ollama_voice.py"
FIXTURE = Path(__file__).resolve().parent / "fixtures/ollama_provider_v096.py"
EXPECTED_SHA256 = "0ce07df5f7f72b9e3a4ae4086eb62b4cc94f75ac5526ddda41a6600f335b6aa6"


def load_patch_module():
    spec = importlib.util.spec_from_file_location("ollama_voice_patch", PATCH_SCRIPT)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


class FakeLogger:
    def bind(self, **_kwargs):
        return self

    def debug(self, *_args, **_kwargs):
        pass

    def error(self, *_args, **_kwargs):
        pass


class FakeStream:
    def __init__(self, chunks=(), error=None):
        self.chunks = chunks
        self.error = error
        self.closed = False

    def __iter__(self):
        if self.error:
            raise self.error
        return iter(self.chunks)

    def close(self):
        self.closed = True


class FakeCompletions:
    def __init__(self):
        self.calls = []
        self.stream = FakeStream()

    def create(self, **kwargs):
        self.calls.append(kwargs)
        return self.stream


class FakeOpenAI:
    instances = []

    def __init__(self, **kwargs):
        self.constructor_kwargs = kwargs
        self.chat = types.SimpleNamespace(completions=FakeCompletions())
        self.__class__.instances.append(self)


def make_chunk(content=None, tool_calls=None):
    return types.SimpleNamespace(
        choices=[
            types.SimpleNamespace(
                delta=types.SimpleNamespace(content=content, tool_calls=tool_calls)
            )
        ]
    )


class OllamaVoicePatchTests(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.TemporaryDirectory()
        self.patched_source = Path(self.tempdir.name) / "ollama.py"
        self.patched_source.write_bytes(FIXTURE.read_bytes())
        load_patch_module().patch(self.patched_source)
        compile(self.patched_source.read_bytes(), str(self.patched_source), "exec")

        self.saved_modules = {}
        fake_logger_module = types.ModuleType("config.logger")
        fake_logger_module.setup_logging = lambda: FakeLogger()
        fake_openai_module = types.ModuleType("openai")
        fake_openai_module.OpenAI = FakeOpenAI
        fake_base_module = types.ModuleType("core.providers.llm.base")
        fake_base_module.LLMProviderBase = type("LLMProviderBase", (), {})
        modules = {
            "config": types.ModuleType("config"),
            "config.logger": fake_logger_module,
            "openai": fake_openai_module,
            "core": types.ModuleType("core"),
            "core.providers": types.ModuleType("core.providers"),
            "core.providers.llm": types.ModuleType("core.providers.llm"),
            "core.providers.llm.base": fake_base_module,
        }
        for name, module in modules.items():
            self.saved_modules[name] = sys.modules.get(name)
            sys.modules[name] = module

        spec = importlib.util.spec_from_file_location("patched_ollama_fixture", self.patched_source)
        self.provider_module = importlib.util.module_from_spec(spec)
        assert spec.loader is not None
        spec.loader.exec_module(self.provider_module)
        FakeOpenAI.instances.clear()

    def tearDown(self):
        for name, module in self.saved_modules.items():
            if module is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = module
        self.tempdir.cleanup()

    def make_provider(self, model_name):
        provider = self.provider_module.LLMProvider(
            {"model_name": model_name, "base_url": "http://ollama:11434"}
        )
        client = FakeOpenAI.instances[-1]
        self.assertEqual(client.constructor_kwargs["timeout"], 20.0)
        self.assertEqual(client.constructor_kwargs["max_retries"], 0)
        return provider, client.chat.completions

    def test_qwen35_response_uses_bounded_non_reasoning_request_and_preserves_history(self):
        provider, completions = self.make_provider("qwen3.5:2b")
        history = [{"role": "user", "content": "hello"}]
        completions.stream = FakeStream([make_chunk("Hello")])

        self.assertEqual(list(provider.response("session", history)), ["Hello"])
        self.assertEqual(history, [{"role": "user", "content": "hello"}])
        call = completions.calls[0]
        self.assertEqual(call["reasoning_effort"], "none")
        self.assertEqual(call["temperature"], 0.3)
        self.assertEqual(call["max_tokens"], 128)
        self.assertEqual(call["messages"], history)
        self.assertTrue(completions.stream.closed)

    def test_legacy_qwen3_prefix_is_deepcopied_and_api_reasoning_is_disabled(self):
        provider, completions = self.make_provider("qwen3:8b")
        history = [
            {"role": "assistant", "content": "prior"},
            {"role": "user", "content": "hello"},
        ]
        completions.stream = FakeStream([make_chunk("ok")])

        self.assertEqual(list(provider.response("session", history)), ["ok"])
        self.assertEqual(history[-1]["content"], "hello")
        call = completions.calls[0]
        self.assertEqual(call["messages"][-1]["content"], "/no_think hello")
        self.assertIsNot(call["messages"], history)
        self.assertEqual(call["reasoning_effort"], "none")

    def test_non_qwen_has_no_reasoning_effort(self):
        provider, completions = self.make_provider("llama3.2:3b")
        completions.stream = FakeStream([make_chunk("ok")])

        self.assertEqual(list(provider.response("session", [{"role": "user", "content": "hi"}])), ["ok"])
        call = completions.calls[0]
        self.assertNotIn("reasoning_effort", call)
        self.assertEqual(call["temperature"], 0.3)
        self.assertEqual(call["max_tokens"], 128)

    def test_function_stream_preserves_tools_content_tuple_and_close(self):
        provider, completions = self.make_provider("qwen3.5:2b")
        tool_calls = [types.SimpleNamespace(id="call-1")]
        completions.stream = FakeStream([make_chunk(tool_calls=tool_calls), make_chunk("done")])
        functions = [{"type": "function", "function": {"name": "finish"}}]

        self.assertEqual(
            list(provider.response_with_functions("session", [{"role": "user", "content": "go"}], functions)),
            [(None, tool_calls), ("done", None)],
        )
        call = completions.calls[0]
        self.assertEqual(call["tools"], functions)
        self.assertEqual(call["reasoning_effort"], "none")
        self.assertTrue(completions.stream.closed)

    def test_stream_close_runs_when_iteration_raises(self):
        provider, completions = self.make_provider("qwen3.5:2b")
        completions.stream = FakeStream(error=RuntimeError("synthetic stream failure"))

        with self.assertRaisesRegex(RuntimeError, "synthetic stream failure"):
            list(provider.response("session", [{"role": "user", "content": "hi"}]))
        self.assertTrue(completions.stream.closed)

    def test_fixture_hash_and_patch_fail_closed_on_wrong_or_already_patched_source(self):
        self.assertEqual(hashlib.sha256(FIXTURE.read_bytes()).hexdigest(), EXPECTED_SHA256)
        patcher = load_patch_module()
        with tempfile.TemporaryDirectory() as tmp:
            wrong = Path(tmp) / "wrong.py"
            wrong.write_text("changed source\n", encoding="utf-8")
            before = wrong.read_bytes()
            with self.assertRaisesRegex(ValueError, "does not match expected"):
                patcher.patch(wrong)
            self.assertEqual(wrong.read_bytes(), before)

            patched = Path(tmp) / "patched.py"
            patched.write_bytes(FIXTURE.read_bytes())
            patcher.patch(patched)
            after_first_patch = patched.read_bytes()
            with self.assertRaisesRegex(ValueError, "does not match expected"):
                patcher.patch(patched)
            self.assertEqual(patched.read_bytes(), after_first_patch)

    def test_docker_context_and_overlay_are_narrow(self):
        deploy = ROOT / "deploy/v6/xiaozhi-nas"
        dockerignore = (deploy / ".dockerignore").read_text(encoding="utf-8").splitlines()
        self.assertEqual(
            dockerignore,
            ["**", "!Dockerfile.edge-tts", "!Dockerfile.voice", "!edge-tts.requirements.txt", "!patch_ollama_voice.py"],
        )
        dockerfile = (deploy / "Dockerfile.voice").read_text(encoding="utf-8")
        self.assertIn("server_0.9.6@sha256:9cf52d6b79c9d157356aba2c8bf1db645c2d2db4f14720c28128920020addccc", dockerfile)
        self.assertIn("--no-deps", dockerfile)
        self.assertIn("--require-hashes", dockerfile)
        self.assertIn("--only-binary=:all:", dockerfile)
        self.assertIn("version == '7.3.1'", dockerfile)
        self.assertIn("patch_ollama_voice.py /opt/xiaozhi-esp32-server/core/providers/llm/ollama/ollama.py", dockerfile)
        compose = (deploy / "compose.voice-fix.yaml").read_text(encoding="utf-8")
        self.assertIn("image: claw4-xiaozhi-v6:voice-20261020-01", compose)
        self.assertIn("dockerfile: Dockerfile.voice", compose)
        self.assertNotIn("ports:", compose)
        self.assertNotIn("volumes:", compose)
        self.assertEqual(subprocess.run([sys.executable, "--version"], capture_output=True).returncode, 0)


if __name__ == "__main__":
    unittest.main()
