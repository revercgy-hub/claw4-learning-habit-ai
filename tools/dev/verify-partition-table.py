#!/usr/bin/env python3
"""Verify the ACTUALLY GENERATED partition table against the approved CSV.

Rationale (A05 task book, reviewer ruling #3): the approved CSV contains BLANK
offsets (auto-resolved by the tool), so "the layout matches the approved input"
is an assumption unless the generated binary is decoded and compared row by row.

  * the approved CSV is used AS IS -- blank offsets are never filled in by hand
  * the generated `partition-table.bin` is decoded with the frozen IDF's
    gen_esp32part, using an explicit partition-table offset and flash size
  * the two decoded layouts are compared row by row (name/type/subtype/offset/
    size/flags), order included
  * overlap and range are checked
  * headroom comes from THIS candidate's app image only - never a historical one
  * app <= 4 MiB only shows both slots can hold it; it does NOT show dual-slot
    OTA works (that flow is outside this package)

Usage:
  <python> tools/dev/verify-partition-table.py \
      --csv  E:/c/partitions/v1/32m_dual.csv \
      --bin  <build>/partition_table/partition-table.bin \
      --app  <build>/<app>.bin \
      --table-offset 0x9000 --flash-size 32MB

  <python> tools/dev/verify-partition-table.py --csv <approved.csv> --self-test

Exit codes:
  0  every assertion held
  1  a real assertion failed (row differs / row count / overlap / range / ota
     mismatch / app exceeds ota_0), or an input was missing, or decoding failed
  2  usage error (neither --self-test nor both --bin and --app were given)

The --self-test mode builds REAL binaries with the frozen gen_esp32part from a
mutated copy of the approved CSV and asserts both the exit code and the emitted
field-level difference for each case. It exists because the first round only
covered the PASS path; the row-diff branch was therefore never executed, and a
mis-aligned printf argument list in it raised a TypeError the first time a real
mismatch was fed in. Evidence: ## CP1-REVIEW-FIX-001 in
docs/project_management/reports/WB_A05_BUILD_001_REPORT.md.
"""

import argparse
import hashlib
import os
import re
import subprocess
import sys
import tempfile

DEFAULT_GEN = (r"E:\workbuddy\esp-idf-5.5.4-ascii\components\partition_table"
               r"\gen_esp32part.py")
DEFAULT_IDF_PY = (r"E:\workbuddy\claw4-idf-tools\python_env"
                  r"\idf5.5_py3.12_env\Scripts\python.exe")

FLASH_SIZES = {
    "1MB": 1 << 20, "2MB": 2 << 20, "4MB": 4 << 20, "8MB": 8 << 20,
    "16MB": 16 << 20, "32MB": 32 << 20, "64MB": 64 << 20, "128MB": 128 << 20,
}

EXPECT_OTA0_OFFSET = 0x200000
EXPECT_OTA0_SIZE = 9437184          # 9 MiB
EXPECT_OTA1_OFFSET = 0xB00000
EXPECT_OTA1_SIZE = 4194304          # 4 MiB


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def size_to_bytes(text):
    m = re.fullmatch(r"\s*(\d+)\s*([KkMm]?)\s*", text)
    if not m:
        raise ValueError("unparsable size %r" % text)
    n = int(m.group(1))
    unit = m.group(2).upper()
    return n * (1 << 20 if unit == "M" else 1 << 10 if unit == "K" else 1)


ROW_DIFF_FIELDS = ("name", "type", "subtype", "offset", "size", "flags")


def format_field_value(key, value):
    """Render one field for diagnostics; numeric fields show hex AND decimal."""
    if key in ("offset", "size"):
        return "0x%x (%d)" % (value, value)
    return "%r" % (value,)


def describe_row_diff(expected_row, actual_row):
    """Field-level diff between two decoded rows, one line per differing field.

    Every entry names the field and shows BOTH sides. This deliberately does NOT
    use one big positional format string: the previous version had a mis-aligned
    argument list (an extra %s in the actual-side group, so `%d flags` consumed a
    string) and raised `TypeError: %d format: a real number is required, not str`
    the first time a real row mismatch was fed to it -- i.e. rc=1 came from a
    traceback instead of from the documented comparison path. See
    CP1-REVIEW-FIX-001 in WB_A05_BUILD_001_REPORT.md.
    """
    out = []
    for key in ROW_DIFF_FIELDS:
        exp_v, act_v = expected_row[key], actual_row[key]
        if exp_v != act_v:
            out.append("%s: expected %s vs actual %s"
                       % (key, format_field_value(key, exp_v), format_field_value(key, act_v)))
    return out


def _gen(gen_tool, idf_py, source, dest, flash_size, table_offset):
    """Run gen_esp32part once.

    NOTE the tool picks its OUTPUT format from the INPUT format, not from the
    output filename extension: a CSV input is always written out as BINARY, and
    a BIN input is always written out as CSV. So "CSV in -> CSV out" is not a
    mode that exists, and requesting it silently yields a binary file.
    """
    cmd = [idf_py, gen_tool, "--flash-size", flash_size,
           "--offset", table_offset, source, dest]
    r = subprocess.run(cmd, capture_output=True)
    if r.returncode != 0 or not os.path.isfile(dest):
        raise RuntimeError("gen_esp32part failed on %s: rc=%d %s\n%s" % (
            source, r.returncode, r.stderr.decode("utf-8", "replace"),
            r.stdout.decode("utf-8", "replace")))
    return dest


def _parse_csv(path):
    rows = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = [p.strip() for p in line.split(",")]
            if len(parts) < 5:
                continue
            name, typ, sub, off, size = parts[0], parts[1], parts[2], parts[3], parts[4]
            flags = parts[5] if len(parts) > 5 else ""
            rows.append({"name": name, "type": typ, "subtype": sub,
                         "offset": int(off, 16), "size": size_to_bytes(size),
                         "size_text": size, "flags": flags})
    if not rows:
        raise RuntimeError("decoded 0 partitions from %s" % path)
    return rows


def decode_bin(gen_tool, idf_py, bin_path, flash_size, table_offset):
    """Generated binary -> resolved layout (one conversion)."""
    out_dir = tempfile.mkdtemp(prefix="a05part_bin_")
    out_csv = os.path.join(out_dir, "from_bin.csv")
    _gen(gen_tool, idf_py, bin_path, out_csv, flash_size, table_offset)
    return _parse_csv(out_csv)


def decode_csv_resolved(gen_tool, idf_py, csv_path, flash_size, table_offset):
    """Approved CSV -> resolved layout.

    Two conversions are required because a CSV input is emitted as binary:
    CSV -> BIN (applies auto-offset resolution) then BIN -> CSV (readable).
    The approved CSV itself is never modified.
    """
    out_dir = tempfile.mkdtemp(prefix="a05part_csv_")
    mid_bin = os.path.join(out_dir, "from_csv.bin")
    out_csv = os.path.join(out_dir, "roundtrip.csv")
    _gen(gen_tool, idf_py, csv_path, mid_bin, flash_size, table_offset)
    _gen(gen_tool, idf_py, mid_bin, out_csv, flash_size, table_offset)
    return _parse_csv(out_csv)


# --------------------------------------------------------------------------
# Self-test: real row-mismatch negative cases (CP1-REVIEW-FIX-001)
#
# The first round only exercised the PASS path, so the row-diff branch was never
# executed and its broken printf argument list went unnoticed. These cases build
# REAL binaries with the frozen gen_esp32part from a mutated copy of the approved
# CSV -- the mismatch is a genuine decoded-layout difference, not a stubbed one.
# Each case asserts BOTH the exit code and the diagnostic text, and asserts that
# no Python traceback reached the child's stdout/stderr.
# --------------------------------------------------------------------------

SELF_TEST_APP_BYTES = 1 << 20      # 1 MiB synthetic placeholder (NOT candidate evidence)


def _csv_split(text):
    """-> (comment_lines, rows) with rows as lists of stripped cells."""
    comments, rows = [], []
    for line in text.splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            comments.append(line)
            continue
        rows.append([c.strip() for c in s.split(",")])
    return comments, rows


def _csv_render(comments, rows):
    return "\n".join(list(comments) + [", ".join(r) for r in rows]) + "\n"


def _csv_set_size(rows, name, new_size):
    rows = [list(r) for r in rows]
    for r in rows:
        if r[0] == name:
            r[4] = new_size
            return rows
    raise KeyError("no such partition: %s" % name)


def _csv_drop_row(rows, name):
    out = [list(r) for r in rows if r[0] != name]
    if len(out) == len(rows):
        raise KeyError("no such partition: %s" % name)
    return out


def _run_verifier(script, argv, cwd):
    """Invoke THIS script as a child process so the exit code is the real one."""
    cmd = [sys.executable, script] + argv
    r = subprocess.run(cmd, capture_output=True, cwd=cwd)
    return (r.returncode,
            r.stdout.decode("utf-8", "replace"),
            r.stderr.decode("utf-8", "replace"))


def run_self_test(args):
    script = os.path.abspath(__file__)
    for label, path in (("approved CSV", args.csv), ("gen tool", args.gen_tool),
                        ("idf python", args.idf_python)):
        if not os.path.isfile(path):
            print("SELF-TEST FAIL: %s not found: %s" % (label, path))
            return 1

    work = tempfile.mkdtemp(prefix="a05part_selftest_")
    with open(args.csv, encoding="utf-8") as f:
        comments, base_rows = _csv_split(f.read())

    app = os.path.join(work, "synthetic_app_1MiB.bin")
    with open(app, "wb") as f:
        f.write(b"\xE9" + b"\x00" * (SELF_TEST_APP_BYTES - 1))

    cases = [
        # label, mutated rows, expected rc, stdout must contain, stdout must NOT contain
        ("positive   : generated BIN == approved CSV",
         base_rows, 0,
         ["RESULT: PASS"], ["DIFF", "Traceback"]),
        ("negative-1 : one row's size differs (coredump 64K -> 128K)",
         _csv_set_size(base_rows, "coredump", "128K"), 1,
         ["RESULT: FAIL",
          "row differs for coredump: size: expected 0x10000 (65536) vs actual 0x20000 (131072)"],
         ["Traceback"]),
        ("negative-2 : one partition removed (row count 13 -> 12)",
         _csv_drop_row(base_rows, "coredump"), 1,
         ["ROW COUNT MISMATCH",
          "row count mismatch: expected 13 rows, actual 12",
          "RESULT: FAIL"],
         ["Traceback"]),
        ("negative-3 : ota_0 size differs -> cascading offsets downstream",
         _csv_set_size(base_rows, "ota_0", "8M"), 1,
         ["RESULT: FAIL", "row differs for ota_0", "row differs for ota_1",
          "row differs for ota_0: size: expected 0x900000 (9437184) vs actual 0x800000 (8388608)",
          "row differs for ota_1: offset: expected 0xb00000 (11534336) vs actual 0xa00000 (10485760)"],
         ["Traceback"]),
    ]

    print("self-test workspace  : %s" % work)
    print("approved CSV         : %s" % args.csv)
    print("synthetic app        : %d bytes (placeholder, NOT candidate evidence)"
          % SELF_TEST_APP_BYTES)
    print("")

    bad = 0
    for i, (label, rows, want_rc, want_in, want_out) in enumerate(cases, 1):
        case_dir = os.path.join(work, "case%d" % i)
        os.makedirs(case_dir)
        csv_path = os.path.join(case_dir, "mutated.csv")
        bin_path = os.path.join(case_dir, "partition-table.bin")
        with open(csv_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(_csv_render(comments, rows))
        try:
            _gen(args.gen_tool, args.idf_python, csv_path, bin_path,
                 args.flash_size, args.table_offset)
        except Exception as exc:  # noqa: BLE001
            print("  %-62s GENERATION FAILED: %s" % (label, exc))
            bad += 1
            continue

        rc, out, err = _run_verifier(script, [
            "--csv", args.csv, "--bin", bin_path, "--app", app,
            "--gen-tool", args.gen_tool, "--idf-python", args.idf_python,
            "--table-offset", args.table_offset, "--flash-size", args.flash_size,
        ], case_dir)

        problems = []
        if rc != want_rc:
            problems.append("rc=%d want %d" % (rc, want_rc))
        for needle in want_in:
            if needle not in out:
                problems.append("stdout missing %r" % needle)
        for needle in want_out:
            if needle in out:
                problems.append("stdout unexpectedly contains %r" % needle)
        # gen_esp32part writes progress chatter to stderr, so assert on tracebacks
        # specifically rather than on stderr being empty.
        if "Traceback" in err or "Traceback" in out:
            problems.append("python traceback present")
        if "TypeError" in err or "TypeError" in out:
            problems.append("TypeError present")

        print("  %-62s rc=%d %s" % (label, rc, "OK" if not problems else "PROBLEM"))
        if problems:
            bad += 1
            for p in problems:
                print("      - %s" % p)
            print("      --- stdout ---")
            for line in out.splitlines():
                print("      | %s" % line)
            print("      --- stderr ---")
            for line in err.splitlines():
                print("      | %s" % line)

    print("")
    if bad:
        print("SELF-TEST: FAIL (%d problem(s))" % bad)
        return 1
    print("SELF-TEST: PASS -- %d/%d cases behaved as specified "
          "(positive rc=0; every real mismatch rc=1 with a field-level diff and no traceback)"
          % (len(cases), len(cases)))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--csv", required=True, help="approved partition CSV (used as-is)")
    ap.add_argument("--bin", help="generated partition-table.bin (required unless --self-test)")
    ap.add_argument("--app", help="this candidate's app image (.bin) (required unless --self-test)")
    ap.add_argument("--gen-tool", default=DEFAULT_GEN)
    ap.add_argument("--idf-python", default=DEFAULT_IDF_PY)
    ap.add_argument("--table-offset", default="0x9000")
    ap.add_argument("--flash-size", default="32MB", choices=sorted(FLASH_SIZES))
    ap.add_argument("--self-test", action="store_true",
                    help="run the row-diff negative tests and exit (no --bin/--app needed)")
    args = ap.parse_args()

    if args.self_test:
        return run_self_test(args)

    missing = [n for n, v in (("--bin", args.bin), ("--app", args.app)) if not v]
    if missing:
        for n in missing:
            print("FAIL %s is required (or use --self-test)" % n)
        return 2

    failures = []
    for label, path in (("csv", args.csv), ("bin", args.bin), ("app", args.app),
                        ("gen tool", args.gen_tool), ("idf python", args.idf_python)):
        if not os.path.isfile(path):
            failures.append("%s not found: %s" % (label, path))
    if failures:
        for f in failures:
            print("FAIL %s" % f)
        return 1

    flash_bytes = FLASH_SIZES[args.flash_size]

    print("approved CSV        : %s" % args.csv)
    print("  sha256            : %s" % sha256_file(args.csv))
    print("generated BIN       : %s" % args.bin)
    print("  sha256            : %s" % sha256_file(args.bin))
    print("app image           : %s" % args.app)
    print("  bytes             : %d" % os.path.getsize(args.app))
    print("  sha256            : %s" % sha256_file(args.app))
    print("gen tool sha256     : %s" % sha256_file(args.gen_tool))
    print("table offset        : %s" % args.table_offset)
    print("flash size          : %s (%d bytes)" % (args.flash_size, flash_bytes))
    print("")

    try:
        expected = decode_csv_resolved(args.gen_tool, args.idf_python, args.csv,
                                       args.flash_size, args.table_offset)
        actual = decode_bin(args.gen_tool, args.idf_python, args.bin,
                            args.flash_size, args.table_offset)
    except Exception as exc:  # noqa: BLE001
        print("FAIL decoding: %s" % exc)
        return 1

    print("row-by-row comparison (name/type/subtype/offset/size/flags):")
    print("  %-14s %-6s %-9s %-11s %-11s %s" % ("name", "type", "subtype", "offset", "size", "verdict"))
    n = max(len(expected), len(actual))
    for i in range(n):
        e = expected[i] if i < len(expected) else None
        a = actual[i] if i < len(actual) else None
        if e is None or a is None:
            label = (a or e)["name"]
            print("  %-14s  ROW COUNT MISMATCH (expected %d rows, actual %d)"
                  % (label, len(expected), len(actual)))
            failures.append("row count mismatch: expected %d rows, actual %d (first unmatched: %s)"
                            % (len(expected), len(actual), label))
            continue
        diffs = describe_row_diff(e, a)
        if diffs:
            failures.append("row differs for %s: %s" % (e["name"], "; ".join(diffs)))
        print("  %-14s %-6s %-9s 0x%08x  %-11d %s"
              % (a["name"], a["type"], a["subtype"], a["offset"], a["size"],
                 "OK" if not diffs else "DIFF"))

    # overlap / range
    print("")
    ordered = sorted(actual, key=lambda r: r["offset"])
    for i in range(1, len(ordered)):
        prev, cur = ordered[i - 1], ordered[i]
        if prev["offset"] + prev["size"] > cur["offset"]:
            failures.append("OVERLAP: %s ends 0x%x beyond %s start 0x%x"
                            % (prev["name"], prev["offset"] + prev["size"], cur["name"], cur["offset"]))
    last = ordered[-1]
    end = last["offset"] + last["size"]
    if end > flash_bytes:
        failures.append("OUT OF RANGE: last partition %s ends 0x%x > flash 0x%x" % (last["name"], end, flash_bytes))

    # explicit ota assertions
    by_name = {r["name"]: r for r in actual}
    for name, off, size in (("ota_0", EXPECT_OTA0_OFFSET, EXPECT_OTA0_SIZE),
                            ("ota_1", EXPECT_OTA1_OFFSET, EXPECT_OTA1_SIZE)):
        r = by_name.get(name)
        if r is None:
            failures.append("%s partition absent" % name)
            continue
        if r["offset"] != off:
            failures.append("%s offset 0x%x != expected 0x%x" % (name, r["offset"], off))
        if r["size"] != size:
            failures.append("%s size %d != expected %d" % (name, r["size"], size))

    # headroom from THIS candidate only
    app_bytes = os.path.getsize(args.app)
    print("")
    print("-- headroom (this candidate only; no historical values) --")
    if "ota_0" in by_name:
        o0 = by_name["ota_0"]
        head = o0["size"] - app_bytes
        print("ota_0 slot      : %d bytes" % o0["size"])
        print("app image       : %d bytes" % app_bytes)
        print("ota_0 headroom  : %d bytes (%.2f%%)" % (head, 100.0 * head / o0["size"]))
        if head < 0:
            failures.append("app image %d EXCEEDS ota_0 slot %d -- HARD STOP" % (app_bytes, o0["size"]))
    if "ota_1" in by_name:
        o1 = by_name["ota_1"]
        fits = app_bytes <= o1["size"]
        print("ota_1 slot      : %d bytes" % o1["size"])
        print("app fits ota_1  : %s" % fits)
        if not fits:
            print("NOTE: app > ota_1 -> DUAL-SLOT OTA UNUSABLE; record explicitly and")
            print("      submit application-only evidence. Do not change partitions or OTA policy.")
            print("      (dual-slot OTA flow/selection/rollback is OUTSIDE this package either way.)")
        else:
            print("NOTE: app <= ota_1 only shows both slots can hold it. It does NOT")
            print("      demonstrate that dual-slot OTA works.")
    print("last partition end: 0x%08x   trailing unallocated: %d bytes"
          % (end, flash_bytes - end))

    print("")
    if failures:
        print("RESULT: FAIL (%d)" % len(failures))
        for f in failures:
            print("  - %s" % f)
        return 1
    print("RESULT: PASS -- generated partition table matches the approved input, no overlap, in range")
    return 0


if __name__ == "__main__":
    sys.exit(main())
