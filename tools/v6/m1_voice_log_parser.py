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
# Reviewed opt-in 1 Hz AfeAudioEngine diagnostics. Keep each record exact,
# bounded, and tied to the firmware's emitting ESP-IDF tag.
M1_AFE_STATE = re.compile(
    r"^[IWEVD] \((\d{1,10})\) AfeAudioEngine: M1_AFE_STATE "
    r"wake=(\d{1,10}) active=(\d{1,10}) wn_on=(\d{1,10}) "
    r"wn_off=(\d{1,10}) detected=(\d{1,10}) other=(\d{1,10})$"
)
M1_AFE_FLOW = re.compile(
    r"^[IWEVD] \((\d{1,10})\) AfeAudioEngine: M1_AFE_FLOW "
    r"feed_calls=(\d{1,10}) feed_samples=(\d{1,10}) feed_chunks=(\d{1,10}) "
    r"fetch_ok=(\d{1,10}) fetch_fail=(\d{1,10}) fetch_other=(\d{1,10})$"
)
M1_AFE_CTL = re.compile(
    r"^[IWEVD] \((\d{1,10})\) AfeAudioEngine: M1_AFE_CTL "
    r"on_total=(\d{1,10}) on_rc=(-?\d{1,10}) "
    r"off_total=(\d{1,10}) off_rc=(-?\d{1,10})$"
)
MAX_U32 = (1 << 32) - 1
MIN_I32 = -(1 << 31)
MAX_I32 = (1 << 31) - 1
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
                  m1_afe_state_sample_count=0,
                  m1_afe_wake_enabled_last=None, m1_afe_active_last=None,
                  m1_afe_wn_on_total=0, m1_afe_wn_off_total=0,
                  m1_afe_detected_total=0, m1_afe_other_total=0,
                  m1_afe_flow_sample_count=0,
                  m1_afe_feed_calls_total=0, m1_afe_feed_samples_total=0,
                  m1_afe_feed_chunks_total=0, m1_afe_fetch_ok_total=0,
                  m1_afe_fetch_fail_total=0, m1_afe_fetch_other_total=0,
                  m1_afe_control_sample_count=0, m1_afe_control_last=None,
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
        if len(line) <= MAX_UART_LINE_CHARS:
            state = M1_AFE_STATE.fullmatch(line)
            if state:
                timestamp, *values = map(int, state.groups())
                del timestamp  # The timestamp is syntax only; it is not a session key.
                wake, active, wn_on, wn_off, detected, other = values
                if (wake in (0, 1) and active in (0, 1)
                        and all(value <= MAX_U32 for value in values)):
                    totals = ("m1_afe_wn_on_total", "m1_afe_wn_off_total",
                              "m1_afe_detected_total", "m1_afe_other_total")
                    increments = (wn_on, wn_off, detected, other)
                    if (result["m1_afe_state_sample_count"] < MAX_U64
                            and all(result[key] + value <= MAX_U64
                                    for key, value in zip(totals, increments))):
                        result["m1_afe_state_sample_count"] += 1
                        result["m1_afe_wake_enabled_last"] = wake
                        result["m1_afe_active_last"] = active
                        for key, value in zip(totals, increments):
                            result[key] += value
            flow = M1_AFE_FLOW.fullmatch(line)
            if flow:
                timestamp, *values = map(int, flow.groups())
                del timestamp  # Do not infer session identity from log uptime.
                totals = ("m1_afe_feed_calls_total", "m1_afe_feed_samples_total",
                          "m1_afe_feed_chunks_total", "m1_afe_fetch_ok_total",
                          "m1_afe_fetch_fail_total", "m1_afe_fetch_other_total")
                if (all(value <= MAX_U32 for value in values)
                        and result["m1_afe_flow_sample_count"] < MAX_U64
                        and all(result[key] + value <= MAX_U64
                                for key, value in zip(totals, values))):
                    result["m1_afe_flow_sample_count"] += 1
                    for key, value in zip(totals, values):
                        result[key] += value
            control = M1_AFE_CTL.fullmatch(line)
            if control:
                timestamp, on_total, on_rc, off_total, off_rc = map(int, control.groups())
                del timestamp  # Do not infer session identity from log uptime.
                snapshot = {
                    "on_total": on_total,
                    "on_rc": on_rc,
                    "off_total": off_total,
                    "off_rc": off_rc,
                }
                if (all(snapshot[key] <= MAX_U32 for key in ("on_total", "off_total"))
                        and all(MIN_I32 <= snapshot[key] <= MAX_I32 for key in
                                ("on_rc", "off_rc"))
                        and result["m1_afe_control_sample_count"] < MAX_U64):
                    result["m1_afe_control_sample_count"] += 1
                    result["m1_afe_control_last"] = snapshot
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
