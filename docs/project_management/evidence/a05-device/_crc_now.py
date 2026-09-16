import zlib,struct
b=open(r'E:\claw4-a05-build-m0\docs\project_management\evidence\a05-device\_now-otadata.bin','rb').read()
for i,off in enumerate((0,0x1000)):
    e=b[off:off+32]
    seq,=struct.unpack_from('<I',e,0); st,=struct.unpack_from('<I',e,24); crc,=struct.unpack_from('<I',e,28)
    calc=zlib.crc32(e[0:4])&0xFFFFFFFF
    print('sector%d seq=%d state=0x%08X crc_stored=0x%08X crc_calc(zlib)=0x%08X esprom=0x%08X match_esprom=%s'%(i,seq,st,crc,calc,calc^0xFFFFFFFF,(crc==(calc^0xFFFFFFFF))))
