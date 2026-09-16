#!/usr/bin/env python3
"""A05-DEVICE evidence helper: byte-level diff of two NVS partition dumps.

Prints every differing region with surrounding printable context, so we can see
exactly which NVS entries a write appended/updated. Read-only.
Usage: nvs_diff.py <old.bin> <new.bin> [context]
"""
import sys


def printable(bs):
    return "".join(chr(b) if 32 <= b < 127 else "." for b in bs)


def main():
    if len(sys.argv) < 3:
        print("usage: nvs_diff.py <old.bin> <new.bin> [context]")
        return 2
    old = open(sys.argv[1], "rb").read()
    new = open(sys.argv[2], "rb").read()
    ctx = int(sys.argv[3]) if len(sys.argv) > 3 else 48

    if len(old) != len(new):
        print("SIZE MISMATCH old=%d new=%d" % (len(old), len(new)))
    n = min(len(old), len(new))

    regions = []
    i = 0
    while i < n:
        if old[i] != new[i]:
            start = i
            gap = 0
            j = i
            while j < n and gap < 16:
                if old[j] != new[j]:
                    gap = 0
                    j += 1
                else:
                    gap += 1
                    j += 1
            end = j - gap
            regions.append((start, end))
            i = end
        else:
            i += 1

    print("old = %s" % sys.argv[1])
    print("new = %s" % sys.argv[2])
    print("size = %d" % n)
    print("total_changed_bytes = %d" % sum(e - s for s, e in regions))
    print("changed_regions = %d" % len(regions))
    print("")
    for s, e in regions:
        cs = max(0, s - ctx)
        ce = min(n, e + ctx)
        print("--- 0x%06X..0x%06X  (%d bytes)  pages 0x%X / 0x%X ---" % (s, e, e - s, s // 4096, e // 4096))
        print("  old: %s" % printable(old[cs:ce]))
        print("  new: %s" % printable(new[cs:ce]))
        aa = new[s:min(e, s + 64)].hex(" ")
        print("  new_bytes: %s" % aa)
        print("")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
