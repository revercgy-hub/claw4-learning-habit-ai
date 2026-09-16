import sys
import time
import serial

PORT = "COM7"
BAUD = 115200

# argv: outfile duration_s
out = sys.argv[1]
dur = float(sys.argv[2])

p = serial.Serial(PORT, BAUD, timeout=0.05)
try:
    p.dtr = False
    p.rts = False
except Exception:
    pass
time.sleep(0.4)
try:
    p.reset_input_buffer()
except Exception:
    pass

# RTS 脉冲 = EN 复位（本设备 esptool 亦使用 "Hard resetting via RTS pin"）
p.rts = True
time.sleep(0.25)
p.rts = False
t_start = time.time()

buf = bytearray()
while time.time() - t_start < dur:
    try:
        data = p.read(8192)
    except Exception as e:
        data = b""
    if data:
        buf += data

p.close()
with open(out, "wb") as f:
    f.write(buf)
sys.stderr.write("captured %d bytes -> %s (dur=%.0fs)\n" % (len(buf), out, dur))
