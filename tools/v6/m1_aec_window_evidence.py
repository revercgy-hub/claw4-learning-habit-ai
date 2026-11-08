"""Extract only S-24 numeric diagnostic records from a bounded UART capture."""

import argparse
import json
import re
from pathlib import Path

from m1_voice_log_parser import APP_INIT_ELF, MAX_U32, MAX_U64

MAX_INPUT = 8 * 1024 * 1024
MAX_LINE = 512
MAX_TIMER = (1 << 63) - 1
CLOCK_FIELDS = {"t0", "t1", "t_us"}
ENERGY_FIELDS = {"mic_sum_sq", "ref_sum_sq"}
APP_PREFIX = re.compile(r"^[IWEVD] \([0-9]{1,10}\) app_init: ELF file SHA256:")
ROM_BOOT = re.compile(r"^ESP-ROM:esp32p4(?:-[a-zA-Z0-9_-]+)?$")
COLOR_PREFIX = re.compile(r"^\x1b\[[0-9;]{1,12}m")
COLOR_SUFFIX = re.compile(r"\x1b\[[0-9;]{1,12}m$")

WINDOW = "M1_AUDIO_WINDOW"
ENERGY = "M1_AUDIO_ENERGY"
AFE = "M1_AFE_AEC"
FIELDS = {
    WINDOW: ("Claw4Audio", "t0 t1 rx rx_fail rx_fail_tx tx ref_ok ref_zero ref_short short_tx"),
    ENERGY: ("Claw4Audio", "t1 mic_sum_sq ref_sum_sq tx_calls tx_write_us_max inactive underflow active pending"),
    AFE: ("AfeAudioEngine", "t_us requested apply_total latest_rc"),
}
RULES = {
    kind: (
        re.compile(rf"^I \(([0-9]{{1,10}})\) {tag}: {kind}(?=\s|$)"),
        re.compile(rf"^I \([0-9]{{1,10}}\) {tag}: {kind}" + "".join(
            rf" {key}=(-?[0-9]{{1,10}})" if key == "latest_rc" else rf" {key}=([0-9]{{1,20}})"
            for key in fields.split()
        )),
        fields.split(),
    )
    for kind, (tag, fields) in FIELDS.items()
}


def _bounded(name, value):
    if name == "latest_rc":
        return -(1 << 31) <= value < (1 << 31)
    limit = MAX_TIMER if name in CLOCK_FIELDS else MAX_U64 if name in ENERGY_FIELDS else MAX_U32
    return 0 <= value <= limit


def _valid_pair(window, energy):
    return (
        window["t1"] == energy["t1"]
        and window["ref_ok"] + window["ref_short"] == window["rx"]
        and window["ref_zero"] <= window["ref_ok"]
        and window["short_tx"] <= window["ref_short"]
        and window["rx_fail_tx"] <= window["rx_fail"]
        and energy["inactive"] + energy["underflow"] == window["ref_short"]
        and energy["active"] <= 1
        and energy["pending"] <= 8192
        and energy["mic_sum_sq"] <= window["rx"] * (1 << 30)
        and energy["ref_sum_sq"] <= window["ref_ok"] * (1 << 30)
        and (window["tx"] == 0 or energy["tx_calls"] > 0)
    )


def summarize(raw, expected_elf):
    if not re.fullmatch(r"[0-9a-fA-F]{64}", expected_elf):
        raise ValueError("Expected a full SHA-256 ELF digest")
    expected_elf = expected_elf.lower()

    def result(status, reason=None, **values):
        return {"status": status, "reason": reason, "expected_elf_sha256": expected_elf,
                "scope": "CODEC_WINDOWS_AND_AFE_REQUEST_SNAPSHOTS_ONLY",
                "windows": [], "afe": [], **values}

    if len(raw) > MAX_INPUT:
        return result("INVALID", "INPUT_TOO_LARGE")

    anchor = None
    pending = None
    windows, afe = [], []
    lines = raw.decode("utf-8", "replace").split("\n")
    for i, source in enumerate(lines):
        line = COLOR_SUFFIX.sub("", COLOR_PREFIX.sub("", source.rstrip("\r")))
        complete = i < len(lines) - 1
        if APP_PREFIX.match(line):
            match = APP_INIT_ELF.fullmatch(line)
            if not complete or not match or int(match[1]) > MAX_U32:
                return result("INVALID", "MALFORMED_ELF_ANCHOR")
            prefix = match[2].lower()
            if anchor is not None or not expected_elf.startswith(prefix):
                return result("INVALID", "MULTIPLE_OR_MISMATCHED_ELF")
            anchor = prefix
            continue
        if anchor is None:
            continue
        if ROM_BOOT.fullmatch(line):
            return result("INVALID", "REBOOT_AFTER_ANCHOR")
        for kind, (prefix_rule, value_rule, names) in RULES.items():
            prefix_match = prefix_rule.match(line)
            if not prefix_match:
                continue
            match = value_rule.fullmatch(line) if complete and len(line) <= MAX_LINE else None
            if not match or int(prefix_match[1]) > MAX_U32:
                return result("INVALID", "MALFORMED_DIAGNOSTIC")
            row = dict(zip(names, map(int, match.groups())))
            if not all(_bounded(name, value) for name, value in row.items()):
                return result("INVALID", "DIAGNOSTIC_RANGE")
            row["log_uptime_ms"] = int(prefix_match[1])
            if kind == WINDOW:
                if (pending is not None or row["t0"] <= 0 or row["t1"] - row["t0"] < 1_000_000
                        or (windows and row["t0"] != windows[-1]["t1"])):
                    return result("INVALID", "WINDOW_ORDER_OR_GAP")
                pending = row
            elif kind == ENERGY:
                if pending is None or not _valid_pair(pending, row):
                    return result("INVALID", "UNPAIRED_OR_INCONSISTENT_ENERGY")
                window = {**pending, **{k: row[k] for k in names if k != "t1"}}
                windows.append(window)
                pending = None
            else:
                if (row["t_us"] <= 0 or row["requested"] > 1 or
                        (afe and (row["t_us"] <= afe[-1]["t_us"] or row["apply_total"] < afe[-1]["apply_total"]))):
                    return result("INVALID", "AFE_ORDER_OR_RANGE")
                if row["apply_total"] == 0:
                    row.update(requested=None, latest_rc=None)
                afe.append(row)
            break
    if anchor is None:
        return result("NOT_VERIFIED", "ELF_ANCHOR_MISSING")
    if pending is not None:
        return result("INVALID", "WINDOW_ENERGY_MISSING")
    if not windows or not afe:
        return result("NOT_VERIFIED", "DIAGNOSTICS_MISSING")
    return result("DIAGNOSTICS_PARSED", anchor_prefix=anchor, windows=windows, afe=afe)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--expected-elf-sha256", required=True)
    args = parser.parse_args(argv)
    try:
        with args.capture.open("rb") as capture:
            summary = summarize(capture.read(MAX_INPUT + 1), args.expected_elf_sha256)
        with args.output.open("x", encoding="utf-8", newline="\n") as output:
            json.dump(summary, output, indent=2)
            output.write("\n")
    except ValueError:
        parser.error("Expected a full SHA-256 ELF digest")
    except OSError:
        parser.error("Could not read capture or create a new output file")
    return 0 if summary["status"] == "DIAGNOSTICS_PARSED" else 1


if __name__ == "__main__":
    raise SystemExit(main())
