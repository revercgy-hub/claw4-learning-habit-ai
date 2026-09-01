# Claw4 板卡身份与固件基线（WB-HW-001 + WB-HW-002 采集）

- 任务 1：WB-HW-001（只读设备接收与端口基线），2026-09-01 22:26 ~ 22:31
- 任务 2：WB-HW-002（受控 COM3 启动采集），2026-09-01 23:11 ~ 23:13（用户授权唯一一次 COM3 Open）
- 证据性质：**未连接 JTAG、未发送任何字节、未切换 DTR/RTS、未刷写/擦除/读取 Flash**；仅主机 PnP 枚举 + 115200 被动监听
- 证据分级：`HOST_ENUM_CONFIRMED` / `DEVICE_LOG_CONFIRMED` / `USER_PHOTO_CONFIRMED` / `USER_EVIDENCE_REQUIRED` / `UNKNOWN`
- 物理外观事实：见 [`PHYSICAL_INSPECTION.md`](PHYSICAL_INSPECTION.md)（照片证据，本文件不复制照片内容）

## 1. 板卡身份汇总表

| 项 | 值 | 状态 | 证据 |
| --- | --- | --- | --- |
| P4 主控 | ESP32-P4 **eco2**，chip rev **v1.3**（efuse Min v0.0 / Max v1.99） | `DEVICE_LOG_CONFIRMED` | COM3 日志：`ESP-ROM:esp32p4-eco2-20240710`、`efuse_init: Chip rev: v1.3` |
| P4 CPU | 双核 360 MHz（`cpu freq: 360000000 Hz`） | `DEVICE_LOG_CONFIRMED` | COM3 日志：`cpu_start: Multicore app` / `cpu freq: 360000000 Hz` |
| P4 PSRAM | **32 MB @ 200 MHz**（vendor AP 0x0d, gen-4, X16） | `DEVICE_LOG_CONFIRMED` | COM3 日志：`esp_psram: Found 32MB PSRAM device`、`Speed: 200MHz`、`hex_psram: vendor id 0x0d (AP)` |
| P4 Flash | GD 芯片，**qio 模式**（ROM 阶段 DIO, clock div:2 → app 阶段 `flash io: qio`） | `DEVICE_LOG_CONFIRMED`（芯片/模式）；**容量 UNKNOWN** | COM3 日志：`spi_flash: detected chip: gd`、`flash io: qio`；日志未见容量字段 |
| Board 标签 | `METALIO_CLAW_4`（LCD hardware reset done, GPIO 3） | `DEVICE_LOG_CONFIRMED`（固件内板级标签）；**外观丝印/标签 UNKNOWN** | COM3 日志：`I (1451) METALIO_CLAW_4: LCD hardware reset done (GPIO 3)` |
| SKU | 未知 | `USER_EVIDENCE_REQUIRED` | 用户未提供外观/标签/包装照片；**不按商品页或源码默认值填写** |
| 固件 | **WB-HW-001**: `xiaozhi` v2.0.51（compile 2026-08-07 11:37:54，ELF SHA256 前缀 `d456cc7c`）<br>**WB-HW-002**: `xiaozhi` v2.0.51（compile **2026-08-18 20:07:20**，ELF SHA256 前缀 **`fec753506`**）<br>App version 一致为 v2.0.51，但 compile time 与 ELF 哈希不同（WB-HW-002 运行镜像与 WB-HW-001 不同；该镜像的更新来源、操作方式、时间、授权与启动分区均 `UNKNOWN`）；ESP-IDF 均为 `v5.5.4-dirty` | `DEVICE_LOG_CONFIRMED`（两个时点的日志事实） | COM3 日志：`app_init: Project name: xiaozhi` / `App version: 2.0.51`；WB-HW-002 `Compile time: Aug 18 2026 20:07:20` / `ELF file SHA256: fec753506...` 来自 `com3_controlled.bin` |
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
- 设备未处于下载模式/恢复模式；三次观察中复位后均正常启动（`boot:0x1f` 后 app 初始化至 `DualNetworkBoard: Initialize WiFi board` 行），未观察到异常循环复位。
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

## 5. 物理外观证据（USER-HW-EVIDENCE-001）

- 完整照片事实与逐张解读见 [`PHYSICAL_INSPECTION.md`](PHYSICAL_INSPECTION.md)（`USER_PHOTO_CONFIRMED`，本文件不复制照片内容）。
- 用户照片补充的**可见事实**（不构成推断）：
  - 屏幕已点亮（photo-01 翻页时钟 `22 46 24` + 日期 `2026年09月01日 星期二`）。
  - 背面品牌"ZAO / CLOUD ZAO"（photo-02），左上角一处圆形摄像头模组外观。
  - 底边银色中框外观含 USB-C、左侧 4×5 共 20 个小圆孔、右侧 3 个小圆孔、最右一横向细长条开口（均为纯形态描述，功能不可据图确认）（photo-03）。
  - 侧边银色中框外观含两组内凹多针连接器开口（纯形态描述，左约 6 针 + 右约 10 针 + 中间一小圆孔）（photo-04）。
- **未在照片中可见**：任何 SKU 标签、序列号、条形码、认证标识、底部贴纸。**`USER_EVIDENCE_REQUIRED`**（板卡丝印/外观 SKU 仍待用户进一步提供）。
- **未在照片中可确认**：底边小圆孔阵列/细长开口的功能、连接器引脚定义、顶部圆形凸起的功能、屏幕分辨率与面板型号、触摸、摄像头模组传感器型号——均仍为 `UNKNOWN`（见 `PHYSICAL_INSPECTION.md` §5）。

## 6. 板卡身份与外观 SKU/丝印分离陈述

| 维度 | 来源 | 状态 |
| --- | --- | --- |
| 板卡身份（`metalio-claw-4`） | 固件内 `Board: ... SKU=metalio-claw-4` 行 + `METALIO_CLAW_4: ...` 行 | `DEVICE_LOG_CONFIRMED` |
| 外观品牌（`ZAO / CLOUD ZAO`） | photo-02 背板中央可见 | `USER_PHOTO_CONFIRMED` |
| 外观 SKU/丝印/序列号/认证 | 4 张照片中**均不可见** | `USER_EVIDENCE_REQUIRED` |
| 板卡 revision（rev_min/rev_max 之外的物理丝印） | 照片不可见 | `USER_EVIDENCE_REQUIRED` |

**不进行以下推断**：
- 不由固件内 `SKU=metalio-claw-4` 推断外观一定有同字样标签或丝印；
- 不由背板"ZAO / CLOUD ZAO"品牌标识推断板卡型号；
- 不由商品页或电商描述推断硬件参数；
- 不由源码默认配置覆盖实机日志结论。

## 7. 运行镜像差异说明（WB-HW-001 → WB-HW-002）

| 字段 | WB-HW-001（22:26-22:31） | WB-HW-002（23:11-23:13） | 差异 |
| --- | --- | --- | --- |
| App version | v2.0.51 | v2.0.51 | 一致 |
| Compile time | Aug  7 2026 11:37:54 | **Aug 18 2026 20:07:20** | **11 天后** |
| ELF SHA256 前缀 | `d456cc7c` | **`fec753506`** | **不同** |
| ESP-IDF | v5.5.4-dirty | v5.5.4-dirty | 一致 |
| 复位前 uptime | 224,088 ms | **399,805 ms** | 设备运行更久 |
| 复位前 CPU | 内核0: 3% / 内核1: 11% | **内核0: 2% / 内核1: 47%** | 内核1 占用更高 |
| I18n strings | (未捕获) | 729 strings (2 locales) | 仅 WB-HW-002 可见 |
| UUID（运行时） | (未捕获) | 3a993ef3-b283-4e06-abd5-a892cfbcb239 | 本次日志值；生成方式/持久性/身份语义 `UNKNOWN` |
| 启动段尾部 | `app_main()` | `DualNetworkBoard: Initialize WiFi board` | WB-HW-002 覆盖稍远 |
| 网络选择 | (未捕获到 DualNetworkBoard 行) | **`Initialize WiFi board`** | P4 应用日志执行到该行；Wi-Fi 初始化/连接状态与 4G 状态 `UNKNOWN` |

- **结论**：WB-HW-002 采集时运行镜像的 compile time 与 ELF hash 与 WB-HW-001 不同（App version 编号一致）。该镜像的更新来源、操作方式、时间、授权与启动分区均 `UNKNOWN`；本节只记录日志可见差异，不声称发生过任何固件刷新操作。
- **运行时 UUID**：仅记录 WB-HW-002 本次日志值 `3a993ef3-b283-4e06-abd5-a892cfbcb239`；其生成方式、持久性（是否每次启动变化）与身份语义均 `UNKNOWN`，不得作为设备身份标识。
- **复位与启动签名**：WB-HW-001 两次监听与 WB-HW-002 一次采集中，复位后均从 `boot:0x1f` 正常启动；WB-HW-002 的 90.17 秒采集内未观察到第二次复位/启动签名。该事实不扩展为显示、触摸、网络或整体业务稳定性的结论。
- **本次 WB-HW-002 关键事实**：单次 COM3 `Open()`、写入 0 B、90.17 s 采集、3,143 B 原始字节、唯一一次预期 `CHIP_USB_UART_RESET` 复位、正常 Flash boot 与应用初始化至 `DualNetworkBoard: Initialize WiFi board` 行、采集内未观察到第二次复位/启动签名；详见 [`DEVICE_LOG_REFERENCE.md`](DEVICE_LOG_REFERENCE.md) §8。
