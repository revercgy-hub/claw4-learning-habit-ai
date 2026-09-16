import zlib, struct

ev = r"E:\claw4-a05-build-m0\docs\project_management\evidence\a05-device"
e = open(ev + r"\backups\backup-0x10e000-0x002000-otadata.bin", "rb").read()[:32]
target = struct.unpack_from("<I", e, 28)[0]
print("entry       =", " ".join("%02X" % b for b in e))
print("ota_seq     = %d" % struct.unpack_from("<I", e, 0)[0])
print("ota_state   = 0x%08X (2=ESP_OTA_IMG_VALID)" % struct.unpack_from("<I", e, 24)[0])
print("crc_stored  = 0x%08X" % target)


def crc_le(init, buf, final_xor=False):
    crc = init
    for c in buf:
        crc ^= c
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    return (crc ^ (0xFFFFFFFF if final_xor else 0)) & 0xFFFFFFFF


def crc_be(init, buf, final_xor=False):
    crc = init
    for c in buf:
        crc ^= (c << 24) & 0xFFFFFFFF
        for _ in range(8):
            crc = ((crc << 1) ^ (0x04C11DB7 if crc & 0x80000000 else 0)) & 0xFFFFFFFF
    return (crc ^ (0xFFFFFFFF if final_xor else 0)) & 0xFFFFFFFF


cands = {}
for L in (4, 8, 20, 24, 28, 32):
    cands["leL%d_iFF" % L] = crc_le(0xFFFFFFFF, e[:L])
    cands["leL%d_i0" % L] = crc_le(0, e[:L])
    cands["beL%d_iFF" % L] = crc_be(0xFFFFFFFF, e[:L])
pad = e[:28] + b"\xff\xff\xff\xff"
cands["le32_padff"] = crc_le(0xFFFFFFFF, pad)
pad2 = e[:28] + b"\x00\x00\x00\x00"
cands["le32_pad00"] = crc_le(0xFFFFFFFF, pad2)
cands["le4_zlib"] = zlib.crc32(e[:4]) & 0xFFFFFFFF

hit = [k for k, v in cands.items() if v == target]
print("---")
for k, v in cands.items():
    print("%-14s = 0x%08X %s" % (k, v, "<== HIT" if v == target else ""))
print("---")
print("HITS:", hit if hit else "NONE (stored CRC does not match any standard formula tested)")

# invert: find X with crc_le(0xFFFFFFFF, X_LE4) == target  (bijective over 32-bit)
def crc_le_byte(crc, c):
    crc ^= c
    for _ in range(8):
        crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    return crc & 0xFFFFFFFF

def crc_le_rev(crc, c):
    # inverse of crc_le_byte for one byte
    for _ in range(8):
        if crc & 0x80000000:
            crc = ((crc ^ 0xEDB88320) << 1 | 1) & 0xFFFFFFFF
        else:
            crc = (crc << 1) & 0xFFFFFFFF
    return crc ^ c

cur = target
x = 0
for i in range(4):
    b = crc_le_rev_byte = None
    # solve for byte i: try all 256
    found = None
    for cand_b in range(256):
        pass
    break
print("(inverse search skipped - not needed for gate decision)")
