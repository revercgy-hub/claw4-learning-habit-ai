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
  <idf python> tools/dev/verify-partition-table.py \
      --csv  E:/c/partitions/v1/32m_dual.csv \
      --bin  <build>/partition_table/partition-table.bin \
      --app  <build>/<app>.bin \
      --table-offset 0x9000 --flash-size 32MB
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


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--csv", required=True, help="approved partition CSV (used as-is)")
    ap.add_argument("--bin", required=True, help="generated partition-table.bin")
    ap.add_argument("--app", required=True, help="this candidate's app image (.bin)")
    ap.add_argument("--gen-tool", default=DEFAULT_GEN)
    ap.add_argument("--idf-python", default=DEFAULT_IDF_PY)
    ap.add_argument("--table-offset", default="0x9000")
    ap.add_argument("--flash-size", default="32MB", choices=sorted(FLASH_SIZES))
    args = ap.parse_args()

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
            print("  %-14s  ROW COUNT MISMATCH (expected %d rows, actual %d)"
                  % (a["name"] if a else e["name"], len(expected), len(actual)))
            failures.append("row count mismatch")
            continue
        same = (e["name"] == a["name"] and e["type"] == a["type"]
                and e["subtype"] == a["subtype"] and e["offset"] == a["offset"]
                and e["size"] == a["size"] and e["flags"] == a["flags"])
        if not same:
            failures.append("row differs for %s: expected %s/%s/%s/0x%x/%d flags=%r vs actual %s/%s/%s/0x%x/%d flags=%r"
                            % (e["name"], e["type"], e["subtype"], e["offset"], e["size"], e["flags"],
                               a["type"], a["subtype"], a["offset"], a["size"], a["flags"]))
        print("  %-14s %-6s %-9s 0x%08x  %-11d %s"
              % (a["name"], a["type"], a["subtype"], a["offset"], a["size"],
                 "OK" if same else "DIFF"))

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
