"""Bounded per-connection guard for exact, recent TTS-to-ASR echo matches."""

import hashlib
import math
import unicodedata


_RECORDS_ATTR = "_claw4_echo_records"
_ACTIVE_ATTR = "_claw4_echo_active_sentence"
_GENERATION_ATTR = "_claw4_echo_generation"
_MAX_RECORDS = 8
_MAX_PCM_BYTES = 16_000 * 2 * 3
_PCM_BYTES_PER_SECOND = 16_000 * 2
_TAIL_SECONDS = 0.55
_MAX_CAPTURE_DELAY = 3.0


def _valid_time(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value) and value >= 0


def _sentence_key(sentence_id):
    if not isinstance(sentence_id, str):
        return None
    key = sentence_id.strip()
    return key if key else None


def _digest(text):
    if not isinstance(text, str):
        return None
    normalized = unicodedata.normalize("NFKC", text).casefold()
    normalized = "".join(
        char for char in normalized
        if not char.isspace() and not unicodedata.category(char).startswith(("P", "Z"))
    )
    if not 6 <= len(normalized) <= 180:
        return None
    return hashlib.sha256(normalized.encode("utf-8")).digest()


def _records(conn):
    records = getattr(conn, _RECORDS_ATTR, None)
    if not isinstance(records, list):
        records = []
        setattr(conn, _RECORDS_ATTR, records)
    return records


def record_text(conn, sentence_id, text, now=None):
    """Start a sentence association; store only its normalized text digest."""
    stamp = _time(now)
    key = _sentence_key(sentence_id)
    digest = _digest(text)
    generation = getattr(conn, _GENERATION_ATTR, 0) + 1
    setattr(conn, _GENERATION_ATTR, generation)
    # Starting malformed/short output deliberately detaches prior audio from it.
    setattr(conn, _ACTIVE_ATTR, None)
    if key is None or digest is None or stamp is None:
        return
    history = _records(conn)
    history[:] = [item for item in history if item["sentence_id"] != key]
    history.append({"sentence_id": key, "digest": digest, "generation": generation,
                    "first_send": None, "last_send": None, "finished": False})
    del history[:-_MAX_RECORDS]
    setattr(conn, _ACTIVE_ATTR, key)


def record_packet(conn, sentence_id, now=None):
    """Record one successfully sent Opus packet for the active sentence."""
    stamp = _time(now)
    key = _sentence_key(sentence_id)
    if stamp is None or key is None or getattr(conn, _ACTIVE_ATTR, None) != key:
        return
    for item in reversed(_records(conn)):
        if item["sentence_id"] == key and not item["finished"]:
            if item["first_send"] is None:
                item["first_send"] = stamp
            elif stamp < item["last_send"]:
                return
            item["last_send"] = stamp
            return


def finish_text(conn, sentence_id):
    """Stop packet association but retain the bounded record for recent capture."""
    key = _sentence_key(sentence_id)
    if key is None:
        return
    if getattr(conn, _ACTIVE_ATTR, None) == key:
        setattr(conn, _ACTIVE_ATTR, None)
    for item in reversed(_records(conn)):
        if item["sentence_id"] == key:
            item["finished"] = True
            return


def snapshot_echoes(conn, capture_end, pcm_bytes):
    """Freeze eligible digest set for this capture before asynchronous ASR."""
    if not _valid_time(capture_end):
        return frozenset()
    if not isinstance(pcm_bytes, int) or isinstance(pcm_bytes, bool) or not 0 < pcm_bytes <= _MAX_PCM_BYTES or pcm_bytes % 2:
        return frozenset()
    duration = pcm_bytes / _PCM_BYTES_PER_SECOND
    capture_start = capture_end - duration
    current_id = _sentence_key(getattr(conn, "sentence_id", None))
    if current_id is None:
        return frozenset()
    eligible = set()
    for item in _records(conn):
        first, last = item["first_send"], item["last_send"]
        if item["sentence_id"] != current_id or first is None or last is None:
            continue
        if not _valid_time(first) or not _valid_time(last) or last < first:
            continue
        if capture_end > last + _MAX_CAPTURE_DELAY:
            continue
        if capture_start <= last + _TAIL_SECONDS and capture_end >= first:
            eligible.add(item["digest"])
    return frozenset(eligible)


def matches_echo(text, snapshot):
    """Exact normalized digest membership only; never substring matching."""
    digest = _digest(text)
    return digest is not None and isinstance(snapshot, (set, frozenset)) and digest in snapshot


def _time(now):
    if now is None:
        import time
        now = time.monotonic()
    return now if _valid_time(now) else None
