"""Run the hash-guarded patch against the exact upstream ASR method."""

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile
import types
import unittest
from unittest.mock import patch as mock_patch


ROOT = Path(__file__).resolve().parents[2]
PATCH_SCRIPT = ROOT / "deploy/v6/xiaozhi-nas/patch_asr_voice.py"
FIXTURE = Path(__file__).resolve().parent / "fixtures/asr_base_v096.py"
EXPECTED_SHA256 = "929532c379d18598eff6fbecb991089f25a0c62cd77f74a25a0d1a5b8d13a713"


def load_patch_module():
    spec = importlib.util.spec_from_file_location("asr_voice_patch", PATCH_SCRIPT)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


class FakeLogger:
    def bind(self, **_kwargs):
        return self

    def info(self, *_args, **_kwargs):
        pass

    def debug(self, *_args, **_kwargs):
        pass

    def error(self, *_args, **_kwargs):
        pass


def load_asr_base(path: Path, name: str):
    module_names = (
        "config", "config.logger", "core", "core.providers", "core.providers.asr",
        "core.providers.asr.dto", "core.providers.asr.dto.dto", "core.handle",
        "core.handle.receiveAudioHandle", "core.handle.reportHandle", "core.utils",
        "core.utils.util",
    )
    modules = {key: types.ModuleType(key) for key in module_names}
    for key in module_names:
        modules[key].__path__ = []
    modules["config.logger"].setup_logging = FakeLogger
    modules["core.providers.asr.dto.dto"].InterfaceType = types.SimpleNamespace(STREAM=object())
    modules["core.handle.receiveAudioHandle"].startToChat = lambda *_args: None
    modules["core.handle.receiveAudioHandle"].handleAudioMessage = lambda *_args: None
    modules["core.handle.reportHandle"].enqueue_asr_report = lambda *_args: None
    modules["core.utils.util"].remove_punctuation_and_length = (
        lambda value: (len(re.sub(r"\W", "", value)), value)
    )
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    with mock_patch.dict(sys.modules, modules):
        spec.loader.exec_module(module)
    return module


async def run_pipeline(module, raw_text, provider_module="core.providers.asr.fun_local",
                       speaker=None, chat_error=False):
    events = []

    class Provider(module.ASRProviderBase):
        async def speech_to_text(self, _opus, _session, _artifacts=None):
            raise AssertionError("fake wrapper should be used")

        async def speech_to_text_wrapper(self, _audio, _session):
            if isinstance(raw_text, Exception):
                raise raw_text
            return raw_text, None

        def stop_ws_connection(self):
            events.append(("stop",))

    Provider.__module__ = provider_module

    async def identify_speaker(_wav, _session):
        return speaker

    async def start_to_chat(_conn, text):
        events.append(("chat", text))
        if chat_error:
            raise RuntimeError("synthetic chat failure")

    def enqueue(_conn, text, audio):
        events.append(("report", text, audio))

    module.startToChat = start_to_chat
    module.enqueue_asr_report = enqueue
    conn = types.SimpleNamespace(
        session_id="synthetic", voiceprint_provider=(
            types.SimpleNamespace(identify_speaker=identify_speaker) if speaker else None
        ),
    )
    await module.ASRProviderBase.handle_voice_stop(Provider(), conn, [b"\x00\x00"])
    return events


class ASRVoicePatchTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        self.tempdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tempdir.cleanup)
        self.source = Path(self.tempdir.name) / "base.py"
        shutil.copyfile(FIXTURE, self.source)
        self.assertEqual(hashlib.sha256(FIXTURE.read_bytes()).hexdigest(), EXPECTED_SHA256)
        self.baseline = load_asr_base(FIXTURE, "s20_baseline_asr")
        load_patch_module().patch(self.source)
        compile(self.source.read_bytes(), str(self.source), "exec")
        self.patched = load_asr_base(self.source, "s20_patched_asr")

    async def compare_unchanged(self, raw, **kwargs):
        old = await run_pipeline(self.baseline, raw.copy() if isinstance(raw, dict) else raw, **kwargs)
        new = await run_pipeline(self.patched, raw.copy() if isinstance(raw, dict) else raw, **kwargs)
        self.assertEqual(new, old)
        return new

    async def test_selected_envelope_changes_chat_only(self):
        raw = {"content": "synthetic arithmetic", "language": "zh", "emotion": "neutral"}
        old = await run_pipeline(self.baseline, raw.copy())
        new = await run_pipeline(self.patched, raw.copy())
        self.assertEqual(old[0], ("stop",))
        self.assertEqual(new[:2], old[:2])
        self.assertEqual(json.loads(new[1][1]), raw)
        self.assertEqual(new[2], ("chat", raw["content"]))
        self.assertEqual(old[2], ("chat", old[1][1]))

    async def test_unknown_provider_keeps_envelope(self):
        await self.compare_unchanged(
            {"content": "synthetic", "language": "zh", "emotion": "neutral"},
            provider_module="core.providers.asr.other",
        )

    async def test_extra_and_speaker_bearing_dictionaries_keep_envelope(self):
        for extra in ({"other": "x"}, {"speaker": "synthetic"}):
            with self.subTest(extra=extra):
                await self.compare_unchanged(
                    {"content": "synthetic", "language": "zh", "emotion": "neutral", **extra}
                )

    async def test_identified_speaker_keeps_envelope(self):
        raw = {"content": "synthetic", "language": "zh", "emotion": "neutral"}
        events = await self.compare_unchanged(raw.copy(), speaker="synthetic speaker")
        self.assertEqual(json.loads(events[1][1])["speaker"], "synthetic speaker")

    async def test_bad_metadata_and_empty_or_oversize_content_keep_envelope(self):
        for raw in (
            {"content": "synthetic", "language": None, "emotion": "neutral"},
            {"content": "synthetic", "language": "zh", "emotion": None},
            {"content": "", "language": "zh", "emotion": "neutral"},
            {"content": "x" * 4097, "language": "zh", "emotion": "neutral"},
        ):
            with self.subTest(content_length=len(raw["content"]), language=raw["language"], emotion=raw["emotion"]):
                await self.compare_unchanged(raw.copy())

    async def test_exact_maximum_content_unwraps(self):
        raw = {"content": "x" * 4096, "language": "zh", "emotion": "neutral"}
        events = await run_pipeline(self.patched, raw)
        self.assertEqual(events[2], ("chat", raw["content"]))

    async def test_bad_content_type_preserves_failure_path(self):
        await self.compare_unchanged({"content": 123, "language": "zh", "emotion": "neutral"})

    async def test_plain_text_and_exception_paths_unchanged(self):
        await self.compare_unchanged("synthetic plain speech")
        await self.compare_unchanged(RuntimeError("synthetic ASR failure"))

    async def test_chat_exception_preserves_prior_cleanup_and_report(self):
        raw = {"content": "synthetic", "language": "zh", "emotion": "neutral"}
        old = await run_pipeline(self.baseline, raw.copy(), chat_error=True)
        new = await run_pipeline(self.patched, raw.copy(), chat_error=True)
        self.assertEqual(new[:2], old[:2])
        self.assertEqual([entry[0] for entry in new], ["stop", "report", "chat"])
        self.assertEqual(new[2], ("chat", raw["content"]))

    async def test_hash_guard_rejects_mismatch_and_reapplication_without_write(self):
        patcher = load_patch_module()
        patched_bytes = self.source.read_bytes()
        with self.assertRaisesRegex(ValueError, "does not match expected"):
            patcher.patch(self.source)
        self.assertEqual(self.source.read_bytes(), patched_bytes)
        self.source.write_bytes(FIXTURE.read_bytes() + b"\n")
        wrong_bytes = self.source.read_bytes()
        with self.assertRaisesRegex(ValueError, "does not match expected"):
            patcher.patch(self.source)
        self.assertEqual(self.source.read_bytes(), wrong_bytes)


if __name__ == "__main__":
    unittest.main()
