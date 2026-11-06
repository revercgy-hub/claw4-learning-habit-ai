"""Bounded, privacy-safe timing probe for the real NAS provider chain.

This tool is intended to run in the isolated NAS probe container. It never
accepts user text: every LLM request uses the fixed synthetic question below.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

VERSION = 1
QUESTION = "What is 17 plus 25? Reply with one short sentence."
EXPECTED_ANSWER = "17 plus 25 is 42."
MODEL = "qwen3.5:2b"
LLM_TYPE = "ollama"
TTS_TYPE = "edge"
SOURCE_HASHES = {
    "ollama": "434751da49b9baf2aa6a1726c2f3202ad79b06a96e7ee27110268366580e22f9",
    "edge": "1b1f46e58a5363d104a363ad1fcf7d3fccc999910e208537c6bafb14664a0281",
    "base": "be741215966d4419a471206d6f8dd9afd7d29dab9bd455bc7b0e5eb10e2c9ca8",
    "tts_utils": "",  # Not used as a source guard: path is supplied by image layout.
    "prompt": "72d4417d61ee6bcdd5c6361bedcf7659031c1a44276d5743b6af40f070f55f45",
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _worker(root: Path, prompt: Path) -> dict:
    """Run once inside child process; all provider failures are class-only."""
    sources = {
        "ollama": root / "core/providers/llm/ollama/ollama.py",
        "edge": root / "core/providers/tts/edge.py",
        "base": root / "core/providers/tts/base.py",
        "prompt": prompt,
    }
    hashes = {k: digest(p.read_bytes()) for k, p in sources.items()}
    if any(hashes[k] != SOURCE_HASHES[k] for k in hashes):
        return {"status": "SOURCE_MISMATCH", "source_sha256": hashes}
    started = time.monotonic()
    try:
        # Keep upstream logging in this disposable child only. Remove loguru sinks
        # before importing providers so payload-bearing logs cannot reach files.
        from loguru import logger
        logger.remove()
        from config.config_loader import load_config
        import asyncio
        config = asyncio.run(load_config())
        from core.utils.prompt_manager import PromptManager
        rendered = PromptManager(config).build_enhanced_prompt(config["prompt"], "l28-synthetic", None)
        if len(rendered) != 177 or config.get("llm", {}).get("type") != LLM_TYPE or config.get("tts", {}).get("type") != TTS_TYPE:
            return {"status": "CONFIG_MISMATCH", "source_sha256": hashes,
                    "prompt_chars": len(rendered), "model_matches": False}
        model = config.get("llm", {}).get("model_name")
        if model != MODEL:
            return {"status": "CONFIG_MISMATCH", "source_sha256": hashes,
                    "prompt_chars": len(rendered), "model_matches": False}
        from core.providers.llm.ollama.ollama import LLMProvider
        provider = LLMProvider(config.get("llm"))
        t0 = time.monotonic()
        first = None
        pieces = []
        for part in provider.response("l28-synthetic", [{"role":"system","content":rendered},{"role":"user","content":QUESTION}]):
            if first is None:
                first = time.monotonic()
            pieces.append(str(part))
        ended = time.monotonic()
        answer = "".join(pieces)
        result = {"status":"OK", "source_sha256":hashes, "prompt_chars":len(rendered),
                  "model_matches":True, "llm_first_yield_ms":None if first is None else round((first-t0)*1000,2),
                  "llm_complete_ms":round((ended-t0)*1000,2), "generated_chars":len(answer),
                  "generated_sha256":digest(answer.encode()),
                  "expected_relation":"contains_42" if "42" in answer else "no_42",
                  "tts_first_mp3_chunk_ms":None,"opus_first_frame_ms":None,
                  "opus_frames":None,"opus_bytes":None,"opus_sha256":None,
                  "opus_decoded_samples":None,"opus_decode_error_class":None}
        # Keep the probe bounded and serial: real TTS receives only the first
        # completed sentence, falling back to the short full answer.
        sentence = next((s.strip() for s in answer.split(".") if s.strip()), answer.strip())
        if sentence:
            from core.providers.tts.edge import TTSProvider
            tts = TTSProvider(config.get("tts"), delete_audio_file=True)
            out = Path("/tmp/l28-stage-probe.mp3")
            tt0 = time.monotonic()
            asyncio.run(tts.text_to_speak(sentence, str(out)))
            result["tts_serialized_ms"] = round((time.monotonic()-tt0)*1000,2)
            result["tts_input_chars"] = len(sentence)
            result["tts_scope"] = "serialized_synthetic_provider_call_not_device_or_live_pipeline"
            try: out.unlink(missing_ok=True)
            except OSError: pass
        else:
            result["tts_serialized_ms"] = None
            result["tts_scope"] = "not_run_empty_llm_answer"
        result["worker_elapsed_ms"] = round((time.monotonic()-started)*1000,2)
        return result
    except BaseException as exc:
        return {"status":"ERROR", "error_class":type(exc).__name__, "source_sha256":hashes,
                "worker_elapsed_ms":round((time.monotonic()-started)*1000,2)}


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
            if p.returncode or not isinstance(data,dict) or data.get("status") not in {"OK","SOURCE_MISMATCH","CONFIG_MISMATCH","ERROR"}:
                raise ValueError()
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
    ap.add_argument("--prompt-file",type=Path,default=Path(os.getenv("L28_PROMPT_FILE","/opt/xiaozhi-esp32-server/config/prompts/claw4-m1-voice-prompt.txt")))
    ap.add_argument("--_worker",action="store_true",help=argparse.SUPPRESS)
    a=ap.parse_args()
    if a._worker:
        print(json.dumps(_worker(a.upstream_root,a.prompt_file),separators=(",",":")))
        return 0
    rows=run_trials((a.upstream_root,a.prompt_file),a.trials,a.timeout_seconds)
    print(json.dumps({"version":VERSION,"scope":"actual_provider_stage_profile_synthetic_only","results":rows},separators=(",",":")))
    return 0 if all(x["status"]=="OK" for x in rows) else 2

if __name__ == "__main__":
    raise SystemExit(main())
