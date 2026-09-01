# Claw4 板卡身份与固件基线（WB-HW-001 只读采集）

- 任务：WB-HW-001（只读设备接收与端口基线）
- 采集时间：2026-09-01 22:26 ~ 22:31（主机枚举 + 被动串口日志）
- 证据性质：**未连接 JTAG、未发送任何字节、未切换 DTR/RTS、未刷写/擦除/读取 Flash**；仅主机 PnP 枚举 + 115200 被动监听
- 证据分级：`HOST_ENUM_CONFIRMED` / `DEVICE_LOG_CONFIRMED` / `USER_EVIDENCE_REQUIRED` / `UNKNOWN`

## 1. 板卡身份汇总表

| 项 | 值 | 状态 | 证据 |
| --- | --- | --- | --- |
| P4 主控 | ESP32-P4 **eco2**，chip rev **v1.3**（efuse Min v0.0 / Max v1.99） | `DEVICE_LOG_CONFIRMED` | COM3 日志：`ESP-ROM:esp32p4-eco2-20240710`、`efuse_init: Chip rev: v1.3` |
| P4 CPU | 双核 360 MHz（`cpu freq: 360000000 Hz`） | `DEVICE_LOG_CONFIRMED` | COM3 日志：`cpu_start: Multicore app` / `cpu freq: 360000000 Hz` |
| P4 PSRAM | **32 MB @ 200 MHz**（vendor AP 0x0d, gen-4, X16） | `DEVICE_LOG_CONFIRMED` | COM3 日志：`esp_psram: Found 32MB PSRAM device`、`Speed: 200MHz`、`hex_psram: vendor id 0x0d (AP)` |
| P4 Flash | GD 芯片，**qio 模式**（ROM 阶段 DIO, clock div:2 → app 阶段 `flash io: qio`） | `DEVICE_LOG_CONFIRMED`（芯片/模式）；**容量 UNKNOWN** | COM3 日志：`spi_flash: detected chip: gd`、`flash io: qio`；日志未见容量字段 |
| Board 标签 | `METALIO_CLAW_4`（LCD hardware reset done, GPIO 3） | `DEVICE_LOG_CONFIRMED`（固件内板级标签）；**外观丝印/标签 UNKNOWN** | COM3 日志：`I (1451) METALIO_CLAW_4: LCD hardware reset done (GPIO 3)` |
| SKU | 未知 | `USER_EVIDENCE_REQUIRED` | 用户未提供外观/标签/包装照片；**不按商品页或源码默认值填写** |
| 固件 | `xiaozhi` v2.0.51（compile 2026-08-07 11:37:54，ELF SHA256 前缀 `d456cc7c`），ESP-IDF v5.5.4-dirty | `DEVICE_LOG_CONFIRMED` | COM3 日志：`app_init: Project name: xiaozhi` / `App version: 2.0.51` / `Compile time: Aug 7 2026 11:37:54` / `ESP-IDF: v5.5.4-dirty` |
| ESP-Hosted / C5 | SDIO 主机驱动任务已启动（`H_SDIO_DRV: sdio_data_to_rx_buf_task started`）；**C5 芯片状态、固件版本、Wi-Fi 连接 UNKNOWN** | `DEVICE_LOG_CONFIRMED`（SDIO 任务）；C5 详情 `UNKNOWN` | COM3 日志：`H_SDIO_DRV: sdio_data_to_rx_buf_task started` |
| 屏幕驱动 SKU | 未知（NV3051F / FL7707N 均未在日志中实例化确认） | `UNKNOWN` | 日志仅见 `LCD hardware reset done`，未见屏驱初始化完成日志 |
| 触摸 | 未知 | `UNKNOWN` | 日志未捕获 touch 初始化字段 |
| 4G/网络模组 | 复合设备自我描述 `EigenComm Compo`（VID 19D1:0001），含 `log`（COM4）/`at`（COM5）两接口；COM4 log 口有持续二进制输出 | `HOST_ENUM_CONFIRMED`（描述符/拓扑）；**模组型号与功能 UNKNOWN，不推断** | PnP `BusReportedDeviceDesc: EigenComm Compo`；COM4 被动捕获 8,512 B 二进制流 |
| 复位源 | `rst:0x17 (CHIP_USB_UART_RESET)`；boot `0x1f (SPI_FAST_FLASH_BOOT)` 正常从 Flash 启动 | `DEVICE_LOG_CONFIRMED`（设备行为，见 §3 重要观察） | COM3 日志复位头 |

## 2. 端口与拓扑汇总表（HOST_ENUM_CONFIRMED）

| 端口 | PnP 实例 ID | 总线描述 | 父设备 | Container ID | 驱动/状态 | 本轮处理 |
| --- | --- | --- | --- | --- | --- | --- |
| COM3 | `USB\VID_303A&PID_1001&MI_00\9&5D32851&0&0000` | `USB JTAG/serial debug unit` | `USB\VID_303A&PID_1001\80:F1:B2:D2:ED:BB` | `{7DF4FC2F-7294-57DE-86D3-0069CE869ED5}` | usbser / OK | 115200 被动读取 2 次（20s+25s） |
| （P4 JTAG MI_02） | `USB\VID_303A&PID_1001&MI_02\9&5D32851&0&0002` | `USB JTAG/serial debug unit` | 同上 | 同上 | OK | **不连接、不操作** |
| COM4 | `USB\VID_19D1&PID_0001&MI_02\9&17910EBA&0&0002` | `log` | `USB\VID_19D1&PID_0001\000000000001`（EigenComm Compo） | `{06DCB058-254D-5F12-88E3-D7439F84E6A3}` | usbser / OK | 115200 被动读取 2 次（20s+15s），仅二进制流 |
| COM5 | `USB\VID_19D1&PID_0001&MI_00\9&17910EBA&0&0000` | `at` | 同上 | 同上 | usbser / OK | **不打开、不发送 AT**，仅记录描述符 |
| COM6 | `USB\VID_1A86&PID_7522\8&2F2F2084&0&2` | `USB-SERIAL CH340K` | `USB\VID_1A86&PID_8091\7&98c1dd8&0&1` | `{21657416-A5DD-11F1-ADF3-A8E2910A9B99}` | CH341SER_A64 / OK | 115200 被动读取 1 次（15s），近乎静默 |

**USB 拓扑**：Genesys GL3520 hub（`VID_05E3&PID_0610`）→ WCH hub（`VID_1A86&PID_8091`）→ 三个独立设备：
1. Espressif P4 复合设备（COM3 串口 + MI_02 JTAG 接口）
2. `VID_19D1&PID_0001` EigenComm Compo（COM4 log + COM5 at）
3. CH340K（COM6）

> 注意：COM3/JTAG 与 COM4/5/6 是不同 Container ID、不同父设备，属三个独立 USB 设备；COM4 与 COM5 属于同一个 EigenComm 复合设备。

## 3. 重要设备行为观察（需 Codex 评估）

- **打开 COM3（USB JTAG/serial 调试口）会触发设备复位**：两次被动监听（DTR=RTS=false、未发任何字节）均观察到 `rst:0x17 (CHIP_USB_UART_RESET)` 复位头后完整重启（一次复位前 uptime 224,088 ms，另一次 uptime 1,451 ms）。设备复位后从 `boot:0x1f (SPI_FAST_FLASH_BOOT)` 正常启动、app 正常运行。
- 判断：疑似 Windows usbser 驱动在 `Open()` 时对 USB CDC 控制线（DTR）的短暂动作触发 ESP32-P4 USB-Serial/JTAG 芯片复位逻辑；**属设备/驱动交互行为，非我们主动复位**。但任何后续"打开调试口"操作都应预期可能引发一次重启。
- 设备未处于下载模式/恢复模式；未观察到异常循环复位（复位后稳定启动）。
- COM4（EigenComm log）打开后持续输出**专有二进制流**（15s 内 8,512 B，无可读 ASCII 文本）——仅证明模组 log 口上电活跃，不用于推断模组型号或功能。

## 4. 未确认项与所需证据

| 项 | 当前状态 | 所需证据 | 获取方式（仍须遵守只读边界） |
| --- | --- | --- | --- |
| SKU / 板卡丝印 | `USER_EVIDENCE_REQUIRED` | 外观、标签、丝印、包装照片 | 用户拍照提供 |
| 屏幕驱动 SKU（NV3051F/FL7707N） | `UNKNOWN` | 屏驱初始化日志（当前日志未捕获） | 用户配合再次上电 + 被动监听全启动段 |
| 触摸型号/状态 | `UNKNOWN` | touch 初始化/事件日志 | 同上 |
| Flash 容量 | `UNKNOWN` | 启动日志 `flash size` 字段或后续只读验证 | 被动日志（不读 Flash） |
| C5 固件/连接状态 | `UNKNOWN` | ESP-Hosted 连接日志、C5 固件版本 | 被动日志；或后续按授权只读探测 |
| 4G 模组型号 | `UNKNOWN` | 模组侧 log 文本输出（当前为二进制流） | 仅被动监听；**不发送 AT** |
| partition/OTA 实际布局 | `UNKNOWN` | 启动日志分区信息或源码交叉 | 被动日志/源码；不改分区 |
