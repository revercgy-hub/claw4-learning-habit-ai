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
    # --- touch --------------------------------------------------------------
    ("touch_init",          "line",  r"Claw4V6: GT911 touch initialized"),
    ("touch_last_count",    "int",   r"V6M0: TOUCH count=(\d+)"),
    ("touch_record_cycles", "count", r"V6M0: TOUCH count=\d+; audio record begin"),
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
    last_ts = 0
    lines = 0

    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            line = re.sub(r"\x1b\[[0-9;]*m", "", raw.rstrip("\n"))
            elf = re.search(r"(?:ELF file SHA256:\s*|CANDIDATE_ELF_SHA256=)([0-9a-f]{8,64})", line)
            if elf: elf_hashes.add(elf.group(1))
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
        "sha256": sha256_file(path),
        "bytes": os.path.getsize(path),
        "lines": lines,
        "last_timestamp_ms": last_ts,
        "elf_hashes": sorted(elf_hashes),
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

    # Not integrated -------------------------------------------------------
    row("SD 卡 / Camera / 电源键", "no signal in candidate",
        "NOT_TESTED", "not integrated in m0.1")

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


def build_matrix(boots, flashes, candidate_path, candidate, user_confirmations) -> dict:
    expected_elf = candidate["files"].get("build/xiaozhi.elf", {}).get("sha256")
    for capture in boots:
        for observed in capture.get("elf_hashes", []):
            if not expected_elf or not expected_elf.startswith(observed):
                raise ValueError(f"Candidate ELF mismatch: {capture['log']}")
    primary = boots[-1]
    signals, source = aggregate_signals(boots)
    health_all = [dict(sample, log=capture["log"])
                  for capture in boots for sample in capture["health"]]
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
        "rows": build_rows(signals, source, flashes[-1] if flashes else {}, boots,
                           user_confirmations),
        "open_items": [
            *(["唤醒词未捕获到正事件"] if not signals.get("wake_detected") else []),
            "恢复写回未实测，回滚不可信",
            "SD 卡 / Camera / 电源键未集成",
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
    parser.add_argument("--json-out", default=None)
    parser.add_argument("--md-out", default=None)
    parser.add_argument("--assume-display-visible", action="store_true", default=False)
    parser.add_argument("--assume-audio-loopback", action="store_true", default=False)
    args = parser.parse_args(argv)

    for path in [*args.boot, *args.flash, args.candidate]:
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

    matrix = build_matrix(boots, flashes, args.candidate, candidate, confirmations)

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
