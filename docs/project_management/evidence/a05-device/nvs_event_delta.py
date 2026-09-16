#!/usr/bin/env python3
"""A05-DEVICE evidence helper: compare the learning outbox event-id set between two NVS dumps.

The device stores its pending-sync outbox as utf-8 text inside NVS (blob key C4L1OUTBOX).
Every queued event carries a 16-hex-digit id rendered as "p|ev-<id>". Comparing the set of
distinct ids tells us whether new events were appended between two snapshots (vs. an NVS
page GC that only moves identical bytes). Read-only.
Usage: nvs_event_delta.py <old.bin> <new.bin>
"""
import re
import sys

EV = re.compile(rb"p\|ev-([0-9a-f]{16})")
SESS = re.compile(rb"s\|(sess-[0-9a-f]{16})")
TASK = re.compile(rb"t\|([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})")
SEQ = re.compile(rb"\b(\d{1,3})\.\d{1,3}\.1\.[0-9]\.")


def ids(path, rx):
    data = open(path, "rb").read()
    return [m.decode() for m in rx.findall(data)]


def main():
    if len(sys.argv) < 3:
        print("usage: nvs_event_delta.py <old.bin> <new.bin>")
        return 2
    old, new = sys.argv[1], sys.argv[2]
    eo, en = ids(old, EV), ids(new, EV)
    so, sn = ids(old, SESS), ids(new, SESS)
    to, tn = ids(old, TASK), ids(new, TASK)

    print("=== event ids (p|ev-*) ===")
    print("old: occurrences=%d distinct=%d" % (len(eo), len(set(eo))))
    print("new: occurrences=%d distinct=%d" % (len(en), len(set(en))))
    added = sorted(set(en) - set(eo))
    removed = sorted(set(eo) - set(en))
    print("added_ids   = %d %s" % (len(added), added[:20]))
    print("removed_ids = %d %s" % (len(removed), removed[:20]))
    print("")
    print("=== session ids ===")
    print("old distinct = %s" % sorted(set(so)))
    print("new distinct = %s" % sorted(set(sn)))
    print("")
    print("=== task ids ===")
    print("old distinct = %s" % sorted(set(to)))
    print("new distinct = %s" % sorted(set(tn)))
    print("")
    print("=== verdict ===")
    if added:
        print("NEW_EVENTS_APPENDED = True  (learning runtime generated %d new event id(s))" % len(added))
    elif set(en) == set(eo):
        print("NEW_EVENTS_APPENDED = False (identical event-id set; any byte move is NVS page GC)")
    else:
        print("CHANGED (ids removed but none added - possible flush)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
