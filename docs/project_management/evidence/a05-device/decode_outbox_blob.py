#!/usr/bin/env python3
"""A05-DEVICE-T1 pre-check: decode the device's persisted learning outbox straight
from a raw NVS partition dump (READ-ONLY).

The Claw4 device stores OutboxState as a UTF-8 text blob (magic "C4L1OUTBOX",
see learning/metalio_claw4/device/core/outbox_codec.{h,cpp}). NVS keeps an entry
as `span` 32-byte chunks: chunk0 = [ns,type,span,chunkIdx,crc32(4),key(16),data(8)],
so the blob's bytes are contiguous starting at entry_offset+24.

Format:
  C4L1OUTBOX
  D|<device_state>            T|<task_count>  + t| rows
  S|<0|1>                     + s| row
  E|<pending_count>           + p| rows
  Q|<next_sequence> A|<last_acked> F|<diag_failed> R|<diag_recovered>
  p| row = event_id, device_id, child_id, sequence, timestamp, ts_source,
           type, version, payload-map, dead_letter   (tab separated)

Usage: t1_outbox_decode.py <nvs_dump.bin> [...]
"""
import re
import sys

TEXT_MIN = 0x20


def printable_run(data, start):
    """Read forward from `start` while bytes look like the text blob."""
    out = bytearray()
    i = start
    while i < len(data):
        b = data[i]
        if b == 0xFF:
            break
        if b in (0x00,):
            break
        if b == 0x0A or (0x09 <= b <= 0x7F) or b >= 0x80:  # tab/LF/ascii/utf8
            out.append(b)
            i += 1
            continue
        break
    return bytes(out)


def decode(dump):
    data = open(dump, "rb").read()
    idxs = [m.start() for m in re.finditer(rb"C4L1OUTBOX\n", data)]
    print("=" * 72)
    print("dump: %s   size=%d   blob_magic_hits=%d" % (dump, len(data), len(idxs)))
    for n, magic_off in enumerate(idxs):
        # BLOB entries: 24-byte item header + 16-byte key + 8 bytes, so the first
        # data byte (the magic) sits at entry_offset + 32; continuation chunks are
        # 32 raw data bytes each.
        entry_off = magic_off - 32
        if entry_off < 0:
            continue
        ns, typ, span = data[entry_off], data[entry_off + 1], data[entry_off + 2]
        key = data[entry_off + 8:entry_off + 24].split(b"\x00")[0].decode("ascii", "replace")
        ln = (span - 1) * 32 if span >= 1 else 0
        raw = data[magic_off:magic_off + ln]
        text = raw.split(b"\xff")[0]
        pending = 0
        seqs = []
        for line in text.decode("utf-8", "replace").split("\n"):
            if line.startswith("p|"):
                f = line.split("\t")
                if len(f) == 10:
                    pending += 1
                    seqs.append((int(f[3]), f[6], f[0], f[9]))
        hdr = [x for x in text.decode("utf-8", "replace").split("\n")
               if x[:2] in ("E|", "Q|", "A|", "S|", "T|", "F|", "R|")]
        print("-" * 72)
        print("[%d] off=0x%06X ns=%d type=0x%02X span=%d key=%r text=%d B"
              % (n, entry_off, ns, typ, span, key, len(text)))
        print("   hdr=%s" % hdr)
        if seqs:
            seqs.sort()
            print("   pending=%d sequences=%s" % (pending, [s[0] for s in seqs]))
        else:
            print("   pending=0")


def main():
    for d in sys.argv[1:]:
        decode(d)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
