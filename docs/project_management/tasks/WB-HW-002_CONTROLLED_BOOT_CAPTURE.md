# WB-HW-002：受控 COM3 启动采集与物理证据整理

## 调度信息

- 负责人：WorkBuddy
- 复检人：Codex
- 优先级：P0
- 当前状态：`CHANGES_REQUIRED`（初次提交 `63281b2` 已采集；仅允许按最新 Codex 复检报告修订文档，不得新增硬件操作）
- 目标分支：`workbuddy/wb-hw-002-controlled-boot`
- 前置：`WB-HW-001` 已验收；用户照片已归档；用户已明确允许一次 COM3 打开并接受一次设备重启

## 目标

在不访问 Flash、不写串口、不操作 JTAG/AT 的前提下，只打开 COM3 一次，以该次已知的 `CHIP_USB_UART_RESET` 作为启动采集起点，获取约 90 秒的完整只读启动日志；同时把用户照片能直接支持的物理事实整理为可审查文档。

本任务不刷写、不改固件、不验证触摸交互或摄像头成像，也不完成完整 Bring-up Stage 1。

## 开始前必须读取

1. `AGENTS.md`
2. `docs/project_management/TASK_BOARD.md`
3. `docs/project_management/WORKBUDDY_GIT_SYNC.md`
4. 本任务包
5. `docs/project_management/reports/CODEX_REVIEW_WB-HW-001_2026-09-01.md`
6. `docs/project_management/reports/CODEX_USER_HW_EVIDENCE_2026-09-01.md`
7. `docs/BOARD_REVISION.md`
8. `docs/DEVICE_LOG_REFERENCE.md`
9. `项目总规划/AGENTS.md`
10. `项目总规划/CLAW4_BRINGUP_PROMPT.md` 的 B001、B013 与安全边界

## 输入证据

- 上一轮原始证据：`E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-001\`
- 用户原始照片：`E:\workbuddy\学习习惯培育AI-device-evidence\USER-HW-EVIDENCE-001\photos\`
- 本轮原始证据输出：`E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\`

原始照片和原始串口日志不得提交 Git。Git 中只记录绝对路径、文件名、大小、SHA-256、采集时间、参数以及必要的脱敏摘录。

## 允许修改

- `docs/BOARD_REVISION.md`
- `docs/DEVICE_LOG_REFERENCE.md`
- `docs/PHYSICAL_INSPECTION.md`
- `docs/project_management/reports/WB-HW-002_REPORT.md`

不得修改任务看板、任务包、Codex 报告、源码、配置或上述列表之外的任何文件。

## 执行步骤

### A. 开始与只读预检

1. 按同步指令取得干净任务分支，确认 HEAD 与远端任务分支一致。
2. 用 `Get-CimInstance Win32_SerialPort` 和 PnP 属性确认 COM3 仍是 `VID_303A&PID_1001` 的 Espressif `USB JTAG/serial debug unit`。此步骤只查询枚举，不打开端口。
3. 确认 `E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\` 可写，并在仓库外准备原始日志与采集过程记录。
4. 如果 COM3 不存在、身份不一致或正被占用，立即停止；不得尝试 COM4/5/6，也不得打开 COM3 试错。

### B. 唯一一次 COM3 采集

1. 创建一个 `System.IO.Ports.SerialPort` 实例：COM3、115200、8N1、无握手、`DtrEnable=false`、`RtsEnable=false`。
2. 在本任务中只允许调用一次 `Open()`。不得先测试打开、关闭后重开或失败重试。
3. 打开后不得改变 DTR/RTS，不得调用任何 `Write*` API，不得发送字符、控制帧或测试数据。
4. 连续读取约 90 秒，或在完整启动后应用日志已稳定且总采集时长不少于 60 秒时结束；按原始字节保存到仓库外证据目录。
5. 正常关闭一次端口，记录开始/结束时间、串口参数、读取字节数、`Open()` 调用次数和写入字节数（必须为 0）。
6. 若 `Open()` 或采集失败，立即关闭/释放资源并停止；**不得重新打开**。报告 `BLOCKED`，等待 Codex 重新调度和用户重新授权。

### C. 证据整理

1. 计算本轮所有原始证据文件的大小与 SHA-256。
2. 检查日志是否包含 SSID、密码、token、密钥、账号、定位或儿童信息；原始日志仍留仓库外，Git 摘录必须脱敏。
3. 在启动日志实际出现时，记录 ROM/复位源、固件版本、板级标签、PSRAM、Flash 日志字段、分区表、屏幕驱动、触摸、ESP-Hosted/C5、网络初始化和稳定运行证据。未出现的项目写 `UNKNOWN`，不得用源码推断替代实机日志。
4. `docs/PHYSICAL_INSPECTION.md` 只记录照片可见事实，并引用照片文件、大小与哈希。不得把孔洞或连接器猜成麦克风、扬声器、SIM/SD、调试口或其他功能。
5. 更新板卡和日志参考文档，生成任务报告并完成范围校验。

## 绝对禁止

- 第二次 COM3 `Open()`、失败重试、主动复位、手动切换 BOOT/RESET 或 DTR/RTS；
- 打开 COM4、COM5、COM6，发送 AT 命令，连接或操作 JTAG；
- 任意串口 `Write`、`WriteLine`、控制帧、输入注入；
- `esptool`、`idf.py flash`、Flash 读取/写入/擦除、分区读取或修改；
- 修改或构建固件、sdkconfig、partition table、Bootloader、Secure Boot、Flash Encryption、OTA 或 BSP；
- 测试触摸、摄像头、网络业务、4G/GPS 或外部连接器；
- 提交原始日志、原始照片、凭据、构建产物、`vendor/` 或 `toolchains/`。

## 交付物要求

### `docs/PHYSICAL_INSPECTION.md`

至少包括照片索引、仓库外路径/大小/SHA-256、逐张可见事实、不可确认项和证据等级 `USER_PHOTO_CONFIRMED`。

### `docs/BOARD_REVISION.md`

保留已有实机日志事实，补充用户照片证据。固件内 `METALIO_CLAW_4` 标签与外观 SKU/板卡 revision 必须分开陈述。

### `docs/DEVICE_LOG_REFERENCE.md`

新增 WB-HW-002 采集参数、唯一一次 `Open()` 记录、写入 0 B、原始证据索引、脱敏关键行、复位/启动是否稳定和未确认项。

### `WB-HW-002_REPORT.md`

按模板记录任务 ID、分支、提交号、实际文件、逐项自检、命令与结果、原始证据哈希、范围偏差、风险和 Codex 复检重点。

## 验收标准

- [ ] 相对调度基线的 Git diff 只有四个允许文件。
- [ ] COM3 PnP 身份在打开前经只读枚举确认。
- [ ] 整个任务恰好一次 COM3 `Open()`，115200 8N1，DTR/RTS false；串口写入 0 B。
- [ ] 只出现预期的一次 `CHIP_USB_UART_RESET`，随后正常 Flash boot；若证据不符则报告并停止，不得重试。
- [ ] 捕获不少于 60 秒且能判断应用启动后是否稳定；不足时如实标记，不得重开补采。
- [ ] 原始日志和照片位于仓库外，大小与 SHA-256 可复验，Git 中无原始二进制证据。
- [ ] 屏驱、触摸、C5/Hosted、网络、分区、Flash 等结论只依据本次日志；缺失即 `UNKNOWN`。
- [ ] 物理外观只记录可见事实，没有对开孔、接口、SKU 或硬件能力作无证据推断。
- [ ] 未执行 Flash、刷写、JTAG、AT、其他端口、固件修改或范围外操作。
- [ ] `git diff --check origin/main...HEAD` 无错误。

## 停止条件

- COM3 消失、身份变化、被占用或首次 `Open()` 失败；
- 日志出现下载模式、恢复模式、异常循环复位或非预期复位；
- 需要第二次打开、串口写入、人工按键、Flash/JTAG/AT 或其他未授权操作；
- 日志含敏感数据且无法在 Git 交付物中安全脱敏；
- 需要修改四个允许路径之外的文件。

完成后只提交本任务并声明 `REVIEW_READY` 或 `BLOCKED`，不得修改看板、合并 `main`、继续 Stage 1、开始 WB-002 或任何 MVP 开发。
