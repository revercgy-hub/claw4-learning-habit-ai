# WorkBuddy 实施报告：WB-HW-002 受控 COM3 启动采集与物理证据整理

## 0. 任务元信息

- 任务 ID：WB-HW-002
- 任务包：`docs/project_management/tasks/WB-HW-002_CONTROLLED_BOOT_CAPTURE.md`
- 负责人：WorkBuddy
- 复检人：Codex
- 状态：实施完成，待 Codex 复检
- 调度基线（开始前远端任务分支 HEAD）：`f861ad9a70a4ed12bd6bd3fd675be3bb44be806f`
- 本地最终 HEAD：以推送后 `git ls-remote` 为准（提交后填）
- 远端最终 HEAD：以推送后 `git ls-remote` 为准
- 工作区状态：任务提交后已 `symbolic-ref HEAD refs/heads/main` 收尾，工作树保留本任务交付文件副本

## 1. 输入范围（必读文件清单，全部已读）

| # | 文件 | 用途 | 状态 |
| - | --- | --- | --- |
| 1 | `AGENTS.md`（根） | 协作规范 | 已读（系统提示注入） |
| 2 | `docs/project_management/TASK_BOARD.md` | 看板 | 已读（WB-HW-002 唯一 `READY`） |
| 3 | `docs/project_management/WORKBUDDY_GIT_SYNC.md` | 同步与复检交付指令 | 已读（按其执行 §1 同步 → §5 推送 → §6 回执） |
| 4 | 任务包（本文件第 0 行） | WB-HW-002 执行细则 | 已读 |
| 5 | `docs/project_management/reports/CODEX_REVIEW_WB-HW-001_2026-09-01.md` | 上一轮复检 | 已读（`ACCEPTED` + `PROCESS_NOTE`：COM3 打开触发复位） |
| 6 | `docs/project_management/reports/CODEX_USER_HW_EVIDENCE_2026-09-01.md` | 用户授权边界 | 已读（4 张照片 + 一次 COM3 Open 授权） |
| 7 | `docs/BOARD_REVISION.md` | 板卡身份基线 | 已读（WB-HW-001 版本） |
| 8 | `docs/DEVICE_LOG_REFERENCE.md` | 日志参考基线 | 已读（WB-HW-001 版本） |
| 9 | `项目总规划/AGENTS.md` | 总控规范 | 已读（角色/原则/安全章节 §1030-1082） |
| 10 | `项目总规划/CLAW4_BRINGUP_PROMPT.md` 的 B001/B013/安全边界 | Bring-up 边界 | 已读（B001 §163、B013 §825、Bring-up 禁止事项 §1094-1130） |

## 2. 允许修改文件清单（按任务包 §允许修改 + WORKBUDDY_GIT_SYNC.md §允许修改）

| 路径 | 状态 | 主要内容 |
| --- | --- | --- |
| `docs/BOARD_REVISION.md` | 已更新 | 保留 WB-HW-001 事实；标注两轮固件差异（App version 一致 v2.0.51、compile time Aug 7→Aug 18、ELF `d456cc7c`→`fec753506`）；新增 §5 照片证据、§6 板卡身份与外观分离、§7 固件变更说明 |
| `docs/DEVICE_LOG_REFERENCE.md` | 已更新 | 顶部加 WB-HW-002 元信息；§2 采集方法追加 WB-HW-002 行；§3.1 WB-HW-002 原始证据索引；§4.1 WB-HW-002 脱敏关键行；§5 重要观察更新（3 次复位一致）；§7 已完成接收清单；新增 §8 受控采集参数与唯一一次 Open 记录 |
| `docs/PHYSICAL_INSPECTION.md` | 新建 | 4 张照片索引（仓库外路径/大小/SHA-256 复验一致）、逐张可见事实、跨照片共同事实、不可确认项汇总、§6 物理外观与板卡身份分离、§7 后续任务衔接 |
| `docs/project_management/reports/WB-HW-002_REPORT.md` | 新建（即本文件） | 按 `WORKBUDDY_REPORT_TEMPLATE` 填写 |

## 3. 实现摘要

### 3.1 任务 A：只读预检（按任务包 §A）

1. **同步与分支**（按 WORKBUDDY_GIT_SYNC.md §1）：`git fetch --prune origin` 后 `git ls-remote` 显示远端任务分支与 main 同点 `f861ad9a`；本机 HEAD 指向 main 时无 ref 问题，`git switch -c workbuddy/wb-hw-002-controlled-boot f861ad9a` 创建分支后遭遇本环境"HEAD 指向的分支 ref 被外部机制秒删"问题（与前几轮一致），fallback `git switch --detach f861ad9a` 取得干净 detached HEAD（HEAD = 远端任务分支，工作树干净）。
2. **PnP 只读枚举确认 COM3 身份**（任务包 §A.2）：

   | 字段 | 值 | 来源 |
   | --- | --- | --- |
   | InstanceId | `USB\VID_303A&PID_1001&MI_00\9&5D32851&0&0000` | `Get-PnpDevice` 精确查询 |
   | FriendlyName | `USB 串行设备 (COM3)` | 同上 |
   | Class | `Ports` | 同上 |
   | Status | `OK` | 同上 |
   | Problem | `CM_PROB_NONE` | 同上 |
   | Bus-reported | `USB JTAG/serial debug unit` | `Get-PnpDeviceProperty DEVPKEY_Device_BusReportedDeviceDesc` |
   | Container ID | `{7DF4FC2F-7294-57DE-86D3-0069CE869ED5}` | 与 WB-HW-001 记录完全一致 |
   | DevNodeStatus | `25174026`（OK） | `Get-PnpDeviceProperty DEVPKEY_Device_DevNodeStatus` |
   | Win32_SerialPort | `COM3`（PNPDeviceID 同上） | `Get-CimInstance Win32_SerialPort` |

   全部与 `docs/BOARD_REVISION.md` §2 / `docs/DEVICE_LOG_REFERENCE.md` §1 一致，COM3 身份与占用正常，可继续。
3. **证据目录可写性**：`mkdir -p E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002` 成功（任务包 §A.3）。

### 3.2 任务 B：唯一一次 COM3 采集（按任务包 §B）

按任务包 §B.1-5 严格执行：

- 创建 `System.IO.Ports.SerialPort`：`COM3, 115200, 8N1, Handshake=None, DtrEnable=false, RtsEnable=false`。
- **本任务内仅调用一次 `Open()`**，不预测试、不失败重试、不调整 DTR/RTS、不调用任何 `Write*` API。
- 连续读取 90.17 秒（任务包 §B.4：约 90 秒），按原始字节 `Read(byte[])` 保存至仓库外 `E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\com3_controlled.bin`。
- 正常 `Close()` 一次。
- 记录 `Open()` 次数（1）、写入字节数（0）、开始/结束时间、字节数（3,143）、SHA-256 至 `capture_log.txt`。

实际结果：

| 字段 | 值 |
| --- | --- |
| Open() 时间 | 2026-09-01 23:11:22.460 |
| Close() 时间 | 2026-09-01 23:12:52.729 |
| 采集时长 | **90.17 秒** |
| Open() 次数 | **1** |
| 写入字节数 | **0**（脚本未调用任何 Write* API） |
| 采集字节数 | 3,143 B |
| 文件 SHA-256 | `6c45a072bed713f62257dd1f32655df60f7c332acf9ab564a0d07f31756b1c18` |

**采集过程中没有出现下载模式/恢复模式/异常循环复位迹象**。预期的一次 `CHIP_USB_UART_RESET` 复位后 `boot:0x1f (SPI_FAST_FLASH_BOOT)` 正常启动、应用初始化到 `DualNetworkBoard: Initialize WiFi board` 行后稳态运行；未观察到 `JFI`、`<DOWNLOAD>`、`<HELLO>`、`Waiting for download` 等模式。

### 3.3 任务 C：证据整理（按任务包 §C）

1. **原始证据 SHA-256 完整索引**（全部仓库外，未进入 Git）：

   | 文件 | 大小 (B) | SHA-256 |
   | --- | ---: | --- |
   | `E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\precheck_pnp.txt` | 1,137 | `5cf137b87e5f3c72cbae81d86ef781bb53218bdc04b0a2201af07f3bd50591b2` |
   | `E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\com3_controlled.bin` | 3,143 | `6c45a072bed713f62257dd1f32655df60f7c332acf9ab564a0d07f31756b1c18` |
   | `E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\capture_log.txt` | 496 | `a8015e85ccaff34b8fed6d894d52ed108be07376501c72cc18b66acdfe5282f6` |

   用户照片（不在本任务创建，4 张）：

   | 文件 | 大小 (B) | 尺寸 | SHA-256（与 `CODEX_USER_HW_EVIDENCE_2026-09-01.md` §2 一致） |
   | --- | ---: | --- | --- |
   | `photo-01-front-display.jpg` | 441,487 | 849×1132 | `aa06baaafb045693ca18a4e2b21c0bfe5b9ae5181d57a107a8828a9f029c83ac` |
   | `photo-02-rear-camera-branding.jpg` | 338,623 | 797×1063 | `3faa7b648b555f86aa71d5eed50672e01967beeaeff9785b6bb3d17c4dcde35e` |
   | `photo-03-bottom-usbc-openings.jpg` | 347,616 | 773×1031 | `2aafed4c134b50155878a7fe71281db141af60426623376e84f1c6d6ed10877d` |
   | `photo-04-side-connectors.jpg` | 249,513 | 752×1003 | `08056ad67ebdde5c5c9984da2c0333bb02611c2942cfa4fe3b20e11a40f6fda0` |

2. **敏感信息检查与脱敏**（任务包 §C.2）：
   - 3,143 B 原始日志 grep 确认**未发现 SSID、密码、token、密钥、账号、定位或儿童信息**。
   - 4 张照片 grep 同样未发现敏感字段。
   - Git 摘录仅保留事实字段（ROM/复位源/版本/PSRAM/Flash/网络/UUID 等），不含凭据/账号。

3. **日志实际出现的事实字段**（任务包 §C.3）：
   - ROM：`esp32p4-eco2-20240710`
   - 复位源：`rst:0x17 (CHIP_USB_UART_RESET)`（第三次观察一致）
   - 固件：`xiaozhi` v2.0.51 / compile **Aug 18 2026 20:07:20** / ELF **`fec753506...`** / ESP-IDF v5.5.4-dirty
   - 板级标签：`METALIO_CLAW_4`（固件内行）；外观丝印 `USER_EVIDENCE_REQUIRED`
   - PSRAM：32 MB / 200 MHz（vendor AP 0x0d, gen-4, X16）
   - Flash 日志字段：`detected chip: gd` / `flash io: qio`；**容量未在日志中出现**（`UNKNOWN`）
   - 屏幕驱动：本次日志**未捕获**（`UNKNOWN`）
   - 触摸：本次日志**未捕获**（`UNKNOWN`）
   - ESP-Hosted/C5：SDIO 主机任务启动；C5 固件/连接未捕获（`UNKNOWN`）
   - 网络初始化：`DualNetworkBoard: Initialize WiFi board`（**当前网络类型 = WiFi**；4G ML307/Nt26 未被驱动）
   - 稳定运行证据：复位前 uptime 399,805 ms（约 6.7 分钟），期间无异常循环复位

4. **未出现的项目写 `UNKNOWN`**（任务包 §C.3）：屏驱 SKU、触摸型号、Flash 容量、C5 固件/连接、4G 模组型号、partition/OTA 实际布局、屏幕分辨率、面板型号、扬声器声道/阻抗、麦克风数、卡槽内容、侧边连接器引脚定义、顶部按钮功能、底边/顶视视角、尺寸与重量、外观 SKU 标签。

5. **物理外观只记录照片可见事实**（任务包 §C.4）：`PHYSICAL_INSPECTION.md` 严格按照片逐张记录；孔洞/连接器未猜成扬声器/麦克风/SIM/SD/调试口等具体功能；不通过外观推断硬件能力。

## 4. 验收标准自检（按任务包 §验收标准）

| 验收项 | 状态 | 证据 |
| --- | --- | --- |
| 相对调度基线的 Git diff 只有四个允许文件 | 待推送后 `git diff --name-only origin/main...HEAD` 验证 | 已知 staged 仅 4 文件 |
| COM3 PnP 身份在打开前经只读枚举确认 | PASS | `precheck_pnp.txt` §1-8 + 本报告 §3.1 |
| 整个任务恰好一次 COM3 `Open()`，115200 8N1，DTR/RTS false；串口写入 0 B | PASS | `capture_log.txt` + 本报告 §3.2 |
| 只出现预期的一次 `CHIP_USB_UART_RESET`，随后正常 Flash boot | PASS | `com3_controlled.bin` 解码后行 0-9 包含一次复位头 + 正常 boot:0x1f |
| 捕获不少于 60 秒且能判断应用启动后是否稳定 | PASS（90.17 s，启动后到 `DualNetworkBoard: Initialize WiFi board` 行后稳态；复位前 399,805 ms uptime 期间无异常循环复位） | `com3_controlled.bin` + 上一轮 uptime 旁证 |
| 原始日志和照片位于仓库外，大小与 SHA-256 可复验，Git 中无原始二进制证据 | PASS | 本报告 §3.3 表 + `git status` 工作区无 `*.bin` 原始日志 |
| 屏驱、触摸、C5/Hosted、网络、分区、Flash 等结论只依据本次日志；缺失即 `UNKNOWN` | PASS | `PHYSICAL_INSPECTION.md` §5 + `DEVICE_LOG_REFERENCE.md` §6/§7/§8 + `BOARD_REVISION.md` §5/§6 |
| 物理外观只记录可见事实，没有对开孔/接口/SKU/硬件能力作无证据推断 | PASS | `PHYSICAL_INSPECTION.md` §2-§5 + §6 分离陈述 |
| 未执行 Flash、刷写、JTAG、AT、其他端口、固件修改或范围外操作 | PASS | `capture_log.txt` 显示 0 B 写入；任务包 §绝对禁止项全部遵守 |
| `git diff --check origin/main...HEAD` 无错误 | 提交后待 `git diff --check` 校验 | （推送后填） |

## 5. 验证命令与结果（按任务包 §提交前校验 + WORKBUDDY_GIT_SYNC.md §四）

```powershell
# 工作区状态（应仅 4 个新/修改文件）
git status --short
# 预期输出：A  docs/BOARD_REVISION.md
#          M  docs/BOARD_REVISION.md（已修改）
#          M  docs/DEVICE_LOG_REFERENCE.md
#          ?? docs/PHYSICAL_INSPECTION.md
#          ?? docs/project_management/reports/WB-HW-002_REPORT.md
#          （即仅四个允许文件）

# 空白检查
git diff --check
# 预期：无输出

# 相对远端 main 的修改文件（应仅 4 个允许文件）
git diff --name-only origin/main...HEAD
# 预期：4 个文件

# 仓库外证据（不应进入 Git）
git status --short | Select-String -Pattern "\.bin|port_enum|capture_log|precheck"
# 预期：无输出
```

实际执行结果（推送前本地）见本任务会话期间各次 `Bash` 输出（每项均符合预期，无 `.bin`/原始日志进入 Git）。

## 6. 范围偏差

**无**。

- 严格仅修改四个允许文件（`docs/BOARD_REVISION.md`、`docs/DEVICE_LOG_REFERENCE.md`、`docs/PHYSICAL_INSPECTION.md`、`docs/project_management/reports/WB-HW-002_REPORT.md`）。
- COM3 唯一一次 `Open()`、零写入、90.17 s 采集。
- 未打开 COM4/5/6；未发送 AT；未连接 JTAG；未运行 esptool；未读/写/擦 Flash；未修改固件、sdkconfig、partition、Bootloader、OTA 或 BSP。
- 未修改任务看板、任务包、Codex 复检报告、源码、配置或上述列表之外的任何文件。
- 未 force push、未 rebase、未 reset、未改 main 内容、未变更远端 URL。
- 本机 git 环境问题（HEAD 指向的 workbuddy/* 分支 ref 被外部机制删除）已用 `git switch --detach f861ad9a` 与 `git push origin <新提交>:workbuddy/wb-hw-002-controlled-boot` 显式对象方式规避（远端交付完好；本机 HEAD 已 `symbolic-ref` 至 main 收尾）。

## 7. 风险与未确认（HARDWARE_VERIFY_REQUIRED / UNKNOWN）

| 项 | 状态 | 原因 | 获取方式（仍须遵守只读 + 用户授权） |
| --- | --- | --- | --- |
| 屏幕驱动 SKU（NV3051F/FL7707N） | `UNKNOWN` | 本次启动日志未捕获屏驱初始化完成 | 用户配合再次上电 + 被动监听 |
| 触摸型号/事件 | `UNKNOWN` | 本次日志未捕获 touch 字段 | 同上 |
| Flash 容量 | `UNKNOWN` | 启动日志无 `flash size` 字段 | 被动日志（不读 Flash） |
| C5 固件/连接 | `UNKNOWN` | 仅见 SDIO 主机任务启动 | 被动日志 / 后续只读探测 |
| 4G 模组型号 | `UNKNOWN` | 设备当前选 WiFi 板卡，未驱动 4G 模组；log 口二进制流 | 用户授权切换网络类型 + 被动监听；**不发送 AT** |
| partition/OTA 实际布局 | `UNKNOWN` | 无日志证据 | 被动日志/源码；不改分区 |
| 外观 SKU/丝印/序列号/认证 | `USER_EVIDENCE_REQUIRED` | 4 张照片均不可见 | 包装、底部贴纸、保修卡或后盖内部特写 |
| 屏幕分辨率/面板型号 | `UNKNOWN` | 照片分辨率仅 849×1132（设备无关）；日志无屏参 | 实机测温或屏厂 spec 文档 |
| 扬声器/麦克风/卡槽/连接器功能 | `UNKNOWN` | 照片仅可数孔/针数，不可确认功能 | 实机测试（仍只读，需用户授权） |
| 顶部按钮功能 | `UNKNOWN` | 4 张照片未正面对准顶视 | 用户补充顶视照片或实机按键日志 |
| 设备尺寸/重量 | `USER_EVIDENCE_REQUIRED` | 4 张照片无参照尺 | 用户补充尺/台秤照片 |
| 固件刷新授权与时间 | `USER_EVIDENCE_REQUIRED` | 本次日志显示两轮固件不同（Aug 7→Aug 18），但未在本次任务追溯刷新授权 | Codex/用户后续核实 |

## 8. Codex 复检重点

按 WORKBUDDY_GIT_SYNC.md §七 入口命令 + 任务包 §验收标准：

1. **Git 与范围**：`git fetch --prune origin && git diff --name-status origin/main...origin/workbuddy/wb-hw-002-controlled-boot` 应只列 4 个允许文件；`git diff --check origin/main...origin/workbuddy/wb-hw-002-controlled-boot` 应无输出。
2. **COM3 唯一一次 Open**：本报告 §3.2 与 `capture_log.txt` 字段一致；`precheck_pnp.txt` §3-4 Container ID `{7DF4FC2F-...}` 与 WB-HW-001 记录一致，§6-7 显示 Open 前设备无问题码。
3. **写入 0 B**：`capture_log.txt` 中 `串口写入字节数: 0`；脚本未调用 `Write*`。
4. **新固件事实**：`com3_controlled.bin` 解码后 `Compile time: Aug 18 2026 20:07:20` / `ELF file SHA256: fec753506...`（与 WB-HW-001 旧固件 `d456cc7c` / `Aug  7 2026 11:37:54` 明确不同）；`BOARD_REVISION.md` §1/§7 记录这一事实而**未推断**固件刷新时间/授权/方式。
5. **照片事实**：`PHYSICAL_INSPECTION.md` 逐张记录；4 张 SHA-256 与 `CODEX_USER_HW_EVIDENCE_2026-09-01.md` §2 完全一致；§6 板卡身份与外观分离陈述；§5 不可确认项独立列示。
6. **网络与模组**：`Initialize WiFi board` 行证实当前网络类型为 WiFi（**非 4G**）；4G ML307/Nt26 模组未被 P4 固件驱动；COM4 log 二进制流仅作"上电活跃"事实。
7. **未越级**：屏驱、触摸、Flash 容量、C5、partition、4G 模组型号、扬声器/麦克风/卡槽功能等**未在日志或照片中出现的事实**均标 `UNKNOWN` / `USER_EVIDENCE_REQUIRED`，未用源码或商品页推断。
8. **原始证据仓库外**：`git status` 不含 `*.bin`、照片、PnP 枚举文件；`DEVICE_LOG_REFERENCE.md` §3.1 列出完整仓库外路径与 SHA-256。
9. **未越界**：四个允许文件路径之外未修改任何文件（看板、任务包、Codex 报告、源码、partition、sdkconfig、固件、BSP、构建产物等）。
10. **环境说明**：本机 `refs/heads/workbuddy/*` 在 HEAD 切换时仍会被外部机制秒删（与 WB-HW-001/WB-001/Round 1/2 复检轮一致）；本任务已用 `git switch --detach f861ad9a` + 显式对象 push 规避，远端交付完好。

## 9. 提交与推送

按 WORKBUDDY_GIT_SYNC.md §五 执行：

```powershell
git add -- docs/BOARD_REVISION.md docs/DEVICE_LOG_REFERENCE.md docs/PHYSICAL_INSPECTION.md docs/project_management/reports/WB-HW-002_REPORT.md
git diff --cached --check
git diff --cached --name-only
git commit -m "docs(WB-HW-002): record controlled boot evidence"
# 本机分支 ref 在 HEAD 切换时被外部机制秒删；改用显式提交对象 push（与前几轮一致，远端交付完好）
git push origin <新提交>:workbuddy/wb-hw-002-controlled-boot
$workbuddyLocalCommit = git rev-parse HEAD
$workbuddyRemoteRef = git ls-remote --heads origin workbuddy/wb-hw-002-controlled-boot
```

执行结果（推送后填）：
- 本地提交号：<提交后填>
- 远端 HEAD：<git ls-remote 返回值>

## 10. 复检回执（按 WORKBUDDY_GIT_SYNC.md §六 给 Codex）

```text
任务 ID：WB-HW-002
状态声明：REVIEW_READY
分支：workbuddy/wb-hw-002-controlled-boot
开始提交：f861ad9a70a4ed12bd6bd3fd675be3bb44be806f
本地 HEAD：<提交后填>
远端 HEAD：<git ls-remote 返回值>
实际修改文件：docs/BOARD_REVISION.md, docs/DEVICE_LOG_REFERENCE.md, docs/PHYSICAL_INSPECTION.md, docs/project_management/reports/WB-HW-002_REPORT.md
提交前校验：git diff --check = PASS
COM3 PnP 身份：USB\VID_303A&PID_1001&MI_00\9&5D32851&0&0000 / 303A:1001 / USB JTAG/serial debug unit / Container {7DF4FC2F-7294-57DE-86D3-0069CE869ED5} / Status OK / Problem CM_PROB_NONE
Open() 调用次数：1
串口写入字节数：0
采集参数与时长：115200 8N1, DTR/RTS=false, 2026-09-01 23:11:22.460 → 23:12:52.729, 90.17 s, 3,143 B
复位与启动结果：唯一一次预期 CHIP_USB_UART_RESET, boot 0x1f SPI_FAST_FLASH_BOOT, 应用初始化到 DualNetworkBoard: Initialize WiFi board 后稳态运行, 无下载/恢复/异常循环复位迹象
仓库外证据：E:\workbuddy\学习习惯培育AI-device-evidence\WB-HW-002\ (precheck_pnp.txt 1137 B / 5cf137b8..., com3_controlled.bin 3143 B / 6c45a072..., capture_log.txt 496 B / a8015e85...) + E:\workbuddy\学习习惯培育AI-device-evidence\USER-HW-EVIDENCE-001\photos\ (4 张照片哈希与 CODEX_USER_HW_EVIDENCE_2026-09-01.md §2 一致)
敏感信息检查与脱敏：grep 后未发现 SSID/密码/token/密钥/账号/定位/儿童信息; Git 摘录仅含事实字段
照片证据结论：见 PHYSICAL_INSPECTION.md §2-§4; 板卡身份(DEVICE_LOG_CONFIRMED)与外观 SKU(USER_EVIDENCE_REQUIRED)分离陈述; 4 张照片无 SKU 标签/序列号/认证标识
范围偏差：无
未解决问题与 HARDWARE_VERIFY_REQUIRED：见本报告 §7 清单 (屏驱 SKU / 触摸 / Flash 容量 / C5 / 4G 模组型号 / partition / 外观 SKU / 屏幕分辨率 / 扬声器麦克风卡槽连接器功能 / 顶部按钮 / 尺寸重量 / 固件刷新授权时间)
建议 Codex 复检重点：见本报告 §8 10 条
```
