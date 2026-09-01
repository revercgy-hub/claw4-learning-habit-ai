# Codex 复检报告：WB-HW-001 只读设备接收与端口基线

- 日期：2026-09-01
- 复检对象：远端 `workbuddy/wb-hw-001-readonly-intake` @ `9867a56121df4cdcdc85f66f4617f3ab8e0b7f81`
- 前序调度提交：`878e18be6fd2380bb2eca384d0786aee61e1e811`
- 复检人：Codex
- 结论：`ACCEPTED`
- G2 真机 Stage 1：`INCOMPLETE`

## 1. Git 与范围

- 任务提交是调度提交 `878e18b` 的普通直接后继。
- 相对 `origin/main` 只新增：
  - `docs/BOARD_REVISION.md`
  - `docs/DEVICE_LOG_REFERENCE.md`
  - `docs/project_management/reports/WB-HW-001_REPORT.md`
- `git diff --check origin/main...origin/workbuddy/wb-hw-001-readonly-intake` 无输出。
- 主工作区三个未跟踪交付文件的 blob ID 与远端三个 blob 完全一致；复检以远端 Git 对象为准。

## 2. 仓库外原始证据

证据目录 `E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-001\` 存在。10 个文件的实测大小和 SHA-256 与 `DEVICE_LOG_REFERENCE.md` 全部一致：

- 4 份 PnP 枚举文件；
- 1 份采集过程记录；
- 2 份 COM3 启动日志；
- 2 份 COM4 捕获；
- 1 份 COM6 捕获。

原始日志未进入 Git。COM3 脱敏摘录中未发现 Wi-Fi 密码、token、密钥或儿童信息。

## 3. 已确认的实机事实

COM3 原始日志直接支持：

- ESP32-P4 eco2，efuse chip rev v1.3；
- 双核应用，CPU 360 MHz；
- 32 MB PSRAM，200 MHz，AP vendor、generation 4、X16；
- Flash vendor `gd`，应用阶段 `qio`；容量仍未知；
- `xiaozhi` v2.0.51，compile time 2026-08-07 11:37:54；
- ESP-IDF v5.5.4-dirty；
- ESP-Hosted SDIO host task 已启动；
- 固件板级标签 `METALIO_CLAW_4`；外观 SKU/丝印仍未确认。

PnP 原始证据支持 COM3/JTAG、COM4 `log`、COM5 `at`、COM6 CH340K 的拓扑。交付物没有把 COM4/5/6 的模块归属或功能越级标为实机确认。

## 4. 安全边界复核

- 未运行 `esptool`、`idf.py flash`、Flash 读取/写入/擦除、JTAG、AT、驱动安装或固件修改。
- COM5 未打开；所有采集程序未调用串口写入。
- 原始日志留在仓库外并有哈希索引。

### PROCESS_NOTE：COM3 打开会触发复位

第一次以 DTR/RTS false 打开 COM3 后，日志出现 `rst:0x17 (CHIP_USB_UART_RESET)`。第二次打开再次出现同一复位源。

这不是 Flash/串口写入违规，也未导致下载模式、恢复模式或循环重启，因此本任务不要求返工。但第一次观察后再次打开已经是可预期的设备重启副作用。后续流程必须把“打开 COM3”按受控重启操作管理：任务包需明确授权，执行前需用户确认当前设备可以重启，不得称为纯被动、零影响监听。

## 5. 验收与下一门禁

- WB-HW-001 标记 `ACCEPTED`。
- `BLK-HW-001` 保持已解除。
- G2 仍未通过：屏幕 SKU、触摸、Flash 容量、C5 固件/连接、实际分区/OTA 和完整网络路径尚未确认。
- 下一硬件任务在以下用户输入前保持 `BACKLOG`：
  1. 设备外观、包装、SKU 标签和板卡丝印照片；
  2. 是否允许下一轮打开 COM3，接受其触发一次设备重启，以采集完整启动段。
- Flash 访问和刷写仍受 `BLK-FLASH-AUTH-001` 阻塞。

## 6. 复检命令

```text
git fetch --prune origin
git rev-parse origin/workbuddy/wb-hw-001-readonly-intake
git diff --check origin/main...origin/workbuddy/wb-hw-001-readonly-intake
git diff --name-status origin/main...origin/workbuddy/wb-hw-001-readonly-intake
Get-ChildItem E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-001 -File
Get-FileHash <每个证据文件> -Algorithm SHA256
rg -a -n "ESP-ROM|rst:|boot:|Found 32MB|cpu freq|App version|ESP-IDF|Chip rev|flash io|H_SDIO_DRV|METALIO_CLAW_4" <COM3 原始日志>
```
