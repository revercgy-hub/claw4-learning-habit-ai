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
# Keep the new M1 inputs narrow: ESP-IDF's UART prefix, exact emitting tag,
# exact message layout, and uint32 values as produced by the firmware's %u.
M1_DIAG = re.compile(
    r"^[IWEVD] \((\d{1,10})\) Claw4Audio: M1_INPUT_DIAG "
    r"mic_n=(\d{1,10}) rms=(\d{1,10}) peak=(\d{1,10}) "
    r"i2s_read_failures=(\d{1,10})$"
)
M1_WAKE = re.compile(r"^[IWEVD] \((\d{1,10})\) Application: Wake word detected$")
MAX_U32 = (1 << 32) - 1
MAX_U64 = (1 << 64) - 1
MAX_UART_LINE_CHARS = 256


def summarize(text):
    """Return aggregates only; never include source lines or unrecognized text."""
    result = dict(schema="claw4-v6-m1-voice-log/1", candidate_anchor_count=0,
                  candidate_anchor_prefixes=[], boot_ready_count=0,
                  wake_events_observed=0, health_samples=0,
                  m1_input_diag_window_count=0, m1_mic_samples_total=0,
                  m1_rms_max=None, m1_peak_max=None,
                  m1_i2s_read_failures_total=0, m1_wake_log_count=0,
                  heap_min_bytes=None, psram_min_bytes=None,
                  crash_markers=0,
                  voice_flow={k: "NOT_VERIFIED" for k in
                              ("speech", "asr", "llm", "tts", "device_playback",
                               "aec_effectiveness", "near_end_during_playback", "wake_recovery")},
                  privacy="raw lines, speech, transcripts, endpoints and credentials are not emitted")
    wakes, heap, psram = set(), [], []
    for line in text.splitlines():
        # A bounded line also prevents huge hostile inputs from reaching the
        # regular expressions or numeric conversions below.
        diag = M1_DIAG.fullmatch(line) if len(line) <= MAX_UART_LINE_CHARS else None
        if diag:
            mic_n, rms, peak, failures = map(int, diag.groups()[1:])
            if all(value <= MAX_U32 for value in (mic_n, rms, peak, failures)):
                samples_total = result["m1_mic_samples_total"] + mic_n
                failures_total = result["m1_i2s_read_failures_total"] + failures
                if (samples_total <= MAX_U64 and failures_total <= MAX_U64
                        and result["m1_input_diag_window_count"] < MAX_U64):
                    result["m1_input_diag_window_count"] += 1
                    result["m1_mic_samples_total"] = samples_total
                    result["m1_i2s_read_failures_total"] = failures_total
                    result["m1_rms_max"] = max(result["m1_rms_max"] or 0, rms)
                    result["m1_peak_max"] = max(result["m1_peak_max"] or 0, peak)
        if len(line) <= MAX_UART_LINE_CHARS and M1_WAKE.fullmatch(line):
            if result["m1_wake_log_count"] < MAX_U64:
                result["m1_wake_log_count"] += 1
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
