import zlib,struct
b=open(r'E:\claw4-a05-build-m0\docs\project_management\evidence\a05-device\backups\backup-0x10e000-0x002000-otadata.bin','rb').read()
e=b[0:32]
target=struct.unpack_from('<I',e,28)[0]
print('stored crc = 0x%08X'%target)
def variants(buf):
    out={}
    out['zlib_finalxor']   = zlib.crc32(buf)&0xFFFFFFFF
    out['esprom_nofinal']  = (zlib.crc32(buf)&0xFFFFFFFF)^0xFFFFFFFF
    out['init0_finalxor']  = (zlib.crc32(buf)&0xFFFFFFFF)  # same as above
    return out
for L in (4,8,20,24,28,32):
    for name,v in variants(e[:L]).items():
        if v==target: print('HIT len=%d %s'%(L,name))
    print('len=%2d  zlib=0x%08X  esprom=0x%08X'%(L, zlib.crc32(e[:L])&0xFFFFFFFF, (zlib.crc32(e[:L])&0xFFFFFFFF)^0xFFFFFFFF))
# also try with crc field zeroed, and crc32 of ota_seq with seed 0 (raw table, no init inversion)
def crc_le(init, buf):
    crc=init
    for c in buf:
        crc ^= c
        for _ in range(8):
            crc = (crc>>1) ^ (0xEDB88320 if crc&1 else 0)
    return crc & 0xFFFFFFFF
print('raw crc_le(0xFFFFFFFF,[01,00,00,00]) = 0x%08X'%crc_le(0xFFFFFFFF, e[:4]))
print('raw crc_le(0,         [01,00,00,00]) = 0x%08X'%crc_le(0, e[:4]))
