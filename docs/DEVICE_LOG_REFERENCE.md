# Claw4 设备日志参考（WB-HW-001 只读采集）

- 任务：WB-HW-001（只读设备接收与端口基线）
- 采集时间：2026-09-01 22:26 ~ 22:31
- 采集边界：**未连接 JTAG、未发送任何字节、未切换 DTR/RTS、未刷写/擦除/读取 Flash、未发送 AT**
- 原始证据存放：`E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-001\`（仓库外，Git 不提交原始日志）

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

**COM4（EigenComm log 口）**：仅二进制流，无可读文本；证明 log 口上电活跃。**不用于推断模组型号/功能**。
**COM6（CH340K）**：65 B 近静默，无可读文本；归属未知。

## 5. 重要观察：打开 COM3 触发设备复位

- 两次被动监听（DTR=RTS=false、零写入）均观察到 `rst:0x17 (CHIP_USB_UART_RESET)` 后完整重启。
- 疑似 Windows usbser 驱动 `Open()` 对 USB CDC DTR 控制线的短暂动作触发 ESP32-P4 USB-Serial/JTAG 复位逻辑。
- 属设备/驱动交互行为；后续任何"打开调试口"的操作应预期可能引发一次重启，且不影响正常启动。
- 设备未处于下载/恢复模式，未观察到异常循环复位。

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

## 7. 下一次安全采集建议

1. 用户提供外观/标签照片 → 补齐 SKU（仍只读，不刷写）。
2. 用户手动重新上电（监听就绪后），完整捕获**从 ROM 到 app_main 之后**的启动段，重点补全：屏驱初始化、触摸、Wi-Fi/网络路径、partition 信息。
3. 保持 115200、DTR=RTS=false；预期打开调试口会触发一次复位，以此为采集起点。
4. COM4 如需文本日志，需模组厂商文档确认其 log 协议，**不得发送 AT 探测**。
5. 任何 Flash/分区/OTA 相关验证继续受 `BLK-FLASH-AUTH-001` 阻塞，需用户明确授权。
