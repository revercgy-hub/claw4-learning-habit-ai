# WB-STREAM-002：领域状态机与离线 Outbox 连续开发流

## 1. 调度信息

- 负责人：WorkBuddy
- 异步复检与普通缺陷修复：Codex
- 状态：`READY`
- 分支：`workbuddy/domain-offline-stream`
- 固定起始基线：以本任务发布提交为准，见 `WORKBUDDY_GIT_SYNC.md`
- 顺序：CP0 → CP1 → CP2；每个 checkpoint 验证、独立提交并普通 push 后直接继续
- 报告：`docs/project_management/reports/WB-STREAM-002_REPORT.md`

本流只做可在 Windows 主机运行的 C++17 业务核心和测试，不接入真机或官方固件。

## 2. 开始前必须读取

1. `AGENTS.md`
2. `项目总规划/AGENTS.md`
3. `docs/project_management/CONTINUOUS_DEVELOPMENT.md`
4. `docs/project_management/TASK_BOARD.md`
5. `docs/project_management/WORKBUDDY_GIT_SYNC.md`
6. `docs/project_management/reports/CODEX_REVIEW_WB-STREAM-001_2026-09-02.md`
7. `docs/ARCHITECTURE.md`
8. 本任务包

## 3. 全流禁止事项

- 不修改 `vendor/MetalioClaw4/**`、官方 sdkconfig、partition CSV、Bootloader、OTA 或 BSP。
- 不打开串口，不读取/擦除/刷写 Flash，不构建设备发布固件，不运行任何设备烧录命令。
- 不加入 LVGL、Wi-Fi、GPIO、ESP-IDF、FreeRTOS、NVS 实现或任何硬件依赖。
- 不接入真实数据库、NAS、AI Provider、账号、密钥、儿童数据、付费或公网业务服务。
- 不实现 Camera、Voice、情绪/人脸识别、本地大模型、4G/GPS 或非 MVP 功能。
- 不修改任务看板、任务包、Codex 报告、同步指令或 `AGENTS.md`。
- 不 force push、rebase、reset、删除/改写分支或修改远端 URL。

## 4. CP0：建立本机 C++17“编译、链接、运行”门槛

### 目标

建立可复现的 Windows 主机测试入口。P4 交叉编译器只能继续用于接口语法检查，不能替代本机可执行单元测试。

### 允许修改

- `tools/dev/verify-host-cpp-tests.ps1`
- `firmware/tests/host/**`
- `.gitignore`（仅在现有规则确实未覆盖本地工具/输出时最小修改）
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`

### 工具链规则

1. 先搜索已有 `clang++.exe`、`g++.exe` 或 Visual Studio `cl.exe`，记录绝对路径和版本。
2. 若不存在，允许使用 `winget` 安装官方 `LLVM.LLVM` 的当前稳定版到用户范围；不得请求管理员权限，不得关闭证书校验。记录包 ID、版本、来源和实际路径。
3. 安装包、编译器、缓存和二进制只能留在用户工具目录或已忽略的 `toolchains/`、`out/`，不得提交。
4. 若安装需要提权、来源/哈希不可验证或本机程序仍不能运行，CP0 标记 `BLOCKED` 并停止整个流；不得用交叉编译“假装”运行测试。

### 脚本要求

- 支持 `-CompilerPath`；未传入时可探测 PATH/标准安装路径，但不能写死当前用户路径。
- 以 C++17、警告视为错误编译并链接 `firmware/tests/host/smoke_test.cpp`，输出到 `out/host-tests/`，随后实际运行程序并透传退出码。
- 路径含空格和中文时必须正常工作；失败应显示编译器、命令阶段和退出码，不吞掉原始诊断。
- 继续调用现有 `verify-interface-contracts.ps1`，证明接口交叉编译未回退。

### 验收与提交

- 主机 smoke 程序真实执行并返回 0；报告必须区分 compile/link/run。
- 接口契约脚本 PASS；`git diff --check` PASS。
- 提交：`test(WB-STREAM-002): add native C++ test gate`
- push 后记录 `CP0_CHECKPOINT=<hash>`，立即进入 CP1。

## 5. CP1：纯领域 Reducer 与状态机

### 目标

实现无硬件依赖、无内部 I/O 的确定性领域 reducer：输入不可变 `DomainState`、intent 和显式上下文，输出“下一状态 + 事件”，不直接发布 UI、不写磁盘、不发网络。提交由 CP2 outbox 决定，持久化失败时旧状态仍是唯一已提交状态。

### 允许修改

- `firmware/main/learning_domain/**`
- `firmware/tests/unit/domain/**`
- `tools/dev/verify-host-cpp-tests.ps1`
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`

### 必须实现

1. Task 转换：Ready → InProgress → Paused ↔ InProgress → Completed；Skip 只在允许状态发生。
2. StudySession：Created/Running/Paused/Completed/Aborted；一个 Task 可有多个 session。
3. `completion_type` 在 Created/Running/Paused 时为空；Completed/Aborted 时必须存在且与状态一致。
4. Start/Pause/Resume/Complete/Skip 生成规定的领域事件；同一转换中的 sequence 连续，event/session ID、epoch 时间和单调时间由调用上下文注入，禁止在 reducer 内访问系统时钟或随机数。
5. Timer 到 0 只能令当前专注段暂停/待用户决策，不能自动完成 session 或 Task。
6. 显式 Complete 才能生成 `task.completed`；重复 Complete 不生成新事件或 sequence。
7. 完整快照恢复同一 session；用户选择保存/结束时 session 可为 `AutoSaved`，但 Task 不自动 Completed。损坏快照产生 Aborted session，Task 仍不自动 Completed。
8. 实际专注秒数来自单调时间累计并排除暂停区间；拒绝负数和倒退时间。
9. reducer 不包含或引用 `sync`、LVGL、Wi-Fi、GPIO、ESP-IDF、FreeRTOS 或 BSP 头文件。

### 必测场景

- 所有合法转换与非法状态拒绝；Start 的 task/session 事件顺序。
- pause/resume 多轮计时；单调时钟倒退；Timer 到 0 不完成。
- 显式完成、重复完成、Skip；一个 Task 的第二个 session。
- 完整/损坏快照两条重启路径以及 Task 不被隐式完成。
- 每个失败结果的输入状态保持不变、无事件、sequence 不被消耗。

不得只用 `assert` 后在 Release/NDEBUG 下失效；测试 runner 要显式统计 case/assertion、失败返回非 0。目标至少 20 个独立 case，报告写出精确数量。

### 验收与提交

- 本机编译、链接并实际运行领域单测；0 失败。
- P4 接口契约仍 PASS；依赖扫描 0 个禁止头；`git diff --check` PASS。
- 提交：`feat(WB-STREAM-002): implement domain reducer`
- push 后记录 `CP1_CHECKPOINT=<hash>`，立即进入 CP2。

## 6. CP2：平台无关 Transactional Outbox 核心

### 目标

实现 `EventSink` 的平台无关事务核心和可注入存储接口，并用主机 fake 模拟掉电/重启/写失败。真实 NVS/文件系统适配器不在本流范围内。

### 允许修改

- `firmware/main/sync/**`
- `firmware/tests/unit/sync/**`
- `firmware/tests/fakes/**`
- `tools/dev/verify-host-cpp-tests.ps1`
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`

### 必须实现

1. `persistTransition` 原子提交完整下一领域快照与全部事件；任一步失败时二者都不可见。
2. 每设备 sequence 严格递增并作为持久化状态恢复；禁止 reducer 或 UI 私自分配已提交 sequence。
3. pending 上限 200；关键业务事件（至少 Task/Session Started/Completed）不能被容量策略静默丢弃。
4. `sync.failed`/`sync.recovered` 只进入受限、可合并的本地诊断槽，不进入业务 pending 队列、不占用 200 条容量。
5. ACK 清理必须同时满足逐事件结果为 Accepted/Duplicate 且 `sequence <= last_acked_sequence`；Conflict/Rejected/Gap 及 ACK gap 之后的事件全部保留。
6. 401/403/Auth、网络和 5xx 不删除 pending；业务 4xx 可标记死信摘要但保留原 event_id 和原文供修复重放。
7. fake 存储支持确定性故障注入和重新实例化，以模拟“提交前失败、提交中失败、响应丢失、进程重启”。不得将 fake 宣称为真实掉电安全或 NVS 已验证。
8. 所有 API 明确所有权和线程假设；本轮至少保证单线程确定性，不虚构真机并发安全。

### 必测场景

- 快照与多事件成功原子提交；每个故障注入点均完全回滚。
- 重启后恢复 pending、ACK 和下一 sequence；重复 replay 收敛。
- 200 条边界、非关键诊断合并、关键事件无空间时整次转换失败且旧状态保留。
- Accepted/Duplicate 清理、gap 前缀、Conflict/Rejected/Gap/Auth/Network/5xx/业务 4xx 保留策略。
- 同 event_id 修复重放不生成新 ID；非法/非连续 sequence 被拒绝且不污染存储。

目标至少 20 个独立 sync case；所有 host 测试总计与失败数写入报告。

### 验收与提交

- 主机全量测试编译、链接、运行 0 失败，连续运行 5 次稳定通过。
- P4 接口契约 PASS；禁止依赖扫描 PASS；`git diff --check` PASS。
- 提交：`feat(WB-STREAM-002): add transactional outbox core`
- push 后给出 `CP2_CHECKPOINT=<hash>` 和 `STREAM_CHECKPOINT_READY`，停止扩项并通知 Codex。

## 7. 工作流报告最低内容

每个 checkpoint 更新同一报告并记录：父提交、提交主题（当前提交自身可按规范描述）、修改文件、验收逐项结果、完整命令、编译器版本、case/assertion 数、关键输出、范围偏差、未解决风险和 Codex 复检重点。下一 checkpoint 回填上一 checkpoint 的精确 hash。

## 8. 停止条件

- CP0 无法获得能在本机执行测试二进制的可信 C++17 工具链。
- 当前 checkpoint 验证失败且无法在允许路径内修复。
- 需要修改范围外文件、官方固件、真机、Flash、分区、NVS 实现或硬件配置。
- 需要管理员权限、关闭安全校验、真实凭据/儿童数据或外部业务服务。
- 领域契约存在两种不兼容解释，且架构文档无法唯一决定。

触发后保留已推送 checkpoint，在报告中标记 `BLOCKED` 并停止；不得跳过失败项或自行开启 UI/设备集成。
