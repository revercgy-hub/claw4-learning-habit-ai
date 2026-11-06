"""Bounded, privacy-safe timing probe for the real NAS provider chain.

This tool is intended to run in the isolated NAS probe container. It never
accepts user text: every LLM request uses the fixed synthetic question below.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import unicodedata
import math
import contextlib
import io
from pathlib import Path
import subprocess
import sys
import time
from types import SimpleNamespace

VERSION = 1
QUESTION = "一加一等于几？"
MODEL = "qwen3.5:2b"
LLM_TYPE = "ollama"
TTS_TYPE = "edge"
SOURCE_HASHES = {
    "ollama": "434751da49b9baf2aa6a1726c2f3202ad79b06a96e7ee27110268366580e22f9",
    "edge": "1b1f46e58a5363d104a363ad1fcf7d3fccc999910e208537c6bafb14664a0281",
    "base": "be741215966d4419a471206d6f8dd9afd7d29dab9bd455bc7b0e5eb10e2c9ca8",
    "prompt": "72d4417d61ee6bcdd5c6361bedcf7659031c1a44276d5743b6af40f070f55f45",
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _safe_relation(answer: str) -> str:
    compact = unicodedata.normalize("NFKC", answer).translate(str.maketrans("零一二三四五六七八九", "0123456789"))
    compact = compact.replace("加", "+").replace("等于", "=")
    compact = re.sub(r"[\s，。？！,?!:：是]+", "", compact)
    return "expected_match" if compact in {"1+1=2", "2", "二"} else "no_match"


def selected_provider_configs(config):
    selected=config["selected_module"]
    return config["LLM"][selected["LLM"]], config["TTS"][selected["TTS"]]


def profile_runtime(llm, tts, decoder, question, system_prompt, clock=time.monotonic):
    t0=clock(); first=None; chunks=[]
    for chunk in llm.response("l28-synthetic", [{"role":"system","content":system_prompt}, {"role":"user","content":question}]):
        if first is None: first=clock()
        chunks.append(str(chunk))
    complete=clock(); answer="".join(chunks)
    if not answer.strip(): raise ValueError("empty_llm")
    metrics={"llm_first_yield_ms":(first-t0)*1000,"llm_complete_ms":(complete-t0)*1000,
             "generated_chars":len(answer),"generated_sha256":digest(answer.encode()),
             "expected_relation":_safe_relation(answer),"tts_request_count":0,
             "tts_first_mp3_chunk_ms":None,"tts_request_ms":None,"opus_first_frame_ms":None,
             "opus_frames":0,"opus_bytes":0,"opus_sha256":None,"opus_decoded_samples":0,
             "opus_decode_error_class":None}
    started=clock(); audio_first=None; opus_first=None
    # Instrument the actual edge_tts async generator, while delegating every chunk.
    from core.providers.tts import edge as edge_module
    communicate_type=edge_module.edge_tts.Communicate
    original_stream=communicate_type.stream
    async def measured_stream(instance, *args, **kwargs):
        nonlocal audio_first
        async for item in original_stream(instance,*args,**kwargs):
            if item.get("type")=="audio" and audio_first is None: audio_first=clock()
            yield item
    communicate_type.stream=measured_stream
    req_times=[]; original_tts=tts.text_to_speak
    async def measured_tts(*args,**kwargs):
        req_times.append(clock())
        try: return await original_tts(*args,**kwargs)
        finally: req_times.append(clock())
    tts.text_to_speak=measured_tts
    frames=[]
    def on_frame(frame):
        nonlocal opus_first
        if opus_first is None: opus_first=clock()
        frames.append(bytes(frame))
    try:
        tts.to_tts_stream(answer, on_frame)
    finally:
        communicate_type.stream=original_stream
        tts.text_to_speak=original_tts
    metrics["tts_request_count"]=len(req_times)//2
    if not frames or not req_times or audio_first is None: raise ValueError("empty_tts_stage")
    metrics["tts_first_mp3_chunk_ms"]=None if audio_first is None else (audio_first-started)*1000
    metrics["tts_request_ms"]=(req_times[-1]-started)*1000
    metrics["opus_first_frame_ms"]=(opus_first-started)*1000
    blob=b"".join(frames); metrics["opus_frames"]=len(frames); metrics["opus_bytes"]=len(blob); metrics["opus_sha256"]=digest(blob)
    samples=0; decoder_errors=[]
    for frame in frames:
        try:
            pcm = decoder.decode(frame, 960)
            if len(pcm) % 2: raise ValueError("odd_pcm")
            samples += len(pcm) // 2
        except Exception as exc: decoder_errors.append(type(exc).__name__)
    metrics["opus_decoded_samples"]=samples
    metrics["opus_decode_error_class"]=decoder_errors[0] if decoder_errors else None
    if decoder_errors or not samples: raise ValueError("opus_decode")
    for key,value in list(metrics.items()):
        if isinstance(value,float): metrics[key]=round(value,2)
    return metrics


def _worker(root: Path, prompt: Path) -> dict:
    """Run once inside child process; all provider failures are class-only."""
    sources = {
        "ollama": root / "core/providers/llm/ollama/ollama.py",
        "edge": root / "core/providers/tts/edge.py",
        "base": root / "core/providers/tts/base.py",
        "prompt": prompt,
    }
    try:
        hashes = {k: digest(p.read_bytes()) for k, p in sources.items()}
    except Exception as exc:
        return {"status":"SOURCE_UNAVAILABLE","error_class":type(exc).__name__}
    if any(hashes[k] != SOURCE_HASHES[k] for k in hashes):
        return {"status": "SOURCE_MISMATCH", "source_sha256": hashes}
    started = time.monotonic()
    try:
        # Keep upstream logging in this disposable child only. Remove loguru sinks
        # before importing providers so payload-bearing logs cannot reach files.
        from loguru import logger
        logger.remove()
        logger.disable("")
        from config.config_loader import load_config
        import asyncio
        config = asyncio.run(load_config())
        logger.remove()
        from core.utils.prompt_manager import PromptManager
        rendered = PromptManager(config).build_enhanced_prompt(config["prompt"], "l28-synthetic", None)
        llm_cfg,tts_cfg=selected_provider_configs(config)
        if len(rendered) != 177 or llm_cfg.get("type") != LLM_TYPE or tts_cfg.get("type") != TTS_TYPE:
            return {"status": "CONFIG_MISMATCH", "source_sha256": hashes,
                    "prompt_chars": len(rendered), "model_matches": False}
        model = llm_cfg.get("model_name") or llm_cfg.get("model")
        if model != MODEL:
            return {"status": "CONFIG_MISMATCH", "source_sha256": hashes,
                    "prompt_chars": len(rendered), "model_matches": False}
        from core.providers.llm.ollama.ollama import LLMProvider
        from core.providers.tts.edge import TTSProvider
        from core.utils.opus_encoder_utils import OpusEncoderUtils
        import opuslib_next
        provider=LLMProvider(llm_cfg); tts=TTSProvider(tts_cfg, True)
        tts.conn=SimpleNamespace(sample_rate=16000)
        tts.opus_encoder=OpusEncoderUtils(sample_rate=16000,channels=1,frame_size_ms=60)
        decoder=opuslib_next.Decoder(16000,1)
        metrics=profile_runtime(provider,tts,decoder,QUESTION,rendered)
        result={"status":"OK","source_sha256":hashes,"prompt_chars":len(rendered),"model_matches":True,
                **metrics,"tts_scope":"serialized_synthetic_provider_not_live_pipeline_or_device"}
        result["worker_elapsed_ms"] = round((time.monotonic()-started)*1000,2)
        return result
    except BaseException as exc:
        return {"status":"ERROR", "error_class":type(exc).__name__, "source_sha256":hashes,
                "worker_elapsed_ms":round((time.monotonic()-started)*1000,2)}


HASH_KEYS = set(SOURCE_HASHES)
OK_FIELDS = {"status","source_sha256","prompt_chars","model_matches","llm_first_yield_ms",
             "llm_complete_ms","generated_chars","generated_sha256","expected_relation",
             "tts_request_count","tts_first_mp3_chunk_ms","tts_request_ms","opus_first_frame_ms",
             "opus_frames","opus_bytes","opus_sha256","opus_decoded_samples",
             "opus_decode_error_class","tts_scope","worker_elapsed_ms"}


def _valid_hash(value):
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value) is not None


def _valid_elapsed(value):
    return type(value) in (int, float) and math.isfinite(value) and 0 <= value <= 60000


def _valid_worker_output(data):
    if not isinstance(data, dict): return False
    status = data.get("status")
    if status == "SOURCE_UNAVAILABLE":
        return (set(data) == {"status", "error_class"} and isinstance(data["error_class"], str)
                and re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]{0,79}", data["error_class"]) is not None)
    if status == "SOURCE_MISMATCH":
        return set(data) == {"status", "source_sha256"} and _valid_sources(data["source_sha256"])
    if status == "CONFIG_MISMATCH":
        return (set(data) == {"status", "source_sha256", "prompt_chars", "model_matches"}
                and _valid_sources(data["source_sha256"]) and data["source_sha256"] == SOURCE_HASHES
                and type(data["prompt_chars"]) is int
                and 0 <= data["prompt_chars"] <= 100000 and data["model_matches"] is False)
    if status == "ERROR":
        return (set(data) == {"status", "error_class", "source_sha256", "worker_elapsed_ms"}
                and _valid_sources(data["source_sha256"]) and data["source_sha256"] == SOURCE_HASHES
                and _valid_elapsed(data["worker_elapsed_ms"])
                and isinstance(data["error_class"], str) and re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]{0,79}", data["error_class"]) is not None)
    if status != "OK" or set(data) != OK_FIELDS: return False
    if not _valid_sources(data["source_sha256"]) or data["source_sha256"] != SOURCE_HASHES: return False
    if data["prompt_chars"] != 177 or data["model_matches"] is not True: return False
    if data["tts_scope"] != "serialized_synthetic_provider_not_live_pipeline_or_device": return False
    for key in ("llm_first_yield_ms","llm_complete_ms","tts_first_mp3_chunk_ms","tts_request_ms","opus_first_frame_ms","worker_elapsed_ms"):
        if not _valid_elapsed(data[key]): return False
    if not (data["llm_first_yield_ms"] <= data["llm_complete_ms"]
            and data["tts_first_mp3_chunk_ms"] <= data["tts_request_ms"]
            and data["tts_request_ms"] <= data["opus_first_frame_ms"]): return False
    for key in ("generated_chars","tts_request_count","opus_frames","opus_bytes","opus_decoded_samples"):
        if type(data[key]) is not int or data[key] <= 0: return False
    return (_valid_hash(data["generated_sha256"]) and _valid_hash(data["opus_sha256"])
            and data["expected_relation"] in {"expected_match","no_match"}
            and data["opus_decode_error_class"] is None)


def _valid_sources(value):
    return isinstance(value, dict) and set(value) == HASH_KEYS and all(_valid_hash(v) for v in value.values())


def run_trials(worker, trials: int, timeout: float):
    rows=[]
    for index in range(trials):
        started=time.monotonic()
        try:
            p=subprocess.run([sys.executable, str(Path(__file__).resolve()), "--_worker"],
                capture_output=True, text=True, timeout=timeout,
                env={**os.environ,"L28_UPSTREAM_ROOT":str(worker[0]),"L28_PROMPT_FILE":str(worker[1])})
        except subprocess.TimeoutExpired:
            rows.append({"trial":index+1,"status":"TIMEOUT","elapsed_ms":round((time.monotonic()-started)*1000,2)})
            continue
        try:
            data=json.loads(p.stdout)
            if p.returncode or not _valid_worker_output(data): raise ValueError()
        except Exception:
            rows.append({"trial":index+1,"status":"INVALID_WORKER_OUTPUT"})
            continue
        data["trial"]=index+1
        rows.append(data)
    return rows


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--trials",type=int,choices=range(1,4),default=1)
    ap.add_argument("--timeout-seconds",type=int,choices=range(1,61),default=45)
    ap.add_argument("--upstream-root",type=Path,default=Path(os.getenv("L28_UPSTREAM_ROOT","/opt/xiaozhi-esp32-server")))
    ap.add_argument("--prompt-file",type=Path,default=Path(os.getenv("L28_PROMPT_FILE","/opt/xiaozhi-esp32-server/claw4-m1-voice-prompt.txt")))
    ap.add_argument("--_worker",action="store_true",help=argparse.SUPPRESS)
    a=ap.parse_args()
    if a._worker:
        # Provider/config diagnostics may contain the utterance or private config.
        # Keep their stdout and stderr confined to this disposable process.
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            result = _worker(a.upstream_root,a.prompt_file)
        print(json.dumps(result,separators=(",",":")))
        return 0
    rows=run_trials((a.upstream_root,a.prompt_file),a.trials,a.timeout_seconds)
    print(json.dumps({"version":VERSION,"scope":"actual_provider_stage_profile_synthetic_only","results":rows},separators=(",",":")))
    return 0 if all(x["status"]=="OK" for x in rows) else 2

if __name__ == "__main__":
    raise SystemExit(main())
