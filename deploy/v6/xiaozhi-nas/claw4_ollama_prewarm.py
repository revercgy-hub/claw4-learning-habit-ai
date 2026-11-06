"""Private, best-effort preload for the one reviewed NAS Ollama deployment.

Socket timeout is per blocking operation, not a hard wall deadline. A slow
dripping peer can occupy the sole daemon worker, but cannot queue more workers.
"""
from __future__ import annotations

import json
import logging
import threading
import time
from urllib.parse import urlsplit
from urllib.request import HTTPRedirectHandler, ProxyHandler, Request, build_opener

MODEL = "qwen3.5:2b"
MODEL_DIGEST = "324d162be6ca5629ae4517c8710434d0bd2d665bc94dbad46e9af8fbf8a2f0df"
HOST = "host.docker.internal"
PORT = 11434
LIMIT = 65536
PROBE_TIMEOUT = 3.0
PRELOAD_TIMEOUT = 15.0
_lock = threading.Lock()
_in_flight = False
_log = logging.getLogger(__name__)


class _NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, msg, headers, newurl):
        return None


_opener = build_opener(ProxyHandler({}), _NoRedirect)


def _stage(status, started, error_class=None, load_ms=None):
    # Fixed status vocabulary and class names only: no URL, reply or exception text.
    elapsed = min(60000, max(0, round((time.monotonic() - started) * 1000)))
    fields = {"status": status, "elapsed_ms": elapsed}
    if error_class:
        fields["error_class"] = error_class[:80]
    if load_ms is not None:
        fields["load_ms"] = load_ms
    _log.info("claw4_ollama_prewarm %s", json.dumps(fields, separators=(",", ":")))


def _native_base(raw):
    if type(raw) is not str or len(raw) > 128:
        return None
    try:
        parts = urlsplit(raw)
        if (parts.scheme != "http" or parts.hostname != HOST or parts.port != PORT
                or parts.username is not None or parts.password is not None
                or parts.query or parts.fragment or parts.path not in ("", "/", "/v1")
                or parts.netloc != f"{HOST}:{PORT}"):
            return None
    except ValueError:
        return None
    return f"http://{HOST}:{PORT}"


def _json_request(base, path, payload=None):
    body = None if payload is None else json.dumps(payload, separators=(",", ":")).encode("ascii")
    request = Request(base + path, data=body,
                      headers={"Content-Type": "application/json"} if body is not None else {},
                      method="POST" if body is not None else "GET")
    timeout = PRELOAD_TIMEOUT if path == "/api/generate" else PROBE_TIMEOUT
    with _opener.open(request, timeout=timeout) as response:
        if response.status != 200:
            raise ValueError("http_status")
        raw = response.read(LIMIT + 1)
    if len(raw) > LIMIT:
        raise ValueError("response_too_large")
    data = json.loads(raw)
    if type(data) is not dict:
        raise ValueError("response_shape")
    return data


def _work(base, model):
    global _in_flight
    started = time.monotonic()
    try:
        resident = _json_request(base, "/api/ps").get("models")
        if type(resident) is not list:
            raise ValueError("ps_shape")
        if any(type(item) is dict and item.get("name") == model for item in resident):
            _stage("resident", started)
            return
        installed = _json_request(base, "/api/tags").get("models")
        if type(installed) is not list:
            raise ValueError("tags_shape")
        if not any(type(item) is dict and item.get("name") == model
                   and item.get("digest") == MODEL_DIGEST for item in installed):
            _stage("model_unavailable", started)
            return
        result = _json_request(base, "/api/generate", {"model": model, "prompt": "", "stream": False})
        if result.get("done") is not True:
            raise ValueError("generate_incomplete")
        load_ns = result.get("load_duration")
        load_ms = (round(load_ns / 1_000_000, 2) if type(load_ns) is int
                   and 0 <= load_ns <= 60_000_000_000 else None)
        _stage("preloaded", started, load_ms=load_ms)
    except Exception as exc:
        _stage("error", started, error_class=type(exc).__name__)
    finally:
        with _lock:
            _in_flight = False


def schedule_ollama_prewarm(provider):
    """Schedule at most one process-wide worker; return immediately."""
    global _in_flight
    try:
        if (type(provider).__module__ != "core.providers.llm.ollama.ollama"
                or type(provider).__name__ != "LLMProvider"):
            return False
        model = provider.model_name
        base = _native_base(provider.base_url)
        if model != MODEL or base is None:
            return False
        with _lock:
            if _in_flight:
                return False
            _in_flight = True
        try:
            threading.Thread(target=_work, args=(base, model), daemon=True,
                             name="claw4-ollama-prewarm").start()
        except Exception:
            with _lock:
                _in_flight = False
            return False
        return True
    except Exception:
        return False
