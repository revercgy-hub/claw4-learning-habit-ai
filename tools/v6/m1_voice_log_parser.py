"""Fail-closed extraction of a small allowlist of existing Claw4 UART markers."""
import argparse
import json
import re
from pathlib import Path

LINE = re.compile(r"^[IWEVD] \((\d+)\) (.*)$")
ANCHOR = re.compile(r"CANDIDATE_ELF_SHA256=([0-9a-fA-F]{9,64})")
HEALTH = re.compile(r"V6M0: HEALTH free=(\d+) psram=(\d+) wake=(\d+) taps=(\d+)")
WAKE = re.compile(r"V6M0: WAKE_DETECTED .*count=(\d+)")
FAILURE = re.compile(r"abort\(\) was called|assert failed|Guru Meditation Error")


def summarize(text):
    """Return aggregates only; never include source lines or unrecognized text."""
    result = dict(schema="claw4-v6-m1-voice-log/1", candidate_anchor_count=0,
                  candidate_anchor_prefixes=[], boot_ready_count=0,
                  wake_events_observed=0, health_samples=0,
                  heap_min_bytes=None, psram_min_bytes=None,
                  crash_markers=0,
                  voice_flow={k: "NOT_VERIFIED" for k in
                              ("speech", "asr", "llm", "tts", "device_playback",
                               "aec_effectiveness", "near_end_during_playback", "wake_recovery")},
                  privacy="raw lines, speech, transcripts, endpoints and credentials are not emitted")
    wakes, heap, psram = set(), [], []
    for line in text.splitlines():
        match = LINE.match(line)
        msg = match.group(2) if match else line
        anchors = ANCHOR.findall(msg)
        result["candidate_anchor_count"] += len(anchors)
        result["candidate_anchor_prefixes"].extend(a.lower() for a in anchors)
        if "V6M0: BOOT_READY" in msg:
            result["boot_ready_count"] += 1
        wake = WAKE.search(msg)
        if wake:
            wakes.add(int(wake.group(1)))
        health = HEALTH.search(msg)
        if health:
            result["health_samples"] += 1
            heap.append(int(health.group(1))); psram.append(int(health.group(2)))
        if FAILURE.search(msg):
            result["crash_markers"] += 1
    result["wake_events_observed"] = len(wakes)
    result["heap_min_bytes"] = min(heap) if heap else None
    result["psram_min_bytes"] = min(psram) if psram else None
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path); parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(json.dumps(summarize(args.input.read_text(encoding="utf-8", errors="replace")), indent=2), encoding="utf-8")
