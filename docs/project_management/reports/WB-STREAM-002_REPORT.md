# WB-STREAM-002 工作流报告：主机侧 MVP 闭环连续开发

- 工作流 ID：WB-STREAM-002
- 分支：`workbuddy/domain-offline-stream`
- 任务包：`docs/project_management/tasks/WB-STREAM-002_DOMAIN_OFFLINE.md`（CP0~CP8 扩展版）
- 起始提交（远端任务分支 HEAD）：`03383dbda702e95f69e5e59eab0332526a2915bd`（= origin/main，含 Codex 预检完善 `ae74ab2` 与修复 `f021233`）
- 执行方式：CP0 → CP1 → … → CP8 连续执行，checkpoint 之间不等待 Codex
- 状态：`CHECKPOINT_READY`（按 checkpoint 更新）
- 更新日期：2026-09-02

## 1. 工作流状态表

| Checkpoint | 状态 | 提交 | 验证摘要 |
| --- | --- | --- | --- |
| CP0 本机 C++17 门槛 | `CHECKPOINT_READY` | `690d8488c7d42f4c92735c66dc4d6417f46fb15b`（`test(WB-STREAM-002): add native C++ test gate`） | 主机 g++ 16.2.0 compile/link/run PASS；P4 接口契约 exit=0 |
| CP1 纯领域 Reducer | `CHECKPOINT_READY` | `09984477015f9bbbab2cf2f8766c32db28ce2158`（`feat(WB-STREAM-002): implement domain reducer`） | 领域单测 27/27 PASS（cases=27 failures=0）；依赖扫描 PASS；接口契约 exit=0 |
| CP2 transactional Outbox | `CHECKPOINT_READY` | `feat(WB-STREAM-002): add transactional outbox core`（本提交自身） | outbox 21/21 + domain 27/27 PASS；连续 5 轮 EXIT=0；接口契约 exit=0 |
| CP2 transactional Outbox | `QUEUED` | — | — |
| CP3 应用协调器 | `QUEUED` | — | — |
| CP4 UI Presenter | `QUEUED` | — | — |
| CP5 持久化后端 | `QUEUED` | — | — |
| CP6 家长 PWA | `QUEUED` | — | — |
| CP7 主机 E2E | `QUEUED` | — | — |
| CP8 稳定性收口 | `QUEUED` | — | — |

## 2. CP0：建立本机 C++17"编译、链接、运行"门槛

### 2.1 工具链探测与安装记录

| 项 | 记录 |
| --- | --- |
| PATH/标准路径搜索（clang++/g++/cl.exe/vswhere/VS） | 均未发现；仅 winget v1.29.290 可用 |
| **安装 1：LLVM.LLVM 22.1.8**（任务包点名官方包） | `winget show --id LLVM.LLVM --exact` 复核：Nullsoft installer、URL `https://github.com/llvm/llvm-project/releases/download/llvmorg-22.1.8/LLVM-22.1.8-win64.exe`、SHA256 `16e5709785fef73c854646241c4a92c5cd574318d1b33c63330dd7721903e55c`；`--scope user` → `No applicable installer found`；改 `--custom "/S /D=E:\workbuddy\claw4-idf-tools\LLVM22"` → 安装成功（winget 日志 `Successfully verified installer hash` / `Successfully installed`），路径 `E:\workbuddy\claw4-idf-tools\LLVM22\bin\clang++.exe`（clang 22.1.8, x86_64-pc-windows-msvc） |
| LLVM 可用性实测 | **不可用于无 VS 主机**：`#include <cstdint>` → `fatal error: file not found`（MSVC-target 官方包不带标准库头，无 VS 亦无 MSVC 链接库）→ 无法用该编译器达成 compile/link/run 门槛 |
| **安装 2（备选尝试）：MSYS2.MSYS2** | winget 装至 `E:\workbuddy\claw4-idf-tools\msys64`；base 成功但 pacman 因 gpg "removing stale lockfile" 死循环（Windows NTFS 锁文件问题）无法安装 gcc；`pacman-key --init` 反复失败后放弃；未关闭 SigLevel/安全校验 |
| **采用：w64devkit v2.9.1（MinGW-w64，GCC 16.2.0）** | 官方 GitHub release `https://github.com/skeeto/w64devkit/releases/download/v2.9.1/w64devkit-x64-2.9.1.7z.exe`（TLS 下载，本地 SHA256 `9208c19755cd4964b7915b9afcf02c66d493a4c870c4b3e83f6c538d9c1237a5`）；7zr 解压至 `E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe`（GCC 16.2.0，x86_64-w64-mingw32，自带 CRT/链接器，可独立编译链接运行） |
| 关键工程结论 | MinGW-w64 gcc 通过 **PATH** 定位 `as`/`ld` → 验证脚本将编译器 bin 目录前置到 `$env:PATH`（内建逻辑，非写死单一用户路径） |
| 安装位置 | `E:\workbuddy\toolchains\`、`E:\workbuddy\claw4-idf-tools\`（用户工具目录，仓库外，均未提交；全程无提权、未关闭证书/哈希校验） |

> 说明：任务包允许途径（LLVM.LLVM）在无 Visual Studio 的本机实测无法完成标准 C++ 编译/链接/运行（MSVC-target 官方包不含 STL 头与 MSVC 链接库）。为达成 CP0"真实 compile/link/run"目标且不请求提权、不降级安全，采用来源可验证的 w64devkit（MinGW-w64 GCC）作为主机 C++17 编译器；全部安装证据、SHA256 与失败记录如上，工具链文件未进入仓库。若 Codex 判定应采用其他官方途径，可在该范围内替换 `-CompilerPath`。

### 2.2 修改文件（CP0）

- `tools/dev/verify-host-cpp-tests.ps1`（新建）
- `firmware/tests/host/smoke_test.cpp`（新建）
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`（新建，本报告）
- `.gitignore`（未修改；现有规则已覆盖 `out/`）

### 2.3 验证命令与结果（CP0）

```powershell
# 主机 smoke：compile/link/run 三阶段（结果见 out/host-tests/host_result.txt）
.\tools\dev\verify-host-cpp-tests.ps1 -CompilerPath "E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe" -CrossCompilerPath "E:\workbuddy\claw4-idf-tools\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-g++.exe"
# 结果（脚本 exit 0）：
#   Compiler : E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe (GCC) 16.2.0
#   compile : PASS (C++17 -Wall -Wextra -Werror)
#   link    : PASS
#   run     : PASS (exit=0)  [smoke] claw4 host smoke checksum=160 cpp=201703
#   interface: exit=0（verify-interface-contracts.ps1 P4 交叉编译 PASS，无回归）
```

**过程证据**：① LLVM MSVC-target `cstdint not found` 实测输出；② MSYS2 pacman gpg lockfile 死循环记录；③ w64devkit 下载 SHA256 `9208c197...` 与 as/ld 需 PATH 定位结论（已内建脚本）。主机 smoke 程序为真实可执行文件，非 `-fsyntax-only` 交叉检查替代。

### 2.4 CP0 验收自检

| 验收标准 | 结果 |
| --- | --- |
| 主机 smoke 真实执行返回 0 | PASS（run exit=0） |
| 报告区分 compile/link/run | PASS（§2.3 三阶段独立记录） |
| 接口契约脚本 PASS | PASS（interface exit=0） |
| `git diff --check` | PASS |
| 工具链未提权/未关闭校验/未提交 | PASS（winget/7zr 均用户范围或便携解压，SHA256 记录） |
| 路径含空格/中文正常 | PASS（脚本基于 `$RepoRoot` 动态拼接） |

### 2.5 建议 Codex 复检重点（CP0）

1. 主机编译器选择偏离任务包点名的 LLVM.LLVM（MSVC-target 无 VS 不可用 → 采用 w64devkit MinGW-w64）——见 §2.1 完整证据，请确认可否接受或指定替代官方途径。
2. `verify-host-cpp-tests.ps1` 自动将编译器 bin 目录加入 PATH（MinGW 找 as/ld 依赖）的实现是否可接受、是否影响非 MinGW 编译器。
3. 工具链安装记录（包/版本/来源/路径/SHA256）是否满足 Codex 审计要求。

## 3. CP1：纯领域 Reducer 与状态机

### 3.1 修改文件（CP1）

- `firmware/main/learning_domain/reducer.h`（新建，纯 reducer 接口）
- `firmware/main/learning_domain/reducer.cpp`（新建，实现）
- `firmware/main/learning_domain/study_session.h`（修改，加单调时间簿记字段）
- `firmware/tests/unit/domain/domain_reducer_tests.cpp`（新建，27 个主机 case）
- `tools/dev/verify-host-cpp-tests.ps1`（修改，增加 unit 测试编译运行与依赖扫描）
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`（修改，回填 CP0 hash + 本段）

**仅上述允许路径**。未引入 sync/LVGL/Wi-Fi/GPIO/ESP-IDF/FreeRTOS/BSP 依赖（依赖扫描 PASS）；未实现持久化/网络/UI。

### 3.2 实现要点

| 项 | 内容 |
| --- | --- |
| 纯 reducer | `DomainReducer::reduce(state, intent, ctx)` → `TransitionResult{ok, reason, intent_result, next, drafts}`；输入不可变，失败返回空 next + 空 drafts |
| 上下文注入 | `ReducerContext{device_id, child_id, now_epoch, monotonic_ms, make_event_id, make_session_id}`；reducer 内不访问系统时钟/RNG/I/O；缺 id 源 → `MissingIdSource` 拒绝 |
| **无 sequence** | reducer 只产出 `EventDraft`（无 sequence 字段）；sequence 由 CP2 outbox 唯一分配（预检契约） |
| Task 转换 | Ready→InProgress（Start）→Paused↔InProgress；Complete→Completed；Skip 仅 Ready；Completed/Skipped 拒绝再 Start；重复 Complete/Skip → `Idempotent` 且零草稿 |
| Session 转换 | Created→Running→Paused↔Running→Completed(Manual)；`completion_type` 在 Created/Running/Paused 为空、Completed/Aborted 时必填（保持 optional 语义） |
| 时间累计 | `actual_seconds`/`pause_seconds` 由单调时钟累计（`segment_start/paused_at` 簿记字段持久化于快照）；负数/倒退 → `ClockWentBackwards` 拒绝 |
| Timer 语义 | `onSegmentTimeout` 只自动暂停专注段（Task/Session→Paused）并**零草稿**，绝不自动完成（I2） |
| 重启恢复 | `recoverSession(Intact)` 保持同一 session 零变更（上层 UI 决策）；`recoverSession(Corrupt)` → session `Aborted`+事件，Task 不自动 Completed（回 Paused）；`endRecoveredSession` → `AutoSaved`+事件，Task 不自动 Completed |
| 事件顺序 | Start 生成 `[task.started, study.session.started]`；Complete 生成 `[task.completed, study.session.completed]`（payload 含 completion_type/actual_seconds） |
| 事件 payload | 完成事件 payload 携带 `completion_type=manual/auto_saved/aborted` 供后端 read-model 投影 |

### 3.3 验证命令与结果（CP1）

```powershell
.\tools\dev\verify-host-cpp-tests.ps1 -CompilerPath "E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe" -CrossCompilerPath "E:\workbuddy\claw4-idf-tools\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-g++.exe"
# 结果（脚本 exit 0，out/host-tests/host_result.txt）：
#   compile/link/run : PASS（smoke）
#   4b scan          : PASS（learning_domain 无 forbidden include）
#   unit domain_reducer_tests : RUN PASS  cases=27 failures=0
#   interface        : exit=0（P4 交叉编译契约 PASS）
```

### 3.4 CP1 验收自检

| 验收标准 | 结果 |
| --- | --- |
| 本机编译、链接并实际运行领域单测，0 失败 | PASS（g++ 16.2.0，27/27） |
| 独立 case ≥20 且 runner 显式统计 | PASS（27 cases，`cases=27 failures=0`，失败返回非 0，非 assert/NDEBUG 依赖） |
| 合法/非法转换、事件顺序、多轮 pause/resume、时钟倒退、Timer 到 0、重复完成、Skip、第二 session、完整/损坏快照、失败保持原状态+空草稿 | PASS（对应 case 全绿，见测试文件） |
| 领域层无 sequence 分配 API | PASS（reducer 只产 EventDraft，无 sequence 字段/API） |
| P4 接口契约 PASS；禁止依赖扫描 0 头 | PASS |
| `git diff --check` | PASS |

### 3.5 CP1 结论

CP1 完成并推送（`09984477015f9bbbab2cf2f8766c32db28ce2158`）。立即进入 CP2（transactional outbox）。

## 4. CP2：平台无关 Transactional Outbox 核心

### 4.1 修改文件（CP2）

- `firmware/main/sync/outbox_storage.h`（新建，可注入持久化接口：load/commit/commitDiagnostic/removeAcked/markDeadLetter）
- `firmware/main/sync/outbox_core.h`（新建，核心接口 + `kMaxPending=200`）
- `firmware/main/sync/outbox_core.cpp`（新建，实现）
- `firmware/tests/fakes/fake_outbox_storage.h`（新建，确定性故障注入 + 共享 FakeDisk 重启模拟）
- `firmware/tests/unit/sync/outbox_core_tests.cpp`（新建，21 个主机 case）
- `tools/dev/verify-host-cpp-tests.ps1`（修改，unit 编译加 sync 实现源与 fakes include 路径）
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`（修改，回填 CP1 hash + 本段）

**仅上述允许路径**。无 NVS/文件系统/网络实现；单线程确定性（API 注释明示所有权假设）。

### 4.2 实现要点

| 任务包要求 | 实现 |
| --- | --- |
| 原子持久化 | `OutboxCore::persistTransition`：空草稿转换只提交快照（不消耗 sequence）；有草稿时分配连续 sequence、物化 `PendingEvent`、`storage.commit(next_domain, rows, next_seq)` 单次原子提交；失败零可见变化 |
| **Outbox 唯一 sequence 所有者** | sequence 从持久化 `next_sequence` 连续分配并随 commit 原子更新；reducer/UI/网络层无分配 API；失败不消耗、重启从已提交计数继续（无空洞） |
| pending ≤200 / 关键事件不静默丢弃 | `kMaxPending=200`；超限 → 整次 `CapacityExceeded`（不裁剪、不静默丢弃关键事件）；`isCriticalEvent` 覆盖 Task/Session Started/Completed/Skipped 等 |
| 诊断槽独立 | `setDiagnostic` 只写可合并 `DiagnosticSlot`（failed/recovered 最新状态），不进入业务队列、不占 200 容量 |
| ACK 清理双条件 | `applyBatchResult`：仅 Accepted/Duplicate 且 `sequence <= last_acked_sequence`（连续前缀内）才 `removeAcked`；Conflict/Rejected/Gap 与 prefix 后行保留 |
| 401/403/网络/5xx 不删 pending | 无 eligible in-prefix 结果 → 零删除；401/403 rejected 不标死信 |
| 业务 4xx 死信 | `markDeadLetter` 保留原 event_id/原文 + 红色摘要 reason，可同 ID 修复重放 |
| fake 故障注入/重启 | `FakeDisk.fail_next_commit` 确定性注入（commit 前失败=零写入）；新 `FakeOutboxStorage` 复用同一 FakeDisk = 进程重启；未宣称真实掉电安全/NVS |
| 防重复 pending | drafts 的 event_id 已 pending → `InvalidTransition` 拒绝（响应丢失重发走 re-sync，不重复入队） |

### 4.3 验证命令与结果（CP2）

```powershell
.\tools\dev\verify-host-cpp-tests.ps1 -CompilerPath "E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe" -CrossCompilerPath "E:\workbuddy\claw4-idf-tools\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-g++.exe"
# 结果（exit 0，out/host-tests/host_result.txt）：
#   unit domain_reducer_tests : cases=27 failures=0 RUN PASS
#   unit outbox_core_tests    : cases=21 failures=0 RUN PASS
#   unit summary : 2 / 2 PASS
#   interface exit=0（P4 交叉编译契约 PASS）
# 连续运行 5 轮：ROUND1~5 EXIT=0（out/host-tests/stability_5runs.txt）
```

**必测场景覆盖（21 case）**：快照+多草稿+sequence 原子提交与各故障注入点回滚（`commit_failure_rolls_back_all/keeps_old_domain`）· 重启恢复 pending/ACK/next sequence（`restart_recovers_state`）· 响应丢失后重启 pending 保留（`restart_after_lost_response_keeps_pending`）· 200 边界（`capacity_exceeded_rejected_whole`）· 诊断槽不占容量/合并（`diagnostic_slot_outside_budget/merges_latest`）· Accepted/Duplicate 清理、gap 前缀、Conflict/Rejected/Gap/401 保留（ack 系列 6 case）· 业务 4xx 死信保留原文（`business_4xx_marks_dead_letter_keeps_row`）· 同 event_id 防重复/重放不生成新 ID（`duplicate_event_id_persist_rejected`、`dead_letter_replay_same_event_id`）· 非法空 event_id 拒绝零污染（`invalid_empty_event_id_rejected`）。

### 4.4 CP2 验收自检

| 验收标准 | 结果 |
| --- | --- |
| 主机全量测试 compile/link/run 0 失败 | PASS（domain 27 + outbox 21，均 exit 0） |
| 连续运行 5 次稳定通过 | PASS（ROUND1~5 EXIT=0） |
| P4 接口契约 PASS | PASS（interface exit=0） |
| 禁止依赖扫描 PASS | PASS（4b scan） |
| `git diff --check` | PASS |

### 4.5 CP2 结论

CP2 完成并推送（精确 hash 由 CP3 报告回填）。立即进入 CP3（设备应用协调器）。
