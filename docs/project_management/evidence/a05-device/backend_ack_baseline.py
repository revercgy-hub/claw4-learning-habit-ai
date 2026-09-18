#!/usr/bin/env python3
"""A05-DEVICE-T1 pre-check (READ-ONLY): report the ACK baseline stored in each
candidate backend DB, plus the sequence numbers already recorded for the target
device. Never prints secrets.
Usage: t1_ack_baseline.py <db> [<db> ...]
"""
import sqlite3
import sys

DEVICE_ID = "988566fb-…"


def main() -> int:
    for db in sys.argv[1:]:
        print("=" * 70)
        print("db: %s" % db)
        con = sqlite3.connect("file:%s?mode=ro" % db, uri=True)
        cur = con.cursor()
        rows = list(cur.execute(
            "SELECT device_id, model, fw_version, installation_id,"
            " last_acked_sequence, created_at FROM devices"))
        for r in rows:
            mark = "  <== TARGET" if r[0] == DEVICE_ID else ""
            print("  device=%s model=%s fw=%s inst=%s last_acked=%s%s"
                  % (r[0], r[1], r[2], r[3], r[4], mark))
        print("  device_children: %s" % list(cur.execute(
            "SELECT device_id, child_id FROM device_children"))[:4])
        print("  events for target (seq, type, state, event_id, received_at):")
        for r in cur.execute(
                "SELECT sequence, type, state, event_id, received_at FROM events"
                " WHERE device_id=? ORDER BY sequence", (DEVICE_ID,)):
            print("    seq=%-4s type=%-28s state=%-10s id=%s at=%s"
                  % (r[0], r[1], r[2], r[3], r[4]))
        print("  max recorded sequence: %s" % cur.execute(
            "SELECT MAX(sequence) FROM events WHERE device_id=?",
            (DEVICE_ID,)).fetchone()[0])
        print("  device_configs: %s" % list(cur.execute(
            "SELECT device_id, online, last_seen_at, last_sync_at FROM device_configs"
            " WHERE device_id=?", (DEVICE_ID,))))
        print("  study_sessions for target: %d" % cur.execute(
            "SELECT COUNT(*) FROM study_sessions WHERE device_id=?",
            (DEVICE_ID,)).fetchone()[0])
        con.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
