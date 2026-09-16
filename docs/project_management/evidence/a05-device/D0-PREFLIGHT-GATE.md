# A05-DEVICE D0 — DEVICE PREFLIGHT GATE（只读阶段结论）

device_id        = p4-80f1b2d2ed14
port             = COM7  (USB\VID_303A&PID_1001, 内建 USB-Serial/JTAG)
timestamp_window = 2026-09-16 12:30:16 ~ 12:35:18 +08:00
NO WRITE FLASH   = TRUE（D0 全程仅 read_flash / flash_id / image_info，无任何写操作）

| Gate | 结果 | 证据 |
|---|---|---|
| DEVICE_IDENTITY          | **PASS** | ESP32-P4 rev v1.3, 40MHz, USB-Serial/JTAG, MAC 80:f1:b2:d2:ed:14 — `_d0a-flash-id.txt` / `device-info.txt` |
| FLASH_SIZE               | **PASS** | 32MB (GigaDevice c8:4019)；冻结布局末段结束 0x01FA6000，尾部余 368,640 B — `flash-id.txt` |
| BACKUP                   | **PASS** | 8 个区域全部 rc=0、长度匹配、逐个 SHA256 已记录 — `preflash-backup-manifest.txt` / `_d0c-backup-log.txt` |
| PARTITION_TABLE          | **PASS** | 实机 13 项与冻结 `32m_dual.csv` 逐项一致；且候选 `partition-table.bin` 与实机 @0x9000 段**逐字节相同** — `partition-decoded.txt` |
| OTA_SELECTION            | **PASS** | otadata[0] ota_seq=1, state=ESP_OTA_IMG_VALID ⇒ boot_index=(1-1)%2=0 ⇒ **ota_0**；且 boot selector 未固定选 ota_1 — `otadata-decoded.txt` |
| RECOVERY_PATH            | **CONFIRMED** | ROM download mode 本任务多次实证可达；备用 UART0/CH340(COM6) — `recovery-path.txt` |
| CANDIDATE HASH           | **MATCH** | xiaozhi.bin 9,271,760 B / sha256 c035e1c0…8d36 == 冻结值 — `candidate-sha256.txt` |

## 附加确认（本轮新增，超出最小要求）
- 候选构建 `flasher_args.json` 的布局偏移（bootloader 0x2000 / parttable **0x9000** / app **0x200000** /
  otadata 0x10E000 / model 0x111000 / resources 0xF00000 / factory_test 0x1300000）**全部与实机吻合**；
  候选 sdkconfig `CONFIG_PARTITION_TABLE_OFFSET=0x9000`，与实机一致 ⇒ 不存在"分区表偏移错位"风险。
- 候选 sdkconfig `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`、`CONFIG_ESP_CONSOLE_UART_NUM=-1`
  ⇒ 应用控制台在 COM7，D2 monitor 走该口。
- 候选 sdkconfig `CONFIG_BOOTLOADER_LOG_LEVEL_NONE=y` ⇒ **bootloader 阶段无串口输出**，
  D2 无法从 bootloader 日志直接读 "Loaded app from partition at offset 0x…"，
  须由应用自身启动横幅/行为反推，或由 D0 的静态推理承担该结论。
- 实机 ota_0 现有 app 入口点 4ff0041a == 候选 xiaozhi.bin 入口点 ⇒ 同源谱系。

## 结论
D0 PREFLIGHT = **PASS**  → 允许进入 D1（且满足"最小写入"前提：bootloader / partition table /
otadata 三项均已被证明与候选兼容且**无需改写**）。

D1 写入集合（最小必要）：
| offset | 内容 | 是否写入 |
|---|---|---|
| 0x200000 | xiaozhi.bin（冻结候选） | **写入** |
| 0x2000   | bootloader | 不写（实机兼容，且候选侧标注 EVIDENCE_ONLY） |
| 0x9000   | partition-table | 不写（逐字节相同） |
| 0x10E000 | otadata | 不写（改写会破坏当前 boot selector 证据） |
| 0xA000 / 0x3C000 / 0x111000 / 0xF00000 / 0x1300000 | nvsfactory / nvs / model / resources / factory_test | 不写（保留原系统与校准/资源数据） |
| 0xB00000 | ota_1 | 不写（保留为回滚位） |
