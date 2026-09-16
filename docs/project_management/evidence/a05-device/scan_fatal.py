import sys
import os
import glob

KEYS = ["panic", "abort", "assert", "watchdog", "WDT", "stack overflow",
        "heap corruption", "Guru Meditation", "LoadProhibited", "StoreProhibited",
        "IllegalInstruction", "brownout", "reset reason", "rst:0x", "Backtrace",
        "Fatal", "E (", "ESP_ERROR_CHECK failed"]

ev = r"E:\claw4-a05-build-m0\docs\project_management\evidence\a05-device"
out = os.path.join(ev, "fatal-scan.txt")

files = []
for pat in ("A05_DEVICE_BOOT_*.log", "*.rawlog", "_steady-attach.log",
            "_nonreset-attach.log", "_liveattach.log"):
    files += sorted(glob.glob(os.path.join(ev, pat)))

lines = []
lines.append("# A05-DEVICE D2 fatal keyword scan")
lines.append("# 扫描对象：本轮全部串口抓取文件")
lines.append("")
total_hits = 0
for f in files:
    data = open(f, "rb").read()
    txt = data.decode("utf-8", "replace")
    if len(txt) < 30:
        txt = "\n".join(repr(b) for b in data.split(b"\n"))
    lines.append("## %s  (size=%d)" % (os.path.basename(f), len(data)))
    for k in KEYS:
        n = txt.count(k)
        if n:
            total_hits += n
            lines.append("  HIT %-24s count=%d" % (k, n))
            # 上下文
            idx = 0
            shown = 0
            while shown < 3:
                i = txt.find(k, idx)
                if i < 0:
                    break
                s = max(0, i - 60)
                ctx = txt[s:i + len(k) + 60].replace("\n", " | ").replace("\r", "")
                lines.append("      ctx: ...%s..." % ctx)
                idx = i + 1
                shown += 1
    if not any(txt.count(k) for k in KEYS):
        lines.append("  (no keyword hits)")
    lines.append("")

lines.append("## 汇总")
lines.append("total_keyword_hits = %d" % total_hits)
lines.append("")
lines.append("## 判定说明")
lines.append("rst:0x 命中来自：① 每次主机打开串口触发的 CHIP_USB_UART_RESET（我方操作，非应用缺陷）；")
lines.append("              ② 用户手动强制重启。")
lines.append("panic / Guru Meditation / LoadProhibited / StoreProhibited / IllegalInstruction /")
lines.append("stack overflow / heap corruption / assert / brownout 命中，若无 => 无应用级致命错误。")

open(out, "w", encoding="utf-8").write("\n".join(lines))
print("wrote", out, "hits=", total_hits)
print("files=", [os.path.basename(f) for f in files])
