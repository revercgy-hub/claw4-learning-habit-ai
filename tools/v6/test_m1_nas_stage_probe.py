from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import asyncio
import types
import sys
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
            def response(self,_id,messages):
                self.assert_messages=messages
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
            llm=LLm()
            result=probe.profile_runtime(llm,tts,Decoder(),probe.QUESTION,"rendered system prompt",lambda:next(ticks))
        finally:
            for n,v in saved.items():
                if v is None: __import__("sys").modules.pop(n,None)
                else: __import__("sys").modules[n]=v
        self.assertEqual(result["opus_frames"],2)
        self.assertEqual(result["opus_decoded_samples"],960)
        self.assertEqual(result["expected_relation"],"expected_match")
        self.assertEqual(llm.assert_messages,[{"role":"system","content":"rendered system prompt"},
                                             {"role":"user","content":probe.QUESTION}])
        self.assertLess(timeline.index("llm"),timeline.index("tts"))
        self.assertLess(timeline.index("mp3"),timeline.index("decode"))

    def test_worker_schema_rejects_secret_unknown_keys(self):
        payload={"status":"OK","secret":"must-not-escape"}
        cp=subprocess.CompletedProcess([],0,stdout=json.dumps(payload),stderr="secret stderr")
        with patch.object(probe.subprocess,"run",return_value=cp):
            rows=probe.run_trials((Path("/u"),Path("/p")),1,45)
        self.assertEqual(rows[0]["status"],"INVALID_WORKER_OUTPUT")
        self.assertNotIn("secret",json.dumps(rows))

    def test_parent_accepts_complete_worker_result(self):
        hashes=dict(probe.SOURCE_HASHES)
        data={"status":"OK","source_sha256":hashes,"prompt_chars":177,"model_matches":True,
              "llm_first_yield_ms":1.0,"llm_complete_ms":2.0,"generated_chars":8,
              "generated_sha256":"a"*64,"expected_relation":"expected_match",
              "tts_request_count":1,"tts_first_mp3_chunk_ms":3.0,"tts_request_ms":5.0,
              "opus_first_frame_ms":6.0,"opus_frames":2,"opus_bytes":20,
              "opus_sha256":"b"*64,"opus_decoded_samples":960,
              "opus_decode_error_class":None,"tts_scope":"serialized_synthetic_provider_not_live_pipeline_or_device",
              "worker_elapsed_ms":8.0}
        cp=subprocess.CompletedProcess([],0,stdout=json.dumps(data),stderr="secret")
        with patch.object(probe.subprocess,"run",return_value=cp):
            self.assertEqual(probe.run_trials((Path("/u"),Path("/p")),1,45),[{**data,"trial":1}])
        for key,value in (("worker_elapsed_ms",float("nan")),("opus_frames",True),
                          ("generated_sha256","secret"),("tts_first_mp3_chunk_ms",None),
                          ("source_sha256",{"ollama":"a"*64})):
            bad={**data,key:value}
            with patch.object(probe.subprocess,"run",return_value=subprocess.CompletedProcess([],0,stdout=json.dumps(bad),stderr="secret")):
                self.assertEqual(probe.run_trials((Path("/u"),Path("/p")),1,45),
                                 [{"trial":1,"status":"INVALID_WORKER_OUTPUT"}])

    def test_failure_statuses_have_exact_metadata_schema(self):
        for data in ({"status":"SOURCE_UNAVAILABLE","error_class":"FileNotFoundError","secret":"x"},
                     {"status":"CONFIG_MISMATCH","source_sha256":dict(probe.SOURCE_HASHES),
                      "prompt_chars":177,"model_matches":False,"error_class":"x"},
                     {"status":"ERROR","source_sha256":dict(probe.SOURCE_HASHES),
                      "worker_elapsed_ms":1.0,"error_class":"Private secret"}):
            with patch.object(probe.subprocess,"run",return_value=subprocess.CompletedProcess([],0,stdout=json.dumps(data),stderr="secret")):
                self.assertEqual(probe.run_trials((Path("/u"),Path("/p")),1,45),
                                 [{"trial":1,"status":"INVALID_WORKER_OUTPUT"}])

    def test_worker_config_and_parent_roundtrip(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td); prompt=root/"claw4-m1-voice-prompt.txt"
            files={"ollama":root/"core/providers/llm/ollama/ollama.py",
                   "edge":root/"core/providers/tts/edge.py",
                   "base":root/"core/providers/tts/base.py","prompt":prompt}
            for key,path in files.items():
                path.parent.mkdir(parents=True,exist_ok=True); path.write_text(key)
            hashes={key:probe.digest(path.read_bytes()) for key,path in files.items()}
            config={"selected_module":{"LLM":"ollama","TTS":"edge"},
                    "LLM":{"ollama":{"type":"ollama","model_name":probe.MODEL}},
                    "TTS":{"edge":{"type":"edge"}},"prompt":"private config"}
            def module(name,**items):
                m=types.ModuleType(name); m.__dict__.update(items); return m
            async def load_config(): return config
            class PromptManager:
                def __init__(self,c): self.c=c
                def build_enhanced_prompt(self,*_): return "s"*177
            class Provider:
                def __init__(self,*_,**__): pass
            names={"loguru":module("loguru",logger=types.SimpleNamespace(remove=lambda:None,disable=lambda _:None)),
                   "config":module("config"),"config.config_loader":module("config.config_loader",load_config=load_config),
                   "core":module("core"),"core.utils":module("core.utils"),
                   "core.utils.prompt_manager":module("core.utils.prompt_manager",PromptManager=PromptManager),
                   "core.utils.opus_encoder_utils":module("core.utils.opus_encoder_utils",OpusEncoderUtils=Provider),
                   "core.providers":module("core.providers"),"core.providers.llm":module("core.providers.llm"),
                   "core.providers.llm.ollama":module("core.providers.llm.ollama"),
                   "core.providers.llm.ollama.ollama":module("core.providers.llm.ollama.ollama",LLMProvider=Provider),
                   "core.providers.tts":module("core.providers.tts"),
                   "core.providers.tts.edge":module("core.providers.tts.edge",TTSProvider=Provider),
                   "opuslib_next":module("opuslib_next",Decoder=Provider)}
            metrics={"llm_first_yield_ms":1.0,"llm_complete_ms":2.0,"generated_chars":2,
                     "generated_sha256":"a"*64,"expected_relation":"expected_match",
                     "tts_request_count":1,"tts_first_mp3_chunk_ms":3.0,"tts_request_ms":5.0,
                     "opus_first_frame_ms":6.0,"opus_frames":1,"opus_bytes":5,
                     "opus_sha256":"b"*64,"opus_decoded_samples":480,"opus_decode_error_class":None}
            with patch.dict(sys.modules,names),patch.object(probe,"SOURCE_HASHES",hashes), \
                 patch.object(probe,"profile_runtime",return_value=metrics) as runtime:
                worker=probe._worker(root,prompt)
                self.assertEqual(worker["status"],"OK",worker)
                self.assertEqual(runtime.call_args.args[3:],(probe.QUESTION,"s"*177))
                cp=subprocess.CompletedProcess([],0,stdout=json.dumps(worker),stderr="private config")
                with patch.object(probe.subprocess,"run",return_value=cp):
                    rows=probe.run_trials((root,prompt),1,45)
                self.assertEqual(rows[0]["status"],"OK")
                self.assertNotIn("private config",json.dumps(rows))


if __name__ == "__main__":
    unittest.main()
