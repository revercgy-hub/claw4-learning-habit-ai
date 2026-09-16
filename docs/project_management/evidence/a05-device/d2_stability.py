import sys
import time
import serial

PORT = "COM7"
out = sys.argv[1]
rounds = int(sys.argv[2])
dur = float(sys.argv[3])

p = serial.Serial(PORT, 115200, timeout=0.05)
try:
    p.dtr = False
    p.rts = False
except Exception:
    pass
time.sleep(0.4)

allbuf = bytearray()
stats = []
for r in range(rounds):
    # 复位
    p.rts = True
    time.sleep(0.25)
    p.rts = False
    t0 = time.time()
    got = 0
    while time.time() - t0 < dur:
        try:
            d = p.read(8192)
        except Exception:
            d = b""
        if d:
            allbuf += d
            got += len(d)
    stats.append((r + 1, got))

try:
    p.close()
except Exception:
    pass
open(out, "wb").write(bytes(allbuf))
sys.stderr.write("rounds_bytes=%r total=%d\n" % (stats, len(allbuf)))
