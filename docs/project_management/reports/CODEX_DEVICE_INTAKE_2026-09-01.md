# Codex Claw4 设备接入准入报告

- 日期：2026-09-01
- 执行者：Codex
- 操作性质：主机侧只读枚举与工具链复检
- 结论：`READY_FOR_READONLY_INTAKE`
- 明确未执行：串口打开/写入、AT 命令、复位、Flash 读取/写入/擦除、刷机、分区或 OTA 操作

## 1. USB / COM 实测

Windows 当前返回 `COM3`、`COM4`、`COM5`、`COM6`：

| 端口 | 实测描述 | VID/PID | 当前判定 |
| --- | --- | --- | --- |
| COM3 | `USB JTAG/serial debug unit` | `303A:1001`, MI_00 | `HOST_ENUM_CONFIRMED`：Espressif P4 调试串口高置信候选；待启动日志确认板卡身份 |
| JTAG interface | 同一 Espressif 复合设备 MI_02 | `303A:1001`, MI_02 | `HOST_ENUM_CONFIRMED`：USB JTAG 接口存在；本轮不连接、不操作 |
| COM4 | Bus-reported description `log` | `19D1:0001`, MI_02 | `HOST_ENUM_CONFIRMED`；归属和用途待被动日志确认，不先验认定模块 |
| COM5 | Bus-reported description `at` | `19D1:0001`, MI_00 | `HOST_ENUM_CONFIRMED`；本轮禁止发送 AT 命令 |
| COM6 | `USB-SERIAL CH340K` | `1A86:7522` | `HOST_ENUM_CONFIRMED`；归属待被动日志确认 |

COM3 的父设备实例为 `USB\\VID_303A&PID_1001\\80:F1:B2:D2:ED:BB`，驱动状态正常。COM4/COM5 属于同一个 `VID_19D1/PID_0001` 复合设备。仅凭描述符不得把 COM4/COM5/COM6 写成 P4、C5 或特定蜂窝模块的实机事实。

## 2. 主机基线复检

- ESP-IDF：`v5.5.4`
- RISC-V GCC：`14.2.0`，crosstool-NG `esp-14.2.0_20260121`
- CMake：`3.30.2`
- Ninja：`1.12.1`
- 官方构建产物仍存在：
  - `E:\\b\\xiaozhi.bin`：9,036,192 bytes
  - `E:\\b\\bootloader\\bootloader.bin`：20,416 bytes
  - `E:\\b\\partition_table\\partition-table.bin`：3,072 bytes

短路径 `E:\\i` 是构建镜像而非 Git 仓库，`idf.py --version` 的 Git version warning 不影响版本判定。

## 3. 门禁判断

- `BLK-HW-001` 已解除：设备和 P4 USB JTAG/串口候选已被 Windows 枚举。
- 当前允许进入不改动设备的 USB/COM 映射、被动官方固件日志和可见 SKU/板号证据采集。
- 当前 boot partition、恢复路径、屏幕 SKU、Flash/PSRAM、C5 固件和实际网络仍未确认。
- 用户尚未明确授权 Flash 访问或刷写，因此新增 `BLK-FLASH-AUTH-001`；这不阻塞 `WB-HW-001`，但阻塞任何 `esptool`、`idf.py flash`、Flash 读取/擦除、分区/Bootloader/OTA 操作。

## 4. 调度结论

发布 `WB-HW-001` 为唯一 `READY` 任务。它只负责 B001/B013 的只读前置证据和端口归属，不宣称完成整个 Bring-up Stage 1。若被动日志需要人工重新上电，WorkBuddy 必须停止并请求用户执行物理操作。
