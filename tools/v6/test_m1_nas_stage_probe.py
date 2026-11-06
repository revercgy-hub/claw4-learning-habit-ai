from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import asyncio
import types
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).with_name("m1_nas_stage_probe.py")
spec = importlib.util.spec_from_file_location("m1_nas_stage_probe", SCRIPT)
probe = importlib.util.module_from_spec(spec)
assert spec.loader
spec.loader.exec_module(probe)


class StageProbeTests(unittest.TestCase):
    def test_question_is_fixed_and_trial_bounds(self):
        self.assertEqual(probe.QUESTION, "一加一等于几？")
        self.assertEqual(probe.MODEL, "qwen3.5:2b")
        self.assertEqual(probe.TTS_TYPE, "edge")

    def test_selected_provider_config_shape(self):
        llm,tts=probe.selected_provider_configs({"selected_module":{"LLM":"ollama","TTS":"edge"},
            "LLM":{"ollama":{"type":"ollama","model_name":"qwen3.5:2b"}},
            "TTS":{"edge":{"type":"edge"}}})
        self.assertEqual(llm["type"],"ollama")
        self.assertEqual(tts["type"],"edge")

    def test_source_mismatch_fails_before_provider_import_or_request(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td); prompt=root/"prompt"; prompt.write_text("private prompt")
            for rel in ("core/providers/llm/ollama/ollama.py","core/providers/tts/edge.py","core/providers/tts/base.py"):
                p=root/rel; p.parent.mkdir(parents=True,exist_ok=True); p.write_text("wrong")
            result=probe._worker(root,prompt)
        self.assertEqual(result["status"],"SOURCE_MISMATCH")
        self.assertNotIn("private prompt",json.dumps(result))

    def test_timeout_is_a_terminal_trial_and_output_is_metadata_only(self):
        with patch.object(probe.subprocess,"run",side_effect=subprocess.TimeoutExpired("worker",45)) as run:
            rows=probe.run_trials((Path("/upstream"),Path("/prompt")),2,45)
        self.assertEqual([r["status"] for r in rows],["TIMEOUT","TIMEOUT"])
        self.assertEqual(run.call_count,2)

    def test_malformed_or_truncated_worker_output_rejected(self):
        bad=subprocess.CompletedProcess([],0,stdout='{"status":"OK"',stderr="secret")
        with patch.object(probe.subprocess,"run",return_value=bad):
            rows=probe.run_trials((Path("/u"),Path("/p")),1,45)
        self.assertEqual(rows,[{"trial":1,"status":"INVALID_WORKER_OUTPUT"}])
        self.assertNotIn("secret",json.dumps(rows))

    def test_nonzero_exit_rejected_even_with_success_json(self):
        good=subprocess.CompletedProcess([],1,stdout='{"status":"OK"}',stderr="private")
        with patch.object(probe.subprocess,"run",return_value=good):
            rows=probe.run_trials((Path("/u"),Path("/p")),1,45)
        self.assertEqual(rows[0]["status"],"INVALID_WORKER_OUTPUT")

    def test_success_path_runs_real_orchestrator_in_order_and_decodes_frames(self):
        timeline=[]
        ticks=iter([0.0,0.2,0.4,0.5,0.6,0.7,0.8,1.0,1.1,1.2])
        class LLm:
            def response(self,*_a):
                timeline.append("llm"); yield "一加一等于二。"
        class Decoder:
            def decode(self,frame,_size): timeline.append("decode"); return b"\0"*960
        class Communicate:
            async def stream(self):
                timeline.append("mp3")
                yield {"type":"audio","data":b"mp3"}
        class TTS:
            text_to_speak=None
            def to_tts_stream(self,text,callback):
                timeline.append("tts")
                asyncio.run(self.text_to_speak(text,None))
                callback(b"opus1"); callback(b"opus2")
        tts=TTS()
        async def text_to_speak(_text,_file):
            async for _chunk in Communicate().stream(): pass
            return b"mp3"
        tts.text_to_speak=text_to_speak
        fake_edge=types.ModuleType("core.providers.tts.edge"); fake_edge.edge_tts=types.SimpleNamespace(Communicate=Communicate)
        names=("core","core.providers","core.providers.tts")
        saved={n:__import__("sys").modules.get(n) for n in names+ ("core.providers.tts.edge",)}
        try:
            for n in names: __import__("sys").modules[n]=types.ModuleType(n)
            __import__("sys").modules["core.providers.tts.edge"]=fake_edge
            result=probe.profile_runtime(LLm(),tts,Decoder(),probe.QUESTION,lambda:next(ticks))
        finally:
            for n,v in saved.items():
                if v is None: __import__("sys").modules.pop(n,None)
                else: __import__("sys").modules[n]=v
        self.assertEqual(result["opus_frames"],2)
        self.assertEqual(result["opus_decoded_samples"],1920)
        self.assertEqual(result["expected_relation"],"expected_match")
        self.assertLess(timeline.index("llm"),timeline.index("tts"))
        self.assertLess(timeline.index("mp3"),timeline.index("decode"))

    def test_worker_schema_rejects_secret_unknown_keys(self):
        payload={"status":"OK","secret":"must-not-escape"}
        cp=subprocess.CompletedProcess([],0,stdout=json.dumps(payload),stderr="secret stderr")
        with patch.object(probe.subprocess,"run",return_value=cp):
            rows=probe.run_trials((Path("/u"),Path("/p")),1,45)
        self.assertEqual(rows[0]["status"],"INVALID_WORKER_OUTPUT")
        self.assertNotIn("secret",json.dumps(rows))


if __name__ == "__main__":
    unittest.main()
