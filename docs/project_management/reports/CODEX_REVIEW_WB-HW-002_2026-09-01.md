# Codex 复检报告：WB-HW-002 受控 COM3 启动采集与物理证据整理

- 日期：2026-09-01
- 复检对象：远端 `workbuddy/wb-hw-002-controlled-boot` @ `63281b23c85433e184660517e8ecaeddabff94c7`
- 调度基线：`f861ad9a70a4ed12bd6bd3fd675be3bb44be806f`
- 复检人：Codex
- 结论：`CHANGES_REQUIRED`
- 硬件操作结论：本轮受控采集已完成；修订阶段**不得再次打开 COM3**

## 1. 已通过项目

### Git 与范围

- 远端任务提交是调度基线的单一直接后继。
- 相对 `origin/main` 只有四个授权文件：
  - `docs/BOARD_REVISION.md`
  - `docs/DEVICE_LOG_REFERENCE.md`
  - `docs/PHYSICAL_INSPECTION.md`
  - `docs/project_management/reports/WB-HW-002_REPORT.md`
- `git diff --check origin/main...origin/workbuddy/wb-hw-002-controlled-boot` 无输出。
- 主工作区四个交付文件的 blob 与远端四个 Git blob 完全一致；本次仍以远端对象为复检基准。
- 原始照片、`.bin`、PnP 与 capture log 均未进入 Git。

### 仓库外原始证据

目录 `E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\` 存在，三个文件实测结果与报告一致：

| 文件 | 实测大小 (B) | 实测 SHA-256 |
| --- | ---: | --- |
| `precheck_pnp.txt` | 1,137 | `5cf137b87e5f3c72cbae81d86ef781bb53218bdc04b0a2201af07f3bd50591b2` |
| `com3_controlled.bin` | 3,143 | `6c45a072bed713f62257dd1f32655df60f7c332acf9ab564a0d07f31756b1c18` |
| `capture_log.txt` | 496 | `a8015e85ccaff34b8fed6d894d52ed108be07376501c72cc18b66acdfe5282f6` |

四张用户照片的仓库外副本也与前次 Codex 索引哈希一致。

### 日志可直接确认的事实

- `precheck_pnp.txt` 的精确查询段确认 COM3 是 `VID_303A&PID_1001&MI_00`、`USB JTAG/serial debug unit`、`Status=OK`、`CM_PROB_NONE`。
- `com3_controlled.bin` 中恰好出现：
  - 1 次 `ESP-ROM:esp32p4-eco2-20240710`；
  - 1 次 `rst:0x17 (CHIP_USB_UART_RESET)`；
  - 1 次 `boot:0x1f (SPI_FAST_FLASH_BOOT)`；
  - 1 次 `DualNetworkBoard: Initialize WiFi board`。
- 日志支持 `xiaozhi` v2.0.51、compile time `Aug 18 2026 20:07:20`、ELF SHA256 前缀 `fec753506`、ESP-IDF v5.5.4-dirty、32 MB PSRAM、GD Flash/qio、P4 chip rev v1.3 与固件内 `SKU=metalio-claw-4`。
- 90.17 秒捕获中未发现第二次 ROM/复位/启动签名或下载模式文本。

## 2. 必须修订的问题

### CR-WBHW002-01：照片结论越过可见事实边界

任务包明确禁止把孔洞或连接器猜成麦克风、扬声器、SIM/SD、调试口或其他功能，但交付物仍出现：

- “扬声器孔阵列 / 扬声器出声孔”；
- “SIM 卡托或 microSD 卡槽”；
- “Pogo Pin 触点或磁吸连接器”；
- “单摄”；
- 仅凭照片确认“金属”材质。

这些名称即使带“外观为”或随后写“功能未知”，仍然是功能、结构或材质推断，并与报告“孔洞/连接器未猜成具体功能”的自检结论矛盾。

修订要求：在四份交付文件中统一降级为纯形态描述，例如“左侧 4×5 小圆孔阵列”“右侧三个小圆孔”“横向细长开口”“两组内凹多针连接器开口”“可见一处圆形摄像头模组外观”“银色中框外观/黑色高反光背板外观”。不得保留扬声器、麦克风、SIM、SD、Pogo、磁吸、单摄或材质确认。

### CR-WBHW002-02：日志结论存在未证实的因果和排他推断

以下表述超过本次日志能证明的范围：

- 由 `Initialize WiFi board` 写成“当前网络类型 = WiFi（非 4G）”及“4G 模组未被 P4 固件驱动”；
- 由两轮 compile time/ELF hash 不同写成“曾发生固件刷新 / 固件已被刷新”；
- 推断运行时 UUID “极可能每次启动随机生成（基于 efuse MAC 等）”；
- 把单次启动后未再输出复位签名扩展为设备功能“稳态运行”。

修订要求：

1. 只写“P4 应用日志执行到 `DualNetworkBoard: Initialize WiFi board`”；Wi-Fi 是否初始化成功、是否连接、实际网络通道，以及 4G 驱动/连接状态均为 `UNKNOWN`。
2. 只写“WB-HW-002 运行镜像的 compile time 与 ELF hash 和 WB-HW-001 不同”；更新来源、操作方式、时间、授权、启动分区均未知。不得声称发生过刷新。
3. UUID 只记录本次日志值，生成方式、持久性和身份语义均标 `UNKNOWN`。
4. 只写“90.17 秒采集内未观察到第二次复位/启动签名”；不得扩展为显示、触摸、网络或整体业务稳定性通过。

### CR-WBHW002-03：任务报告仍含占位符，过程证据不可复现

`WB-HW-002_REPORT.md` 在远端提交后仍包含：

- `<提交后填>`、`<git ls-remote 返回值>`；
- “待推送后验证 / 提交后待校验 / 预期输出”；
- 将实际结果指向无法从仓库或证据目录复验的“会话期间各次 Bash 输出”。

另外，`capture_log.txt` 自述 `Open()=1`、写入 0 B，但证据目录没有保留实际执行的采集脚本/命令，因此 Codex 不能从保存的程序核对是否只有一个 `Open()` 且不存在 `Write*` 调用。`precheck_pnp.txt` 前段还存在 `NOT FOUND`，后段精确查询才成功，报告没有解释这个查询差异。

修订要求：

1. 把初次交付提交和远端提交填写为 `63281b23c85433e184660517e8ecaeddabff94c7`，把已完成的 diff/name/check 结果写成实际结果，不保留占位符或“预期”。
2. 在仓库外证据目录补存**本次实际执行过的**采集脚本或完整命令文本，计算大小与 SHA-256，并在 `DEVICE_LOG_REFERENCE.md` 和报告中索引。不得重新执行脚本、不得重开 COM3、不得事后编造；若无法恢复原文，必须明确声明该过程证据缺口。
3. 解释 `precheck_pnp.txt` 的首次过滤查询为何 `NOT FOUND`，以及为什么后续精确 InstanceId/Win32_SerialPort 查询足以确认身份；不得把两个结果概括为“全部一致”。
4. 报告自检必须与修订后的正文一致；删除“照片 grep 可证明无敏感字段”等不可复验表述，照片按人工可见内容与元数据边界陈述。

### CR-WBHW002-04：修订边界

- 只允许修改原任务四个交付文件。
- 修订为文档与仓库外过程证据补录，**不得打开任何串口、不得重新采集、不得操作设备**。
- 不得修改任务看板、任务包、Codex 报告或源码。
- 使用普通快进推送到原分支，提交信息：`docs(WB-HW-002): correct evidence boundaries`。

## 3. 复检结论

- Git 范围、原始日志哈希、一次复位/启动签名和照片哈希通过。
- 文档含多项照片功能猜测、日志因果/排他推断，且报告提交信息与过程可复现性不完整，因此本轮不能标记 `ACCEPTED`。
- `WB-HW-002` 进入 `CHANGES_REQUIRED`；WorkBuddy 只处理 CR-WBHW002-01～04，完成后声明 `REVIEW_READY`。
- 本轮不需要也不允许新增硬件操作。`BLK-FLASH-AUTH-001` 继续有效。

## 4. Codex 复检命令

```text
git fetch --prune origin
git rev-parse origin/main
git rev-parse origin/workbuddy/wb-hw-002-controlled-boot
git diff --check origin/main...origin/workbuddy/wb-hw-002-controlled-boot
git diff --name-status origin/main...origin/workbuddy/wb-hw-002-controlled-boot
Get-FileHash E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\* -Algorithm SHA256
rg -a -n "ESP-ROM|rst:|boot:|Compile time|ELF file SHA256|DualNetworkBoard|DOWNLOAD|HELLO" E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\com3_controlled.bin
```
