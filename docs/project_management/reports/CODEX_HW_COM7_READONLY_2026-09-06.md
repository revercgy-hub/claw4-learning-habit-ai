# Claw4 串口只读复核（2026-09-06）

任务：`CODEX-APP-FIRST-001`；执行：Codex；目的：确认用户连接的目标串口与当前固件，未执行刷写/擦除。

## 端口识别

`pnputil /enum-devices /connected /class Ports` 结果：

| 端口 | USB 标识 | 只读结论 |
| --- | --- | --- |
| COM7 | `VID_303A&PID_1001&MI_00` | Espressif USB 串口，目标 Claw4；抓到可解析 ESP32-P4 启动日志 |
| COM4 | `VID_19D1&PID_0001&MI_02` | 读取为二进制调试流，不作为应用日志口 |
| COM5 | `VID_19D1&PID_0001&MI_00` | 115200 只读窗口无文本输出 |
| COM6 | `VID_1A86&PID_7522` | CH340K，非目标 Espressif USB 口 |

系统当前不存在 COM3。COM7 是本批次实际目标口。

## COM7 启动证据

在 115200-8N1 下打开 COM7，DTR/RTS 均关闭，不发送任何字节；捕获到：

- `Project name: xiaozhi`
- `App version: 2.0.51`
- `Compile time: Sep 3 2026 22:06:58`
- `ELF file SHA256: 76b7a7a58...`
- `ESP-IDF: v5.5.4`
- `Found 32MB PSRAM device`
- `Chip rev: v1.3`
- `main_task: Calling app_main()`

打开串口期间设备发生 `CHIP_USB_UART_RESET` 并重新枚举；未写入 Flash、NVS、分区或配置。该复位行为只作为串口打开副作用记录，不据此宣称固件功能通过。

## 与当前候选的关系

当前仓库 C5 BUILD ONLY 候选为 `E:/workbuddy/claw4-idf-cold-c5-20260906/xiaozhi.bin`，SHA-256 `f3a8a58c9ea093f6363a264a72802b7323693982c3bb0efef8a047b5a38bcac3`；COM7 设备仍运行旧镜像，二者不一致。

结论：`HARDWARE_PORT_IDENTIFIED=COM7`、`READONLY_BOOT_LOG=PASS`、`NEW_CANDIDATE_ON_DEVICE=NO`。继续验证新 App-first 链路前，需要用户明确授权一次 **ota_0 application-only app-flash + monitor**；禁止 erase、partition、bootloader、ota_1、C5/eFuse 操作。
