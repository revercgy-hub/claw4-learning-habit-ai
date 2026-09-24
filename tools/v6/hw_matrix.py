#!/usr/bin/env python3
"""V6 M0 hardware-matrix collector.

Read-only. Parses private UART capture logs plus the frozen candidate manifest and
emits (a) a structured JSON matrix and (b) a Markdown table. It never opens the
serial port, never writes into out/v6-device-private/, and never promotes a build
result into a hardware fact.

Design rule: a signal that is absent is reported as ABSENT, never as PASS.
The tool refuses to fabricate a row status from a neighbouring row.

Usage:
  python tools/v6/hw_matrix.py scan \
      --boot  out/v6-device-private/boot-m0-04.txt \
      --flash out/v6-device-private/flash-m0-04.txt \
      --candidate integration/v6/m0-candidate-04.json \
      --json-out integration/v6/m0-hw-matrix.json \
      --md-out   docs/v6/V6_M0_HW_MATRIX.md
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from datetime import datetime, timezone

SCHEMA = "claw4-v6-m0-hw-matrix/1"

# Timestamped ESP-IDF log line: "I (9565) TAG: message"
LOG_LINE = re.compile(r"^([IWEVD])\s*\((\d+)\)\s+(.*)$")

# A signal is (key, kind, payload) where kind is one of:
#   "line"    -> first matching line wins
#   "all"     -> every matching line is kept
#   "count"   -> number of matching lines
#   "int"     -> first capture group of the first match, as int
SIGNALS: tuple[tuple[str, str, str], ...] = (
    # --- boot / reset -------------------------------------------------------
    ("reset_reason",        "line",  r"^rst:0x[0-9a-f]+ \([^)]+\)"),
    ("boot_blocked",        "count", r"Claw4V6: BOOT_BLOCKED"),
    ("psram_found",         "line", r"Found 32MB PSRAM device"),
    ("boot_attempts",       "count", r"Calling app_main\(\)"),
    ("panic_resets",        "count", r"^rst:0x[0-9a-f]+ \(SW_CPU_RESET\)"),
    ("boot_ready",          "line",  r"V6M0: BOOT_READY IDF=(\S+);"),
    ("boot_ready_count",    "count", r"V6M0: BOOT_READY"),
    ("idf_version",         "line",  r"V6M0: BOOT_READY IDF=(\S+);"),
    ("assertions",          "count", r"abort\(\) was called|assert failed|Guru Meditation Error"),
    # --- display ------------------------------------------------------------
    ("display_rgb888",      "line",  r"Claw4V6: Native RGB888 display.*"),
    ("display_init",        "line",  r"Claw4V6: NV3051F display initialized.*"),
    ("lvgl_first_refresh",  "line",  r"Claw4V6: LVGL first refresh completed.*"),
    ("panel_capability_errors", "count", r"lcd_panel: esp_lcd_panel_(swap_xy|mirror).*not supported"),
    ("assets_applied",      "int",   r"V6M0: Assets applied=(\d+)"),
    ("assets_mapped",       "line",  r"Assets: The assets map size is .*"),
    # --- camera diagnostic (metadata only; one RAW8 frame is never stored) --
    ("camera_sensor_init",       "count", r"CAMERA_DIAGNOSTIC sensor_stack_initialized=1(?:\s|$)"),
    ("camera_sensor_init_failed", "count", r"CAMERA_DIAGNOSTIC sensor_stack_initialized=0(?:\s|$)"),
    ("camera_frame_capture",     "count", r"CAMERA_DIAGNOSTIC frame_capture=1(?:\s|$)"),
    ("camera_frame_failure",     "count", r"CAMERA_DIAGNOSTIC frame_capture=0\s+stage=\S+ error=-?\d+ cleanup_error=\d+"),
    ("camera_frame_clean",       "count", r"CAMERA_DIAGNOSTIC frame_capture=1\b.*\bsaved=0\b.*\buploaded=0\b.*\bcleanup_error=0\b"),
    ("camera_frame_unqualified", "count", r"CAMERA_DIAGNOSTIC frame_capture=1\b(?!.*\bsaved=0\b.*\buploaded=0\b.*\bcleanup_error=0\b).*"),
    ("camera_cleanup_failure",   "count", r"CAMERA_DIAGNOSTIC .*\bcleanup_error=[1-9]\d*\b"),
    ("camera_saved_frame",       "count", r"CAMERA_DIAGNOSTIC frame_capture=1\b.*\bsaved=[1-9]\d*\b"),
    ("camera_uploaded_frame",    "count", r"CAMERA_DIAGNOSTIC frame_capture=1\b.*\buploaded=[1-9]\d*\b"),
    ("camera_deinit_failure",    "count", r"CAMERA_DIAGNOSTIC deinit=\S+"),
    ("camera_task_failure",      "count", r"CAMERA_DIAGNOSTIC task creation failed"),
    # --- SD mount and power-key diagnostics -------------------------------
    ("sd_mounted",               "line",  r"SD_DIAGNOSTIC mounted blocks=\d+ sector_bytes=\d+ capacity_bytes=\d+"),
    ("sd_mount_failure",         "count", r"SD_DIAGNOSTIC not_mounted error=\S+"),
    ("sd_task_failure",          "count", r"SD_DIAGNOSTIC task creation failed"),
    ("power_key_armed",          "int",   r"POWER_KEY_DIAGNOSTIC armed_after_boot_release=(\d+)"),
    ("power_key_start_failure",  "count", r"POWER_KEY_DIAGNOSTIC unavailable at start error=\S+"),
    ("power_key_read_failure",   "count", r"POWER_KEY_DIAGNOSTIC read error=\S+"),
    ("power_key_task_failure",   "count", r"POWER_KEY_DIAGNOSTIC task creation failed"),
    ("power_key_short",          "count", r"POWER_KEY_SHORT detected; diagnostic has no power side effects"),
    ("power_key_long",           "count", r"POWER_KEY_LONG detected; diagnostic has no power side effects"),
    # --- touch --------------------------------------------------------------
    ("touch_init",          "line",  r"Claw4V6: GT911 touch initialized"),
    ("touch_last_count",    "int",   r"V6M0: TOUCH count=(\d+)"),
    ("touch_record_cycles", "count", r"V6M0: TOUCH count=\d+; (?:audio record begin|probe=\d+ phase=recording begin)"),
    ("reference_probe_begins", "count", r"V6M0: LOCAL_REFERENCE_PROBE_BEGIN id=\d+ phase=playback(?: wake_enabled=1)?;"),
    ("reference_probe_ends", "count", r"V6M0: LOCAL_REFERENCE_PROBE_END id=\d+ drained=\d+ playback_vad_onsets=\d+"),
    ("reference_probe_vad", "count", r"V6M0: LOCAL_REFERENCE_PROBE_END id=\d+ drained=\d+ playback_vad_onsets=[1-9]\d*"),
    ("reference_probe_wake", "count", r"V6M0: WAKE_DETECTED \(local only\) probe=\d+ phase=2 count=\d+"),
    # --- audio --------------------------------------------------------------
    ("audio_i2s",           "line",  r"Claw4Audio: I2S slave .*"),
    ("audio_module_probe",  "line",  r"Claw4V6: Audio module local-mode response .*"),
    ("afe_pipeline",        "line",  r"AFE: AFE Pipeline: .*AEC.*"),
    ("wakenet_model",       "line",  r"AFE_CONFIG: Set WakeNet Model: (\S+)"),
    ("playback_queued",     "count", r"V6M0: Audio testing playback queued"),
    ("wake_detected",       "count", r"V6M0: WAKE_DETECTED \(local only\)"),
    # --- coprocessor / network ---------------------------------------------
    ("c5_slave",            "line",  r"transport: Identified slave \[(\w+)\]"),
    ("sdio_card_init",      "line",  r"H_SDIO_DRV: Card init success.*"),
    ("c5_bootup",           "line",  r"RPC_WRAP: Coprocessor Boot-up"),
    ("wifi_attempt",        "count", r"WifiBoard: Starting WiFi connection attempt"),
    ("wifi_scan_cycles",    "count", r"WifiStation: Scanning all channels"),
    ("saved_channel_scan",  "int",   r"WifiStation: Scanning saved channel (\d+)"),
    ("no_ap_on_saved_ch",   "count", r"WifiStation: No AP on saved channels"),
    ("wifi_scan_done",      "count", r"RPC_WRAP: ESP Event: StaScanDone"),
    ("wifi_no_ap",          "count", r"WifiStation: No AP found"),
    ("wifi_config_mode",    "count", r"WiFi config mode entered"),
    ("connect_timeout",     "count", r"WifiBoard: WiFi connection timeout, entering config mode"),
    ("wifi_disconnected",   "count", r"WifiBoard: WiFi disconnected"),
    ("softap_started",      "count", r"RPC_WRAP: ESP Event: softap started"),
    ("config_ap_ssid",      "line",  r"WifiConfigurationAp: Access Point started with SSID (\S+)"),
    ("config_ap_dhcp",      "line",  r"DHCP server started on interface WIFI_AP_DEF with IP: (\S+)"),
    ("config_web_server",   "count", r"WifiConfigurationAp: Web server started"),
    ("state_machine_reject", "count", r"StateMachine: Invalid state transition"),
    ("found_ap",            "g1",    r"WifiStation: Found AP: ([^,]+),"),
    ("found_ap_count",      "count", r"WifiStation: Found AP: "),
    ("wifi_connecting",     "count", r"WifiBoard: WiFi connecting to "),
    ("sta_connected_event", "count", r"ESP Event: Station mode: Connected"),
    ("sta_ip",              "g1",    r"esp_netif_handlers: sta ip: ([\d.]+),"),
    ("sta_gateway",         "g1",    r"gw: ([\d.]+)"),
    ("wifi_connected",      "line",  r"Connected to WiFi: (\S+)"),
    # A set, not a max: NETWORK_EVENT is a state code, so "the largest value ever
    # seen" is meaningless. What matters is which states were observed at all.
    ("net_events",          "set",   r"V6M0: NETWORK_EVENT=(\d+)"),
    ("mac_not_ready",       "count", r"system_api: .*mac type is incorrect"),
    # --- health -------------------------------------------------------------
    ("health_last",         "line",  r"V6M0: HEALTH .*"),
    ("health_samples",      "count", r"V6M0: HEALTH free=\d+"),
)

KIND = {key: kind for key, kind, _ in SIGNALS}

# Numeric signals are merged across captures by maximum, so a later capture that
# happens to contain no taps cannot erase an earlier capture's tap evidence. Every
# numeric signal here is a counter, a monotonic state code, or a peak value.
_NUMERIC = frozenset(KIND[k] for k in KIND if KIND[k] in ("count", "int"))

_HEALTH = re.compile(r"V6M0: HEALTH free=(\d+) psram=(\d+) wake=(\d+) taps=(\d+)")

# Signals where the newest occurrence is the interesting one. Everything else
# keeps its first match so a later retry does not rewrite the first observation.
LAST_WINS = frozenset({"touch_last_count", "health_last", "net_event"})


def sha256_file(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _classify(key: str, kind: str, match: re.Match[str]) -> object:
    if kind == "count":
        return True  # counted separately
    payload = match.group(0)
    if kind == "int":
        return int(match.group(1))
    if kind == "g1":
        return match.group(1)
    if key == "idf_version":
        return match.group(1)
    return payload


def parse_boot_log(path: str) -> dict:
    """Extract every known signal from one UART capture."""
    preamble: list[str] = []
    counters = {key: 0 for key, kind, _ in SIGNALS if kind == "count"}
    # Pre-seed every scalar key so "absent" is explicit in the JSON and never
    # silently confused with "zero".
    values: dict[str, object] = {
        key: ([] if kind == "set" else None)
        for key, kind, _ in SIGNALS if kind != "count"
    }
    health: list[dict] = []
    elf_hashes = set()
    candidate_elf_anchors: list[str] = []
    elf_file_hashes: list[str] = []
    last_ts = 0
    lines = 0

    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            line = re.sub(r"\x1b\[[0-9;]*m", "", raw.rstrip("\n"))
            for candidate_elf in re.finditer(r"CANDIDATE_ELF_SHA256=([0-9a-zA-Z]+)", line):
                candidate_elf_anchors.append(candidate_elf.group(1))
                elf_hashes.add(candidate_elf.group(1))
            for elf_file in re.finditer(r"ELF file SHA256:\s*([0-9a-zA-Z]+)", line):
                elf_file_hashes.append(elf_file.group(1))
                elf_hashes.add(elf_file.group(1))
            lines += 1
            match = LOG_LINE.match(line)
            if match:
                last_ts = int(match.group(2))
                body = match.group(3)
            else:
                # Untimestamped lines such as "rst:0x17 (...)" belong to the ROM
                # preamble; keep a bounded copy so raw evidence stays traceable.
                if len(preamble) < 8 and line.strip():
                    preamble.append(line.strip())
                body = line

            for key, kind, pattern in SIGNALS:
                found = re.search(pattern, body)
                if not found:
                    continue
                if kind == "count":
                    counters[key] += 1
                    continue
                if kind == "set":
                    values[key] = sorted(set(values[key]) | {int(found.group(1))})
                    continue
                if values[key] is None or key in LAST_WINS:
                    values[key] = _classify(key, kind, found)
                if key == "health_last":
                    detail = _HEALTH.search(body)
                    if detail:
                        health.append({
                            "ts_ms": last_ts,
                            "free": int(detail.group(1)),
                            "psram": int(detail.group(2)),
                            "wake_word_running": int(detail.group(3)),
                            "taps": int(detail.group(4)),
                        })

    values.update(counters)
    return {
        "log": os.path.basename(path),
        "path": path,
        "sha256": sha256_file(path),
        "bytes": os.path.getsize(path),
        "lines": lines,
        "last_timestamp_ms": last_ts,
        "elf_hashes": sorted(elf_hashes),
        "candidate_elf_anchors": candidate_elf_anchors,
        "elf_file_hashes": elf_file_hashes,
        "preamble": preamble,
        "signals": values,
        "health": health,
    }


def parse_flash_log(path: str) -> dict:
    verified = 0
    sections: list[str] = []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            line = raw.strip()
            if "Hash of data verified." in line:
                verified += 1
            if line.startswith("Writing at 0x"):
                sections.append(line.split("...")[0].strip())
    return {
        "log": os.path.basename(path),
        "sha256": sha256_file(path),
        "bytes": os.path.getsize(path),
        "verified_images": verified,
        "write_sections": sorted(set(sections)),
    }


def aggregate_signals(boots: list[dict]) -> tuple[dict, dict]:
    """Merge signals across captures and record which capture supplied each one.

    All captures passed to one run must come from the same frozen candidate;
    mixing candidates would let one build inherit another build's evidence.
    """
    merged: dict[str, object] = {
        key: ([] if KIND[key] == "set" else None) for key in KIND
    }
    source: dict[str, str] = {}
    for capture in boots:
        for key, value in capture["signals"].items():
            if value is None or (KIND[key] == "set" and not value):
                continue
            if KIND[key] == "set":
                combined = sorted(set(merged.get(key) or []) | set(value))
                if combined != merged.get(key):
                    merged[key] = combined
                    source[key] = capture["log"]
            elif KIND[key] in ("count", "int"):
                current = merged.get(key)
                if current is None or value > current:
                    merged[key] = value
                    source[key] = capture["log"]
            elif merged.get(key) is None:
                merged[key] = value
                source[key] = capture["log"]
    for key in KIND:
        if KIND[key] == "count" and merged.get(key) is None:
            merged[key] = 0
    return merged, source


def _state(ok: bool, partial: bool = False) -> str:
    if ok:
        return "PASS"
    return "PARTIAL" if partial else "NOT_VERIFIED"


def build_rows(s: dict, src: dict, flash: dict, boots: list[dict],
               user_confirmations: dict) -> list[dict]:
    """One row per M0 hardware item. Every row cites the concrete signal it used."""
    rows: list[dict] = []
    observed_s = max((c["last_timestamp_ms"] for c in boots), default=0) / 1000.0

    def row(item, evidence, status, note="", source_key=None):
        rows.append({"item": item, "evidence": evidence, "status": status, "note": note,
                     "source": src.get(source_key) if source_key else None})

    # Flash / PSRAM --------------------------------------------------------
    # Decoupled from boot health on purpose: only the flash capture can speak
    # about flash writes, and an unrelated init failure must not silently
    # downgrade (or upgrade) this row.
    row("Flash / PSRAM", "candidate manifest + flash log 'Hash of data verified.'",
        _state(flash.get("verified_images", 0) > 0 and bool(s.get("psram_found"))),
        f"verified writes in this capture: {flash.get('verified_images', 0)}; "
        "long-run stress still not exercised")

    # Boot -----------------------------------------------------------------
    aborts = s.get("assertions", 0)
    if aborts or s.get("boot_blocked", 0):
        # A captured abort is evidence of a defect, not merely an unverified item.
        boot_status = "FAIL"
    else:
        boot_status = _state(bool(s.get("boot_ready")))
    row("启动", s.get("boot_ready") or "BOOT_READY absent",
        boot_status,
        f"boot attempts={s.get('boot_attempts', 0)}, BOOT_READY={s.get('boot_ready_count', 0)}, "
        f"aborts={aborts}, panic resets={s.get('panic_resets', 0)}; "
        f"latest device timestamp {observed_s:.1f}s (not capture duration)",
        source_key="boot_ready")

    # Display --------------------------------------------------------------
    display_built = bool(s.get("display_rgb888")) and bool(s.get("lvgl_first_refresh"))
    display_user = user_confirmations.get("display_visible") is True
    row("显示", s.get("display_rgb888") or "RGB888 line absent",
        _state(display_built and display_user),
        f"display init={'yes' if s.get('display_init') else 'no'}, "
        f"user confirmed visible={'yes' if display_user else 'no'}, "
        f"unsupported-capability errors={s.get('panel_capability_errors', 0)}",
        source_key="display_rgb888")

    # Touch ----------------------------------------------------------------
    row("触摸", s.get("touch_init") or "GT911 init line absent",
        _state(bool(s.get("touch_init")) and s.get("touch_record_cycles", 0) > 0),
        f"tap-driven record cycles={s.get('touch_record_cycles', 0)}, "
        f"last tap count={s.get('touch_last_count', 0)}",
        source_key="touch_record_cycles")

    # Audio ----------------------------------------------------------------
    audio_pipeline = bool(s.get("audio_i2s")) and bool(s.get("afe_pipeline"))
    loopback = user_confirmations.get("audio_loopback") is True
    wake = s.get("wake_detected", 0) > 0
    row("音频", s.get("audio_i2s") or "I2S line absent",
        _state(audio_pipeline and loopback),
        f"AFE={'yes' if s.get('afe_pipeline') else 'no'}, "
        f"loopback user-confirmed={'yes' if loopback else 'no'}, "
        f"WakeNet={s.get('wakenet_model', 'n/a')}, "
        f"WAKE_DETECTED events={s.get('wake_detected', 0)}",
        source_key="audio_i2s")

    # Wake word (separate row: loopback PASS must not imply wake-word PASS) --
    row("唤醒词", "V6M0: WAKE_DETECTED count",
        _state(wake),
        f"positive wake callbacks={s.get('wake_detected', 0)}; armed state is not detection evidence")

    # C5 / network ---------------------------------------------------------
    c5_link = bool(s.get("c5_slave")) and bool(s.get("sdio_card_init")) and bool(s.get("c5_bootup"))
    row("C5 链路", s.get("c5_slave") or "slave identification line absent",
        _state(c5_link),
        f"SDIO card init={'yes' if s.get('sdio_card_init') else 'no'}, "
        f"coprocessor boot-up={'yes' if s.get('c5_bootup') else 'no'}",
        source_key="sdio_card_init")

    # Config portal: proves the C5 radio transmits, not that it can associate.
    softap = (s.get("config_ap_ssid") and s.get("config_ap_dhcp")
              and s.get("config_web_server", 0) > 0)
    row("配网模式（热点/配置页）", s.get("config_ap_ssid") or "no 'Access Point started' line",
        _state(bool(softap)),
        f"AP SSID={s.get('config_ap_ssid', 'n/a')}, "
        f"DHCP={s.get('config_ap_dhcp', 'n/a')}, "
        f"softap events={s.get('softap_started', 0)}, "
        f"connect timeout after={s.get('connect_timeout', 0)} cycle(s); "
        "a started softap proves the radio transmits, it does not prove association",
        source_key="config_ap_ssid")

    ip_ok = any(c["signals"].get("wifi_connected") and c["signals"].get("sta_ip") for c in boots)
    row("网络 / IP", s.get("wifi_connected") or "no 'Connected to WiFi' line",
        _state(ip_ok),
        f"SSID={s.get('found_ap', 'n/a')}, IP={s.get('sta_ip', 'n/a')}, "
        f"gw={s.get('sta_gateway', 'n/a')}, "
        f"NETWORK_EVENT seen={s.get('net_events') or 'none'} "
        f"(0=Scanning 1=Connecting 2=Connected 3=Disconnected 4=ConfigModeEnter), "
        f"found-AP={s.get('found_ap_count', 0)}, connecting={s.get('wifi_connecting', 0)}, "
        f"connected={s.get('sta_connected_event', 0)}; "
        f"scan cycles={s.get('wifi_scan_cycles', 0)}, "
        f"'No AP found' cycles={s.get('wifi_no_ap', 0)}, "
        f"saved-channel scans={s.get('no_ap_on_saved_ch', 0)} "
        f"(saved channel={s.get('saved_channel_scan', 'n/a')}), "
        f"config-mode entries={s.get('wifi_config_mode', 0)}; "
        "'No AP found' means no saved SSID matched, not that the scan returned nothing",
        source_key="wifi_connected")

    # Resource partition ---------------------------------------------------
    applied = s.get("assets_applied")
    row("资源分区", f"V6M0: Assets applied={applied}",
        _state(applied == 1),
        "assets_apply() returned " + ("true" if applied == 1 else "false/absent"),
        source_key="assets_applied")

    # Camera: sensor init alone is not evidence of a captured frame. A clean
    # frame requires cleanup_error=0 and explicit saved=0/uploaded=0 metadata.
    camera_clean = s.get("camera_frame_clean", 0)
    camera_errors = sum(s.get(key, 0) for key in (
        "camera_sensor_init_failed", "camera_frame_failure", "camera_frame_unqualified",
        "camera_cleanup_failure",
        "camera_saved_frame", "camera_uploaded_frame", "camera_deinit_failure",
        "camera_task_failure"))
    camera_has_result = bool(camera_errors)
    if camera_clean:
        camera_status = "PARTIAL" if camera_errors else "PASS"
    elif camera_has_result:
        camera_status = "FAIL"
    else:
        camera_status = "NOT_VERIFIED"
    camera_source = next((key for key in (
        "camera_sensor_init_failed", "camera_frame_failure", "camera_frame_unqualified",
        "camera_cleanup_failure", "camera_saved_frame", "camera_uploaded_frame",
        "camera_deinit_failure", "camera_task_failure")
        if s.get(key, 0)), "camera_frame_clean" if camera_clean else "camera_sensor_init")
    row("相机 RAW8 单帧取帧", "CAMERA_DIAGNOSTIC frame_capture=1; saved=0; uploaded=0; cleanup_error=0",
        camera_status,
        f"sensor init={s.get('camera_sensor_init', 0)}, clean frame={camera_clean}, "
        f"capture failures={s.get('camera_frame_failure', 0)}, "
        f"unqualified frames={s.get('camera_frame_unqualified', 0)}, "
        f"cleanup failures={s.get('camera_cleanup_failure', 0)}, "
        f"deinit/task failures={s.get('camera_deinit_failure', 0)}/"
        f"{s.get('camera_task_failure', 0)}, "
        f"saved/uploaded nonzero={s.get('camera_saved_frame', 0)}/"
        f"{s.get('camera_uploaded_frame', 0)}; sensor init alone is not a frame result",
        source_key=camera_source)

    # An SD mount proves one boot-time mount only, not hot-plug or storage I/O.
    sd_mounted = bool(s.get("sd_mounted"))
    sd_errors = s.get("sd_mount_failure", 0) + s.get("sd_task_failure", 0)
    sd_source = next((key for key in ("sd_mount_failure", "sd_task_failure")
                      if s.get(key, 0)), "sd_mounted")
    if sd_mounted:
        sd_status = "PARTIAL" if sd_errors else "PASS"
    else:
        sd_status = "FAIL" if sd_errors else "NOT_VERIFIED"
    row("SD 卡单次挂载", s.get("sd_mounted") or "SD_DIAGNOSTIC mounted line absent",
        sd_status,
        f"mount failures={s.get('sd_mount_failure', 0)}, task failures={s.get('sd_task_failure', 0)}; "
        "hot-plug, repeated mount/unmount, and sustained I/O are not proven",
        source_key=sd_source)

    # Recognition is a hardware input check; the diagnostic intentionally has
    # no product shutdown or standby action.
    power_short = s.get("power_key_short", 0) > 0
    power_long = s.get("power_key_long", 0) > 0
    power_errors = sum(s.get(key, 0) for key in (
        "power_key_start_failure", "power_key_read_failure", "power_key_task_failure"))
    if power_short and power_long:
        power_status = "PARTIAL" if power_errors else "PASS"
    elif power_errors and not (power_short or power_long):
        power_status = "FAIL"
    elif power_short or power_long:
        power_status = "PARTIAL"
    else:
        power_status = "NOT_VERIFIED"
    power_source = next((key for key in (
        "power_key_start_failure", "power_key_read_failure", "power_key_task_failure",
        "power_key_short", "power_key_long", "power_key_armed")
        if s.get(key, 0)), "power_key_armed")
    row("电源键短按/长按识别", "POWER_KEY_SHORT + POWER_KEY_LONG",
        power_status,
        f"short={s.get('power_key_short', 0)}, long={s.get('power_key_long', 0)}, "
        f"armed after boot release={s.get('power_key_armed', 'absent')}, "
        f"diagnostic errors={power_errors}; no shutdown/standby action is implemented",
        source_key=power_source)

    # Rollback -------------------------------------------------------------
    row("回滚（恢复写回）", "out/v6-device-private/pre-v6-flash.bin",
        "NOT_TESTED",
        "a full 32MiB image was read back and hashed, but it has never been "
        "written back; until then rollback is unproven")
    return rows


DEFAULT_USER_CONFIRMATIONS = {
    "display_visible": False,   # 2026-09-22 用户确认“已显示文字和按钮”
    "audio_loopback": False,    # 2026-09-22 用户确认可录 3 秒并回放
    "psram_stress": None,
}


def _verify_boot_identity(boots, expected_elf, evidence_manifest=None) -> None:
    """Fail closed unless every capture has a verified candidate/session identity.

    Without a manifest, each --boot file is an independent capture and must carry
    its own boot anchor. With a manifest, continuations may inherit an anchor only
    through the existing audio_evidence protocol's ordered path/hash/session rows.
    """
    if not isinstance(expected_elf, str) or not re.fullmatch(r"[0-9a-fA-F]{64}", expected_elf):
        raise ValueError("Candidate ELF requires full SHA256")
    expected_elf = expected_elf.lower()
    if not boots:
        raise ValueError("At least one boot capture is required")

    def identities(capture, allow_empty=False):
        candidates = capture.get("candidate_elf_anchors", [])
        elf_files = capture.get("elf_file_hashes", [])
        if len(candidates) > 1 or len(elf_files) > 1 or (not allow_empty and not (candidates or elf_files)):
            raise ValueError(f"Boot capture requires one unambiguous identity anchor: {capture['log']}")
        markers = [*candidates, *elf_files]
        if any(not re.fullmatch(r"[0-9a-fA-F]{9,64}", value) for value in markers):
            raise ValueError(f"Invalid boot identity anchor: {capture['log']}")
        if any(not expected_elf.startswith(value.lower()) for value in markers):
            raise ValueError(f"Candidate ELF mismatch: {capture['log']}")
        if len(markers) == 2:
            a, b = (value.lower() for value in markers)
            if not (a.startswith(b) or b.startswith(a)):
                raise ValueError(f"Conflicting ELF identity markers: {capture['log']}")
        return candidates

    def startup_counts(capture):
        with open(capture["path"], "r", encoding="utf-8", errors="replace") as handle:
            content = re.sub(r"\x1b\[[0-9;]*m", "", handle.read())
        starts = len(re.findall(r"Calling app_main\(\)", content))
        resets = len(re.findall(r"(?m)^rst:0x[0-9a-fA-F]+", content))
        if starts > 1 or resets > 1:
            raise ValueError(f"Multiple boot starts in capture: {capture['log']}")
        return starts, resets

    if evidence_manifest is None:
        seen_hashes: set[str] = set()
        for capture in boots:
            identities(capture)
            digest = capture.get("sha256", "").lower()
            if not re.fullmatch(r"[0-9a-f]{64}", digest) or digest in seen_hashes:
                raise ValueError(f"Missing or duplicate capture SHA256: {capture['log']}")
            seen_hashes.add(digest)
            startup_counts(capture)
        return

    manifest_elf = evidence_manifest.get("elf_sha256")
    if (not isinstance(manifest_elf, str)
            or not re.fullmatch(r"[0-9a-fA-F]{64}", manifest_elf)
            or manifest_elf.lower() != expected_elf):
        raise ValueError("Evidence manifest ELF SHA256 mismatch")
    entries = evidence_manifest.get("captures")
    if not isinstance(entries, list) or len(entries) != len(boots):
        raise ValueError("Evidence manifest captures must match --boot inputs")

    seen_hashes: set[str] = set()
    sessions: dict[str, dict] = {}
    for capture, entry in zip(boots, entries):
        if not isinstance(entry, dict):
            raise ValueError("Invalid evidence manifest capture")
        path, digest, session = entry.get("path"), entry.get("sha256"), entry.get("session")
        if not isinstance(path, str) or not path or not isinstance(session, str) or not session.strip():
            raise ValueError("Evidence manifest capture requires path and session")
        if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-fA-F]{64}", digest):
            raise ValueError("Full capture SHA256 required")
        if os.path.normcase(os.path.realpath(path)) != os.path.normcase(os.path.realpath(capture["path"])):
            raise ValueError("Evidence manifest capture order/path mismatch")
        actual = capture.get("sha256", "")
        if actual.lower() != digest.lower() or actual.lower() in seen_hashes:
            raise ValueError("Capture hash mismatch or duplicate capture")
        seen_hashes.add(actual.lower())

        state = sessions.get(session)
        candidates = identities(capture, allow_empty=True)
        stamped = []
        with open(capture["path"], "r", encoding="utf-8", errors="replace") as handle:
            for raw in handle:
                line = re.sub(r"\x1b\[[0-9;]*m", "", raw.rstrip("\n"))
                match = LOG_LINE.match(line)
                if match:
                    stamped.append((int(match.group(2)), match.group(3)))
        if not stamped:
            raise ValueError(f"Capture has no timestamped lines: {capture['log']}")

        if state is None:
            if not (capture.get("candidate_elf_anchors") or capture.get("elf_file_hashes")):
                raise ValueError(f"Session requires one matching boot anchor: {capture['log']}")
            startup_counts(capture)
            marker = "CANDIDATE_ELF_SHA256=" if candidates else "ELF file SHA256:"
            anchor_indices = [i for i, (_, msg) in enumerate(stamped) if marker in msg]
            if len(anchor_indices) != 1:
                raise ValueError(f"Boot anchor must be timestamped exactly once: {capture['log']}")
            stamped = stamped[anchor_indices[0]:]
            state = {"last": -1}
            sessions[session] = state
        else:
            starts, resets = startup_counts(capture)
            if candidates or capture.get("elf_file_hashes") or starts or resets:
                raise ValueError(f"Reset in continuation; declare a new session: {capture['log']}")
            if stamped[0][0] <= state["last"]:
                raise ValueError(f"Overlapping/out-of-order continuation: {capture['log']}")

        for stamp, _ in stamped:
            if stamp < state["last"]:
                raise ValueError(f"Timestamp rollback in session: {capture['log']}")
            state["last"] = stamp


def build_matrix(boots, flashes, candidate_path, candidate, user_confirmations,
                 evidence_manifest=None) -> dict:
    expected_elf = candidate["files"].get("build/xiaozhi.elf", {}).get("sha256")
    _verify_boot_identity(boots, expected_elf, evidence_manifest)
    primary = boots[-1]
    signals, source = aggregate_signals(boots)
    health_all = [dict(sample, log=capture["log"])
                  for capture in boots for sample in capture["health"]]
    rows = build_rows(signals, source, flashes[-1] if flashes else {}, boots,
                      user_confirmations)
    camera_row = next(r for r in rows if r["item"] == "相机 RAW8 单帧取帧")
    sd_row = next(r for r in rows if r["item"] == "SD 卡单次挂载")
    power_row = next(r for r in rows if r["item"] == "电源键短按/长按识别")
    return {
        "schema": SCHEMA,
        "generated_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "candidate": {"path": os.path.basename(candidate_path),
                      "name": candidate.get("candidate"),
                      "xiaozhi_bin_sha256": candidate["files"]["build/xiaozhi.bin"]["sha256"],
                      "xiaozhi_bin_bytes": candidate["files"]["build/xiaozhi.bin"]["bytes"]},
        "user_confirmations": user_confirmations,
        "boot_captures": boots,
        "flash_captures": flashes,
        "signals": signals,
        "signal_source": source,
        "health": primary["health"],
        "health_all": health_all,
        "rows": rows,
        "open_items": [
            *(["唤醒词未捕获到正事件"] if not signals.get("wake_detected") else []),
            "恢复写回未实测，回滚不可信",
            *(["相机 RAW8 单帧取帧未通过硬件矩阵"]
              if camera_row["status"] != "PASS" else []),
            *(["SD 卡单次挂载未通过硬件矩阵"]
              if sd_row["status"] != "PASS" else []),
            *(["电源键短按/长按识别未通过硬件矩阵"]
              if power_row["status"] != "PASS" else []),
        ],
    }


def _cell(text: object) -> str:
    """Escape a value for a Markdown table cell."""
    return str(text).replace("|", "\\|").replace("\n", " ")


def render_markdown(matrix: dict) -> str:
    s = matrix["signals"]
    src = matrix.get("signal_source", {})
    out = [
        "# V6 M0 硬件验收矩阵（自动生成）",
        "",
        f"> 生成时间 {matrix['generated_at']}；schema `{matrix['schema']}`。",
        f"> 冻结候选 `{matrix['candidate']['path']}`，"
        f"应用 SHA256 `{matrix['candidate']['xiaozhi_bin_sha256'][:16]}…`。",
        f"> 证据来源 {len(matrix['boot_captures'])} 份启动捕获 / "
        f"{len(matrix['flash_captures'])} 份刷写捕获。",
        "> 数值型信号按**跨捕获取最大值**合并，避免后一份没点击的日志抹掉前一份的触摸证据；",
        "> 缺信号一律 NOT_VERIFIED，绝不由相邻项推断。",
        "",
        "| 项目 | 证据 | 状态 | 来源 | 说明 |",
        "| --- | --- | --- | --- | --- |",
    ]
    for r in matrix["rows"]:
        out.append(f"| {_cell(r['item'])} | `{_cell(r['evidence'])}` | "
                   f"**{r['status']}** | {_cell(r.get('source') or '—')} | {_cell(r['note'])} |")

    out += ["", "## 关键信号（跨捕获合并）", "", "| 信号 | 值 | 来源 |", "| --- | --- | --- |"]
    for key in sorted(s):
        out.append(f"| `{key}` | `{_cell(s[key])}` | {_cell(src.get(key) or '—')} |")

    health = matrix.get("health_all") or []
    out += ["", "## HEALTH 采样（全部捕获）", ""]
    if health:
        out += ["| 来源 | 时刻 (ms) | free | psram | WakeWordRunning | taps |",
                "| --- | --- | --- | --- | --- | --- |"]
        for h in health:
            out.append(f"| `{h['log']}` | {h['ts_ms']} | {h['free']} | {h['psram']} | "
                       f"{h['wake_word_running']} | {h['taps']} |")
        out += ["", "`WakeWordRunning` 是 `audio.IsWakeWordRunning()`。**注意方向**："
                    "`EnableWakeWordDetection(true)` 置位、**检测到唤醒时清位**，"
                    "该位只表示检测是否启用；0 或 1 都不能单独证明命中，采样也可能漏掉中间变化。"
                    "真正的正事件只有 `V6M0: WAKE_DETECTED`。"]
    else:
        out.append("所有捕获都没有 HEALTH 采样。这是证据缺口，不是通过。")

    out += ["", "## 未闭合项", ""]
    for item in matrix["open_items"]:
        out.append(f"- {item}")
    out.append("")
    return "\n".join(out)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="V6 M0 hardware matrix collector")
    parser.add_argument("command", choices=["scan"])
    parser.add_argument("--boot", action="append", required=True, help="UART boot capture")
    parser.add_argument("--flash", action="append", default=[], help="esptool flash capture")
    parser.add_argument("--candidate", required=True, help="frozen m0-candidate JSON")
    parser.add_argument("--evidence-manifest", default=None,
                        help="existing audio_evidence manifest binding continuation captures")
    parser.add_argument("--json-out", default=None)
    parser.add_argument("--md-out", default=None)
    parser.add_argument("--assume-display-visible", action="store_true", default=False)
    parser.add_argument("--assume-audio-loopback", action="store_true", default=False)
    args = parser.parse_args(argv)

    for path in [*args.boot, *args.flash, args.candidate, *([args.evidence_manifest] if args.evidence_manifest else [])]:
        if not os.path.exists(path):
            print(f"missing input: {path}", file=sys.stderr)
            return 2

    boots = [parse_boot_log(p) for p in args.boot]
    flashes = [parse_flash_log(p) for p in args.flash]
    with open(args.candidate, "r", encoding="utf-8") as handle:
        candidate = json.load(handle)

    confirmations = dict(DEFAULT_USER_CONFIRMATIONS)
    confirmations["display_visible"] = bool(args.assume_display_visible)
    confirmations["audio_loopback"] = bool(args.assume_audio_loopback)

    evidence_manifest = None
    if args.evidence_manifest:
        with open(args.evidence_manifest, "r", encoding="utf-8-sig") as handle:
            evidence_manifest = json.load(handle)
        # audio_evidence paths are relative to the manifest directory.
        for entry in evidence_manifest.get("captures", []):
            if isinstance(entry, dict) and isinstance(entry.get("path"), str):
                entry["path"] = os.path.join(os.path.dirname(os.path.abspath(args.evidence_manifest)), entry["path"])
    for boot, path in zip(boots, args.boot):
        boot["path"] = path
    matrix = build_matrix(boots, flashes, args.candidate, candidate, confirmations, evidence_manifest)

    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as handle:
            json.dump(matrix, handle, ensure_ascii=False, indent=2)
            handle.write("\n")
    if args.md_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.md_out)), exist_ok=True)
        with open(args.md_out, "w", encoding="utf-8") as handle:
            handle.write(render_markdown(matrix))
    if not args.json_out and not args.md_out:
        sys.stdout.write(render_markdown(matrix))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
