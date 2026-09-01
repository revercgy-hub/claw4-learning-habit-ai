# WB-HW-001 WorkBuddy 实施报告

## 1. 元信息

- 任务 ID：WB-HW-001（Claw4 只读设备接收与端口基线）
- 结果：`REVIEW_READY`
- 分支：`workbuddy/wb-hw-001-readonly-intake`
- 开始提交号：`878e18be6fd2380bb2eca384d0786aee61e1e811`（远端 HEAD）
- 执行时间：2026-09-01 22:24 ~ 22:31（采集），22:31+（文档与提交）

## 2. 输入与范围

- 已读取输入：`AGENTS.md`、`docs/project_management/TASK_BOARD.md`、`docs/project_management/WORKBUDDY_GIT_SYNC.md`、本任务包 `WB-HW-001_READONLY_DEVICE_INTAKE.md`、`CODEX_DEVICE_INTAKE_2026-09-01.md`、`项目总规划/AGENTS.md`（历史完整阅读）、`CLAW4_BRINGUP_PROMPT.md`（B001/B013 与安全边界）、`docs/PRE_DEVICE_PREP.md`、`docs/BUILD.md`、`docs/CLAW4_PLATFORM_MAP.md`
- 允许修改路径（严格按任务包）：
  - `docs/BOARD_REVISION.md`
  - `docs/DEVICE_LOG_REFERENCE.md`
  - `docs/project_management/reports/WB-HW-001_REPORT.md`
- 原始日志目录（仓库外）：`E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-001\`
- 实际范围偏差：**无**

## 3. 修改文件

| 文件 | 修改目的 |
| --- | --- |
| `docs/BOARD_REVISION.md` | 板卡身份/固件基线证据表（P4/C5/Board/SKU/Firmware） |
| `docs/DEVICE_LOG_REFERENCE.md` | 端口映射、采集方法、原始证据哈希索引、脱敏关键行 |
| `docs/project_management/reports/WB-HW-001_REPORT.md` | 本报告 |

## 4. 实现摘要

### 4.1 主机端口枚举（22:26-22:27）

- 枚举 4 个 COM：COM3 / COM4 / COM5 / COM6，全部 `Status=OK`、`CM_PROB_NONE`，与 Codex 准入报告一致。
- 拓扑（PnP 实测）：Genesys GL3520 hub（05E3:0610）→ WCH hub（1A86:8091）→ 三个独立 USB 设备：
  - Espressif P4 复合设备（COM3 = `USB\VID_303A&PID_1001&MI_00` + MI_02 JTAG 接口，Container `{7DF4FC2F-...}`）
  - `VID_19D1&PID_0001` 复合设备，**BusReportedDeviceDesc=`EigenComm Compo`**（COM4 `log` MI_02 + COM5 `at` MI_00，Container `{06DCB058-...}`）
  - CH340K（COM6，`1A86:7522`，Container `{21657416-...}`）
- 完整枚举证据：`port_enum*.txt`（4 个文件）+ 采集过程 `capture_log.txt`，见 `DEVICE_LOG_REFERENCE.md` §3 哈希索引。

### 4.2 被动串口日志（22:28-22:31）

- **COM3**（P4 调试口）：115200、DTR=RTS=false、零写入，被动监听 2 次（20s+25s），捕获完整启动段（`com3_passive.bin` / `com3_passive_2.bin`）。`DEVICE_LOG_CONFIRMED` 事实：
  - P4 **eco2**，chip rev **v1.3**；CPU 双核 **360 MHz**
  - PSRAM **32 MB @ 200 MHz**（vendor AP 0x0d, gen-4, X16）
  - Flash **GD 芯片、qio 模式**（ROM 阶段 DIO）；**容量日志未见 → UNKNOWN**
  - 固件 `xiaozhi` **v2.0.51**（2026-08-07 编译，ELF SHA256 前缀 `d456cc7c`），ESP-IDF **v5.5.4-dirty**
  - 板级标签 `METALIO_CLAW_4`（`LCD hardware reset done (GPIO 3)`）
  - `H_SDIO_DRV: sdio_data_to_rx_buf_task started`（ESP-Hosted SDIO 任务启动）
- **COM4**（EigenComm `log` 口）：被动监听 2 次，输出**专有二进制流**（字节级 15s = 8,512 B），无可读 ASCII 文本；仅证明 log 口上电活跃，不推断模组型号。
- **COM5**（`at` 口）：**未打开、未发送任何命令**，仅记录描述符。
- **COM6**（CH340K）：被动监听 15s，65 B 近静默；归属未知（`UNKNOWN`）。

### 4.3 重要设备行为观察

- **打开 COM3 会触发设备复位**：两次被动监听（DTR=RTS=false、零写入）均观察到 `rst:0x17 (CHIP_USB_UART_RESET)` 后完整重启（复位前 uptime 224,088 ms / 1,451 ms）。判断为 usbser 驱动 `Open()` 对 USB CDC DTR 的短暂动作触发 ESP32-P4 USB-Serial/JTAG 复位逻辑，**非我方主动复位**；设备复位后从 `SPI_FAST_FLASH_BOOT` 正常启动，未进入下载/恢复模式，无异常循环复位。
- 该观察已写入 `BOARD_REVISION.md` §3 与 `DEVICE_LOG_REFERENCE.md` §5，需 Codex 评估其对后续调试流程的影响。

### 4.4 未确认项

SKU/丝印（`USER_EVIDENCE_REQUIRED`，需用户照片）、屏幕驱动 SKU、触摸、Flash 容量、C5 固件/连接、4G 模组型号、partition/OTA 布局（均 `UNKNOWN`，详见 `BOARD_REVISION.md` §4）。

## 5. 验收标准自检

| 验收项 | PASS / FAIL / BLOCKED | 证据 |
| --- | --- | --- |
| Git diff 只有三个允许文件 | PASS | `git diff --name-status origin/main...HEAD` 仅 3 个文件（提交后核验） |
| 主机枚举表可追溯到实例 ID、VID/PID 和采集时间 | PASS | `port_enum_detailed/full/busdesc.txt`（22:26-22:27 采集） |
| 没有向任何串口发送数据或切换控制线 | PASS | 全部 `DtrEnable=false; RtsEnable=false`，仅 `Read*`，无 `Write*`；`capture_log.txt` 无写入记录 |
| 原始日志位于仓库外且有 SHA-256；Git 中无原始日志 | PASS | 原始文件在 `E:\...\device-evidence\WB-HW-001\`，10 个文件哈希已列（`DEVICE_LOG_REFERENCE.md` §3）；Git 未跟踪 |
| 实机日志事实与主机描述符事实明确分级 | PASS | `HOST_ENUM_CONFIRMED` / `DEVICE_LOG_CONFIRMED` / `USER_EVIDENCE_REQUIRED` / `UNKNOWN` 四档全程使用 |
| 未把 COM4/5/6 归属、SKU、屏驱或硬件能力写成已确认 | PASS | COM4/5/6 仅 `HOST_ENUM_CONFIRMED`，归属 `UNKNOWN`；SKU `USER_EVIDENCE_REQUIRED`；屏驱 `UNKNOWN` |
| 未执行任何 Flash、固件、分区、JTAG、AT 或安装操作 | PASS | 全程只读：PnP 查询 + .NET SerialPort 被动读取；无 esptool/idf.py/写串口/JTAG 连接/AT/安装 |
| `git diff --check origin/main...HEAD` 无错误 | PASS | 提交后核验（见 §6） |

## 6. 验证命令与结果

```text
# 端口枚举（只读，PowerShell CIM/PnP）
Get-CimInstance Win32_PnPEntity | Where-Object { ... VID_303A|VID_19D1|VID_1A86 ... }
=> COM3/COM4/COM5/COM6 全部 Status=OK, CM_PROB_NONE（证据文件 port_enum*.txt）
Get-PnpDeviceProperty -KeyName DEVPKEY_Device_BusReportedDeviceDesc
=> COM3/MI_02: "USB JTAG/serial debug unit"; COM4: "log"; COM5: "at";
   19D1 复合: "EigenComm Compo"; COM6: "USB Serial"

# 被动串口读取（只读；DtrEnable=false; RtsEnable=false; 无 Write）
.NET SerialPort ReadExisting / Read(byte[])
=> COM3: 2,911 chars + 2,878 chars（完整启动日志）
=> COM4: 8,512 B / 15s 二进制流（无可读文本）
=> COM6: 65 B / 15s（近静默）
=> COM5: 未打开

# 证据哈希（仓库外）
sha256sum com3_passive.bin ...  => 见 DEVICE_LOG_REFERENCE.md §3（10 个文件哈希）

# Git 校验（提交前）
git status --short            => ?? docs/BOARD_REVISION.md ?? docs/DEVICE_LOG_REFERENCE.md ?? WB-HW-001_REPORT.md
git diff --check              => PASS
git diff --name-only          => 仅 3 个允许文件
```

## 7. 风险与未决项

- `HARDWARE_VERIFY_REQUIRED` / 待确认：
  - SKU、板卡丝印：`USER_EVIDENCE_REQUIRED`（需用户照片）
  - 屏幕驱动 SKU、触摸、Flash 容量、C5 固件/连接、4G 模组型号、partition/OTA 布局：`UNKNOWN`
- 其他风险：
  - **打开 COM3 调试口触发设备复位（CHIP_USB_UART_RESET）**——需 Codex 评估对后续 B001/B013 采集流程的影响；建议后续采集以"打开端口即重启"为起点，完整捕获启动段。
  - COM4 log 口为专有二进制流，文本日志需模组厂商协议文档；不得以 AT 探测替代。
  - Flash/分区/OTA 相关验证继续受 `BLK-FLASH-AUTH-001` 阻塞（需用户明确授权）。
- 阻塞项：无（本轮只读采集完成，无需用户重新上电——设备在打开 COM3 时已自然重启并输出完整启动日志；但仍建议用户后续手动重新上电以补全屏驱/触摸/网络等后续启动段）。

## 8. 给 Codex 的复检重点

1. 三份交付文件是否全部落在允许范围内（`git diff --name-status origin/main...HEAD`）。
2. `BOARD_REVISION.md` 四档分级是否严格（重点：COM4/5/6 未越级、SKU 未填默认值、屏驱未锁定）。
3. **打开 COM3 触发 `CHIP_USB_UART_RESET` 的观察**是否影响 B001/B013 采集方法，是否需要更新 Bring-up 流程。
4. `EigenComm Compo`（19D1:0001）描述符作为 `HOST_ENUM_CONFIRMED` 的事实记录是否恰当（模组型号保持 `UNKNOWN`）。
5. 原始证据哈希索引是否满足任务包"仓库外 + SHA-256"要求，Git 内确无原始日志。

## 9. 下一步

- 建议 Codex 复检后：用户提供 SKU/标签照片（仍只读），并协调一次"监听就绪后手动重新上电"以补全屏驱/触摸/网络启动段。
- 不自行开始 Stage 1 功能验收、WB-002 或任何 MVP 开发。
