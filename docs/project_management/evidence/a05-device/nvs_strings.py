#!/usr/bin/env python3
"""A05-DEVICE evidence helper: extract human-readable strings from a raw NVS partition dump.

Read-only analysis of an already-captured bin file. No device access.
Usage: nvs_strings.py <nvs_dump.bin> [min_len]
"""
import sys
import hashlib

PRINTABLE = set(range(0x20, 0x7F))


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: nvs_strings.py <nvs_dump.bin> [min_len]")
        return 2
    path = sys.argv[1]
    min_len = int(sys.argv[2]) if len(sys.argv) > 2 else 4

    with open(path, "rb") as fh:
        data = fh.read()

    print("file    = %s" % path)
    print("size    = %d" % len(data))
    print("sha256  = %s" % hashlib.sha256(data).hexdigest())
    print("non_ff  = %d" % sum(1 for b in data if b != 0xFF))
    print("")

    out = []
    cur = bytearray()
    start = 0
    for i, b in enumerate(data):
        if b in PRINTABLE:
            if not cur:
                start = i
            cur.append(b)
        else:
            if len(cur) >= min_len:
                out.append((start, bytes(cur).decode("ascii", "replace")))
            cur = bytearray()
    if len(cur) >= min_len:
        out.append((start, bytes(cur).decode("ascii", "replace")))

    print("strings (offset, value):")
    for off, s in out:
        print("  0x%08X  %s" % (off, s))
    print("")
    print("total_strings = %d" % len(out))

    # focus: the wifi namespace used by SsidManager (namespace "wifi", keys ssid/password/ota_url)
    print("")
    print("=== wifi-namespace key/value candidates ===")
    interesting = ("ssid", "password", "ota_url", "wifi", "http", "ws:", "wss:")
    for off, s in out:
        low = s.lower()
        if any(k in low for k in interesting):
            print("  0x%08X  %s" % (off, s))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
