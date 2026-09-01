# Claw4 设备日志参考（WB-HW-001 + WB-HW-002 采集）

- 任务 1：WB-HW-001（只读设备接收与端口基线），2026-09-01 22:26 ~ 22:31
- 任务 2：WB-HW-002（受控 COM3 启动采集），2026-09-01 23:11 ~ 23:13（用户授权唯一一次 COM3 Open）
- 采集边界：**未连接 JTAG、未发送任何字节（除任务 2 唯一一次 Open 采集外，不变更 DTR/RTS）、未刷写/擦除/读取 Flash、未发送 AT**
- 原始证据存放：
  - WB-HW-001：`E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-001\`（仓库外）
  - WB-HW-002：`E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\`（仓库外）

## 1. 端口映射（HOST_ENUM_CONFIRMED）

| 端口 | 总线描述 | VID/PID | 判定 |
| --- | --- | --- | --- |
| COM3 | `USB JTAG/serial debug unit`（MI_00） | 303A:1001 | Espressif P4 调试串口（`DEVICE_LOG_CONFIRMED` 板卡身份见日志） |
| COM3 同设备 MI_02 | `USB JTAG/serial debug unit` | 303A:1001 | USB JTAG 接口存在；本轮不连接不操作 |
| COM4 | `log`（MI_02） | 19D1:0001 | EigenComm Compo 复合设备 log 口；`HOST_ENUM_CONFIRMED` |
| COM5 | `at`（MI_00） | 19D1:0001 | 同一复合设备 AT 口；本轮不打开不发送 |
| COM6 | `USB-SERIAL CH340K` | 1A86:7522 | `HOST_ENUM_CONFIRMED`；归属未知（不推断） |

> 描述符事实与硬件功能严格区分：COM4/5/6 的模组归属、功能均不推断，除非有实际日志/用户证据。

## 2. 采集方法

| 端口 | 波特率 | DTR/RTS | 时长 | 读取方式 |
| --- | --- | --- | --- | --- |
| COM3 | 115200 8N1 | 均 false | 20s + 25s | .NET `SerialPort.ReadExisting`（UTF-8 文本），只读不写 |
| COM4 | 115200 8N1 | 均 false | 20s（文本）+ 15s（字节级） | `ReadExisting` + `Read(byte[])`，只读不写 |
| COM5 | — | — | — | 未打开（禁止 AT） |
| COM6 | 115200 8N1 | 均 false | 15s | `Read(byte[])`，只读不写 |
| **COM3（WB-HW-002）** | **115200 8N1** | **均 false** | **90.17 s**（**仅 1 次 Open**） | **.NET `SerialPort.Read(byte[])` 字节模式**，保存原始字节；**写入 0 B** |

## 3. 原始证据索引（仓库外目录）

目录：`E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-001\`

| 文件 | 大小 (B) | SHA-256 | 内容 |
| --- | --- | --- | --- |
| `port_enum.txt` | 763 | `a38bd6132af711adc4a3b937583211aced942b8fffddc058d7b2e7b614ac5ee6` | 端口清单与基础 PnP |
| `port_enum_detailed.txt` | 1,252 | `66d1acf3377dcb7bf53d20a326bba201adf8713e6fb6783deb77fb4143d61e65` | 各端口 DeviceID/Service/Status |
| `port_enum_full.txt` | 2,106 | `93b1687653b8e7949e355634c36479443c8f6b60393b44e64871949f5bf4650a` | 父设备/Container ID/问题码 |
| `port_enum_busdesc.txt` | 1,020 | `30f19ed6b5cb143260ca090cfdc967972e9dc76e12b6e8a1652da85ab1b28768` | Bus-reported device description |
| `capture_log.txt` | 568 | `a7f3445386795b528e4a93335fc9f07140b48cd18a34c47c3dbb1b82de540ad7` | 采集过程日志（开关口时间/字符数） |
| `com3_passive.bin` | 2,930 | `82bd9d2c6afbaa8ba624257b520f5212d10e5c560dba10c9bbe4a41fb0605e7c` | COM3 第一次被动日志（完整启动段） |
| `com3_passive_2.bin` | 2,881 | `dbeb53ff230499aa581932c774fae6245a3ef2609d3146a9d680081cf81f996e` | COM3 第二次被动日志（含 METALIO_CLAW_4 行） |
| `com4_passive.bin` | 28,109 | `fdc970fd9b351baeac2e239a15150dbd1a1e94fa530d92fe143fd095bbc59a87` | COM4 文本模式捕获（二进制流 UTF-8 解码） |
| `com4_passive_bytes.bin` | 8,512 | `2c4bc346153d4a04c50f18568ff207e037b81a217b367154921587612b829250` | COM4 字节级捕获（原始二进制流） |
| `com6_passive_bytes.bin` | 65 | `f469de769092ac0ff50d6f7f07897546eb709a3e25522a627136bc31e4422bf5` | COM6 字节级捕获（近乎静默） |

### 3.1 WB-HW-002 原始证据索引

目录：`E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\`

| 文件 | 大小 (B) | SHA-256 | 内容 |
| --- | ---: | --- | --- |
| `precheck_pnp.txt` | 1,137 | `5cf137b87e5f3c72cbae81d86ef781bb53218bdc04b0a2201af07f3bd50591b2` | COM3 PnP 身份只读预检（VID/PID/Container ID/Bus desc/Problem 码） |
| `com3_controlled.bin` | 3,143 | `6c45a072bed713f62257dd1f32655df60f7c332acf9ab564a0d07f31756b1c18` | COM3 唯一一次 `Open()` 字节模式被动采集（90.17 s、零写入） |
| `capture_log.txt` | 496 | `a8015e85ccaff34b8fed6d894d52ed108be07376501c72cc18b66acdfe5282f6` | 采集过程记录（开始/结束时间、`Open()` 次数、写入字节数、字节数、SHA-256） |

## 4. 脱敏关键行（DEVICE_LOG_CONFIRMED）

来源：COM3 被动日志（2 次，内容一致）。仅摘录字段事实，不包含凭据/网络信息。

```text
ESP-ROM:esp32p4-eco2-20240710            ← P4 eco2
rst:0x17 (CHIP_USB_UART_RESET)           ← 复位源（打开调试口时出现，见 §5）
boot:0x1f (SPI_FAST_FLASH_BOOT)          ← 正常 Flash 启动
esp_psram: Found 32MB PSRAM device       ← PSRAM 32MB
esp_psram: Speed: 200MHz                 ← PSRAM 200MHz
cpu_start: cpu freq: 360000000 Hz        ← CPU 360MHz 双核
app_init: Project name:     xiaozhi      ← 应用名
app_init: App version:      2.0.51       ← 固件版本
app_init: Compile time:     Aug  7 2026 11:37:54
app_init: ELF file SHA256:  d456cc7ce... ← 固件 ELF 哈希前缀
app_init: ESP-IDF:          v5.5.4-dirty
efuse_init: Chip rev:         v1.3       ← 芯片版本
spi_flash: detected chip: gd             ← Flash 芯片 GD
spi_flash: flash io: qio                 ← Flash 模式 qio（ROM 阶段 DIO）
H_SDIO_DRV: sdio_data_to_rx_buf_task started  ← ESP-Hosted SDIO 任务启动
I (1451) METALIO_CLAW_4: LCD hardware reset done (GPIO 3)  ← 板级标签 + LCD 复位引脚
I (224088) 系统监控: @@@CPU | 内核0: 3% | 内核1: 11%   ← 设备运行中系统监控（复位前 uptime）
```

### 4.1 WB-HW-002 脱敏关键行（DEVICE_LOG_CONFIRMED，与 WB-HW-001 对比）

来源：`E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\com3_controlled.bin`（一次 Open、零写入、90.17s 采集）。

```text
I (399805) 系统监控: @@@CPU   | 内核0:   2% | 内核1:  47%   ← 复位前 uptime ~400s；内核1 占用高于 WB-HW-001
ESP-ROM:esp32p4-eco2-20240710                                ← 同 WB-HW-001
rst:0x17 (CHIP_USB_UART_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT) ← 第三次确认同源复位
esp_psram: Found 32MB PSRAM device / Speed: 200MHz          ← 同 WB-HW-001
cpu_start: cpu freq: 360000000 Hz                           ← 同 WB-HW-001
app_init: Project name:     xiaozhi                          ← 同 WB-HW-001
app_init: App version:      2.0.51                           ← 同 WB-HW-001（App version 编号未变）
app_init: Compile time:     Aug 18 2026 20:07:20            ← **新固件**（WB-HW-001 为 Aug 7）
app_init: ELF file SHA256:  fec753506...                     ← **新固件**（WB-HW-001 为 d456cc7c）
app_init: ESP-IDF:          v5.5.4-dirty                     ← 同 WB-HW-001
efuse_init: Chip rev:         v1.3                            ← 同 WB-HW-001
spi_flash: detected chip: gd / flash io: qio                 ← 同 WB-HW-001
H_SDIO_DRV: sdio_data_to_rx_buf_task started                 ← 同 WB-HW-001
I (1130) I18n: locale=zh-CN (729 strings, 2 locales)         ← 仅 WB-HW-002 捕获
I (1130) Board: UUID=3a993ef3-b283-4e06-abd5-a892cfbcb239 SKU=metalio-claw-4
I (1131) DualNetworkBoard: Initialize WiFi board            ← **当前网络类型 = WiFi**（非 4G ML307/Nt26）
```

**COM4（EigenComm log 口）**：仅二进制流，无可读文本；证明 log 口上电活跃。**不用于推断模组型号/功能**。
**COM6（CH340K）**：65 B 近静默，无可读文本；归属未知。

## 5. 重要观察：打开 COM3 触发设备复位

- **WB-HW-001 期间**：两次被动监听（DTR=RTS=false、零写入）均观察到 `rst:0x17 (CHIP_USB_UART_RESET)` 后完整重启（一次复位前 uptime 224,088 ms，另一次 1,451 ms）。
- **WB-HW-002 期间**：用户授权唯一一次 `Open()`（DTR/RTS=false、零写入）同样出现 `rst:0x17`，复位前 uptime **399,805 ms**；复位后正常 `boot:0x1f (SPI_FAST_FLASH_BOOT)`，应用初始化到 `DualNetworkBoard: Initialize WiFi board` 行后稳态运行。
- 共 **3 次观察**一致：均为 `CHIP_USB_UART_RESET`，均未进入下载/恢复模式、未观察到异常循环复位。
- 判断：疑似 Windows usbser 驱动 `Open()` 对 USB CDC DTR 控制线的短暂动作触发 ESP32-P4 USB-Serial/JTAG 芯片复位逻辑。
- 属设备/驱动交互行为；后续任何"打开调试口"的操作应预期可能引发一次重启，且不影响正常启动。

## 6. 未确认项

| 项 | 状态 | 说明 |
| --- | --- | --- |
| SKU / 丝印 | `USER_EVIDENCE_REQUIRED` | 需用户照片 |
| 屏幕驱动 SKU | `UNKNOWN` | 日志未捕获屏驱初始化完成 |
| 触摸 | `UNKNOWN` | 日志未捕获 |
| Flash 容量 | `UNKNOWN` | 日志无容量字段 |
| C5 固件/连接 | `UNKNOWN` | 仅见 SDIO 主机任务启动 |
| 4G 模组型号 | `UNKNOWN` | log 口仅二进制流；不发送 AT |
| partition/OTA | `UNKNOWN` | 无日志证据；不改分区 |

## 7. 本次（WB-HW-002）已完成的只读接收

- **物理外观事实归档**：见 [`PHYSICAL_INSPECTION.md`](PHYSICAL_INSPECTION.md)（4 张照片事实、不可确认项独立列示、证据等级 `USER_PHOTO_CONFIRMED`）。
- 用户照片证明**设备屏幕已点亮**（翻页时钟 `22 46 24` + 中文日期 `2026年09月01日 星期二`），外观品牌 `ZAO / CLOUD ZAO`、单摄像头、底边扬声器孔阵列/卡槽、侧边两组内凹 Pogo Pin 连接器均为**仅照片可见事实**；**外观 SKU / 丝印 / 序列号 / 认证标识仍 `USER_EVIDENCE_REQUIRED`**（4 张照片中均不可见）。
- 设备当前**网络类型 = WiFi**（固件日志 `Initialize WiFi board` 行，`DEVICE_LOG_CONFIRMED`）。**4G ML307/Nt26 模组当前未被 P4 固件驱动**（`DualNetworkBoard` 选择了 WiFi 分支）；模组自身（EigenComm Compo，COM4 log / COM5 at）仍上电，但本次日志未发现 P4 与模组之间有数据交互证据。
- **固件已被刷新**（compile time Aug 18、ELF `fec753506`，详见 [`BOARD_REVISION.md`](BOARD_REVISION.md) §7）；设备运行稳定（复位前 uptime ~400 s、无异常循环复位）。
- **采集过程中没有出现 `JFI`、`*<DOWNLOAD>`、`*<HELLO>`、`Waiting for download`、`entering download mode`、`Entering flash mode` 等下载/恢复/异常循环复位迹象**。

## 8. WB-HW-002 受控采集参数与唯一一次 Open 记录

- **任务授权**：用户明确允许打开 COM3 一次（[`CODEX_USER_HW_EVIDENCE_2026-09-01.md`](reports/CODEX_USER_HW_EVIDENCE_2026-09-01.md) §1），接受其可能触发一次 `CHIP_USB_UART_RESET`。
- **串口参数**：`Port=COM3, BaudRate=115200, Parity=None, DataBits=8, StopBits=One, Handshake=None, DtrEnable=false, RtsEnable=false`。
- **打开时间**：2026-09-01 23:11:22.460；**关闭时间**：23:12:52.729；**采集时长 90.17 秒**。
- **`Open()` 调用次数：1**（无先测试打开、关闭后重开或失败重试）。
- **串口写入字节数：0**（脚本未调用任何 `Write*` API）。
- **采集字节**：3,143 B（原始字节，按 `Read(byte[])` 保存至 `com3_controlled.bin`，未做 UTF-8 转换）。
- **采集行为**：与 WB-HW-001 一致——打开 COM3 立即触发 `rst:0x17 (CHIP_USB_UART_RESET)` 复位（本次复位前 uptime 399,805 ms）；复位后 `boot:0x1f (SPI_FAST_FLASH_BOOT)` 正常启动、应用初始化到 `DualNetworkBoard: Initialize WiFi board` 后稳态运行；未进入下载/恢复模式、无异常循环复位。
- **未打开 COM4/5/6**；未发送任何字节；未连接 JTAG；未运行 esptool；未读取/擦除/刷写 Flash；未修改固件、sdkconfig、partition table、Bootloader、OTA 或 BSP。
- **复检命令（Codex 只读验证）**：
  ```text
  Get-FileHash E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\com3_controlled.bin -Algorithm SHA256
  Get-FileHash E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\capture_log.txt      -Algorithm SHA256
  Get-FileHash E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\precheck_pnp.txt     -Algorithm SHA256
  rg -a -n "Aug 18|fec753506|Initialize WiFi|H_SDIO_DRV|METALIO_CLAW_4" <com3_controlled.bin>
  ```
