import sys
import time
import threading
import serial
import serial.tools.list_ports

PORT = "COM7"
out = sys.argv[1]
total = float(sys.argv[2])

buf = bytearray()
lock = threading.Lock()
stop = False
reopens = []


def reader():
    global stop
    p = None
    while not stop:
        if p is None:
            try:
                p = serial.Serial(PORT, 115200, timeout=0.05)
                try:
                    p.dtr = False
                    p.rts = False
                except Exception:
                    pass
                reopens.append(round(time.time() - t0, 2))
            except Exception:
                p = None
                time.sleep(0.2)
                continue
        try:
            d = p.read(4096)
        except Exception:
            d = b""
            try:
                p.close()
            except Exception:
                pass
            p = None
            continue
        if d:
            with lock:
                buf.extend(d)


t0 = time.time()
t = threading.Thread(target=reader, daemon=True)
t.start()

present = None
events = []
while time.time() - t0 < total:
    try:
        pl = [x.device for x in serial.tools.list_ports.comports()]
    except Exception:
        pl = []
    now = PORT in pl
    if now != present:
        events.append((round(time.time() - t0, 2), "PRESENT" if now else "ABSENT", len(pl), round(time.time() - t0, 2)))
        present = now
    time.sleep(0.15)

stop = True
time.sleep(0.6)
try:
    p2 = None
except Exception:
    pass
open(out, "wb").write(bytes(buf))
sys.stderr.write("bytes=%d\n" % len(buf))
sys.stderr.write("port_presence_events(s,state,nports)=%r\n" % (events,))
sys.stderr.write("reader_open_attempts(s)=%r\n" % (reopens,))
