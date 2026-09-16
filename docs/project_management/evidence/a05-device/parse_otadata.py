import sys, struct, zlib

path = sys.argv[1]
data = open(path, "rb").read()
print("file=%s size=%d" % (path, len(data)))

OTA_UNDEFINED = 0xFFFFFFFF
OTA_PENDING_VERIFY = 0xFFFFFFFE
OTA_VALID = 0xFFFFFFFD
STATE = {OTA_UNDEFINED: "UNDEFINED", OTA_PENDING_VERIFY: "PENDING_VERIFY", OTA_VALID: "VALID", 0: "INVALID"}

entries = []
for i, off in enumerate((0x0000, 0x1000)):
    if off + 32 > len(data):
        print("sector %d: OUT OF RANGE" % i)
        continue
    blk = data[off:off + 32]
    ota_seq, = struct.unpack_from("<I", blk, 0)
    label = blk[4:24]
    ota_state, = struct.unpack_from("<I", blk, 24)
    crc_stored, = struct.unpack_from("<I", blk, 28)
    crc_calc = zlib.crc32(blk[:28]) & 0xFFFFFFFF
    ok = (crc_stored == crc_calc)
    lab = label.split(b"\x00")[0].decode("ascii", "replace")
    print("sector %d @0x%04x: ota_seq=%s(0x%08X) seq_label=%r ota_state=%s(0x%08X) crc_stored=0x%08X crc_calc=0x%08X crc_%s"
          % (i, off, ("%d" % ota_seq) if ota_seq != 0xFFFFFFFF else "INVALID",
             ota_seq, lab, STATE.get(ota_state, "OTHER(0x%08X)" % ota_state), ota_state,
             crc_stored, crc_calc, "OK" if ok else "MISMATCH"))
    entries.append((ota_seq, ota_state, ok))

valid = [e for e in entries if e[1] == OTA_VALID and e[2]]
print("---")
print("valid_crc_entries=%d" % len(valid))
if not valid:
    print("BOOT_SELECTION: no CRC-valid otadata entry -> boot from partition-table default (first ota_0 app)")
else:
    seq = max(e[0] for e in valid)
    idx = (seq - 1) % 2
    print("BOOT_SELECTION: max_valid_ota_seq=%d -> ota_partition_index=%d -> %s" % (seq, idx, "ota_0" if idx == 0 else "ota_1"))
