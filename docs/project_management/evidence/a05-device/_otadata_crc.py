import zlib,struct
b=open(r'E:\claw4-a05-build-m0\docs\project_management\evidence\a05-device\backups\backup-0x10e000-0x002000-otadata.bin','rb').read()
for i,off in enumerate((0,0x1000)):
    e=b[off:off+32]
    seq,=struct.unpack_from('<I',e,0)
    state,=struct.unpack_from('<I',e,24)
    crc,=struct.unpack_from('<I',e,28)
    calc=zlib.crc32(e[0:4])&0xFFFFFFFF
    print('sector%d ota_seq=%d state=0x%08X crc_stored=0x%08X crc_calc=0x%08X %s'%(i,seq,state,crc,calc,'OK' if crc==calc else 'MISMATCH'))
