# WB-HW-001：Claw4 只读设备接收与端口基线

## 调度信息

- 负责人：WorkBuddy
- 复检人：Codex
- 优先级：P0
- 当前状态：`ACCEPTED`（Codex 于 2026-09-01 验收；历史任务包，仅供追溯）
- 目标分支：`workbuddy/wb-hw-001-readonly-intake`
- 前置：WB-001 已验收；Windows 已枚举 Claw4 相关 USB/COM

## 目标

在完全不修改设备、固件和 Flash 的前提下，建立当前实机的 USB/COM 拓扑、P4 调试端口、可见板卡/SKU信息和未改动官方固件被动日志基线，为 B001/B013 及后续 Stage 1 制定安全入口。

本任务不是刷机任务，也不完成 B002～B009 的功能验收。

## 开始前必须读取

1. `AGENTS.md`
2. `docs/project_management/TASK_BOARD.md`
3. `docs/project_management/WORKBUDDY_GIT_SYNC.md`
4. 本任务包
5. `docs/project_management/reports/CODEX_DEVICE_INTAKE_2026-09-01.md`
6. `项目总规划/AGENTS.md`
7. `项目总规划/CLAW4_BRINGUP_PROMPT.md` 的 B001、B013 和安全边界
8. `docs/PRE_DEVICE_PREP.md`
9. `docs/BUILD.md`
10. `docs/CLAW4_PLATFORM_MAP.md`

## 已知主机证据

- COM3：Espressif `VID_303A/PID_1001`，bus description 为 `USB JTAG/serial debug unit`。
- 同一复合设备存在 USB JTAG MI_02；本任务不得操作 JTAG。
- COM4：`VID_19D1/PID_0001`，bus description `log`。
- COM5：同一复合设备，bus description `at`；禁止发送 AT 命令。
- COM6：CH340K `VID_1A86/PID_7522`。

以上只有主机枚举事实。COM4/5/6 的模块归属、设备 SKU 和功能均不得猜测。

## 允许修改

- `docs/BOARD_REVISION.md`
- `docs/DEVICE_LOG_REFERENCE.md`
- `docs/project_management/reports/WB-HW-001_REPORT.md`

原始日志必须存放在仓库外：

```text
E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-001\
```

Git 中只记录原始文件的绝对路径、大小、SHA-256、采集时间、端口/波特率和必要的脱敏关键行。不得提交原始日志。

## 执行步骤

1. 按同步指令取得干净任务分支，记录开始提交号。
2. 重新执行主机端口枚举，记录端口、VID/PID、MI、bus description、parent、container ID、驱动状态和采集时间。
3. 只检查端口是否存在；不得使用 `esptool` 探测芯片，不得发送任何字节。
4. 对 COM3、COM4、COM6 依次进行短时被动读取：
   - 首选 115200 baud；只读，不写；
   - `DtrEnable=false`、`RtsEnable=false`，禁止切换下载模式或主动复位；
   - COM5 只记录 `at` 描述符，不打开、不发送 AT 命令；
   - 原始输出写入仓库外证据目录，计算 SHA-256。
5. 如果设备当前没有持续日志，不得自行复位。报告 `USER_POWER_CYCLE_REQUIRED`，请求用户在监听已准备好后手动重新上电。
6. 若被动日志出现 P4、固件版本、IDF、board、SKU、revision、CPU、C5/Hosted、memory、partition、display、touch 等字段，逐项摘录并标记 `DEVICE_LOG_CONFIRMED`；缺失项标记 `UNKNOWN`。
7. 用户未提供外观/标签照片时，Board Revision 和 SKU 标记 `USER_EVIDENCE_REQUIRED`，不得根据商品页或源码默认值填写。
8. 生成三份允许的交付文件，执行 Git 范围校验并普通 push。

## 绝对禁止

- `idf.py flash`、`esptool`、`read_flash`、`erase_flash` 或任何 Flash 访问；
- 向 COM3/4/5/6 发送字符、AT 命令、控制帧或测试数据；
- 主动切换 DTR/RTS、BOOT/RESET、下载模式或 JTAG；
- 修改或构建固件、sdkconfig、partition table、Bootloader、OTA、BSP；
- 安装驱动、下载工具、修改系统设置；
- 把原始设备日志、Wi-Fi 信息、token、密钥或儿童数据提交到 Git；
- 把端口描述符推断成硬件功能已通过。

## 交付物要求

### `docs/BOARD_REVISION.md`

至少包含 P4/C5/Board/SKU/Firmware 表格，每项状态必须是：

- `HOST_ENUM_CONFIRMED`
- `DEVICE_LOG_CONFIRMED`
- `USER_EVIDENCE_REQUIRED`
- `UNKNOWN`

### `docs/DEVICE_LOG_REFERENCE.md`

至少包含端口映射、采集方法、原始证据路径/大小/SHA-256、脱敏关键行、未确认项和下一次安全采集建议。

### `WB-HW-001_REPORT.md`

按模板记录任务分支、实际文件、逐项自检、所有命令和结果、是否读取到日志、是否需要用户重新上电、范围偏差及 Codex 复检重点。

## 验收标准

- [ ] Git diff 只有三个允许文件。
- [ ] 主机枚举表可追溯到实例 ID、VID/PID 和采集时间。
- [ ] 没有向任何串口发送数据或切换控制线。
- [ ] 原始日志位于仓库外且有 SHA-256；Git 中无原始日志。
- [ ] 实机日志事实与主机描述符事实明确分级。
- [ ] 未把 COM4/5/6 归属、SKU、屏驱或硬件能力写成已确认，除非有实际日志/用户证据。
- [ ] 未执行任何 Flash、固件、分区、JTAG、AT 或安装操作。
- [ ] `git diff --check origin/main...HEAD` 无错误。

## 停止条件

- 端口被其他程序占用、消失或读取会要求写入/复位；
- 无启动日志且需要用户重新上电；
- 发现当前设备可能处于下载模式、恢复模式或异常循环复位；
- 日志包含凭据、token、Wi-Fi密码或儿童信息，必须先停止并脱敏；
- 需要超出三个允许文件或任何禁止操作。

完成后只提交本任务并声明 `REVIEW_READY` 或 `BLOCKED`，不得开始完整 Stage 1、WB-002 或任何 MVP 开发。
