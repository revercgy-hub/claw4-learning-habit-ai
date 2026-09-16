import sys
import time
import serial

PORT = "COM7"
BAUD = 115200

out = sys.argv[1]
total = float(sys.argv[2])
pulse_at = [float(x) for x in sys.argv[3].split(",")] if len(sys.argv) > 3 else []

p = serial.Serial(PORT, BAUD, timeout=0.05)
try:
    p.dtr = False
    p.rts = False
except Exception:
    pass
time.sleep(0.4)

marks = []
buf = bytearray()
t0 = time.time()
next_pulse = 0

while time.time() - t0 < total:
    el = time.time() - t0
    if next_pulse < len(pulse_at) and el >= pulse_at[next_pulse]:
        marks.append((el, len(buf)))
        p.rts = True
        time.sleep(0.25)
        p.rts = False
        next_pulse += 1
    try:
        data = p.read(8192)
    except Exception:
        data = b""
    if data:
        buf += data

p.close()
with open(out, "wb") as f:
    f.write(buf)
sys.stderr.write("captured %d bytes -> %s ; pulses at %r ; bytes_before_each_pulse=%r\n"
                 % (len(buf), out, pulse_at, marks))
