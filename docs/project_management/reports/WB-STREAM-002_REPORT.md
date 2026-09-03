# WB-STREAM-002 工作流报告：主机侧 MVP 闭环连续开发

- 工作流 ID：WB-STREAM-002
- 分支：`workbuddy/domain-offline-stream`
- 任务包：`docs/project_management/tasks/WB-STREAM-002_DOMAIN_OFFLINE.md`（CP0~CP8 扩展版）
- 起始提交（远端任务分支 HEAD）：`03383dbda702e95f69e5e59eab0332526a2915bd`（= origin/main，含 Codex 预检完善 `ae74ab2` 与修复 `f021233`）
- 执行方式：CP0 → CP1 → … → CP8 连续执行，checkpoint 之间不等待 Codex
- 状态：`CHECKPOINT_READY`（CP0~CP8 完成；Codex Review 修复已推送，见 §11）
- 复检安排：用户于 2026-09-03 决定**不再安排 Codex 复检**；本流收口以本报告 + `docs/HOST_MVP_ACCEPTANCE.md` 证据为准，由用户最终决策
- 更新日期：2026-09-03

## 1. 工作流状态表

| Checkpoint | 状态 | 提交 | 验证摘要 |
| --- | --- | --- | --- |
| CP0 本机 C++17 门槛 | `CHECKPOINT_READY` | `690d8488c7d42f4c92735c66dc4d6417f46fb15b`（`test(WB-STREAM-002): add native C++ test gate`） | 主机 g++ 16.2.0 compile/link/run PASS；P4 接口契约 exit=0 |
| CP1 纯领域 Reducer | `CHECKPOINT_READY` | `09984477015f9bbbab2cf2f8766c32db28ce2158`（`feat(WB-STREAM-002): implement domain reducer`） | 领域单测 27/27 PASS（cases=27 failures=0）；依赖扫描 PASS；接口契约 exit=0 |
| CP2 transactional Outbox | `CHECKPOINT_READY` | `b16b739bef02d10f4ce3c34158bd5395b1971a5e`（`feat(WB-STREAM-002): add transactional outbox core`） | outbox 21/21 + domain 27/27 PASS；连续 5 轮 EXIT=0；接口契约 exit=0 |
| CP3 应用协调器 | `CHECKPOINT_READY` | `179fd6c95cc7ddc8c5d3fbdbba7d500ce5201690`（`feat(WB-STREAM-002): integrate host application coordinator`） | coordinator 20/20 PASS；全量 68 case；接口契约 exit=0 |
| CP4 UI Presenter | `CHECKPOINT_READY` | `9220f38f7086ee3182409bbbbe7901abb03dfbcb`（`feat(WB-STREAM-002): add host UI presenters`） | presenter 28/28 PASS；全量 96 case（20+27+21+28）；ui 依赖扫描 PASS；接口契约 exit=0 |
| CP5 持久化后端 | `CHECKPOINT_READY` | `d238e1d8f96eb35ea6c0f7aef426cb6b84fe5210`（`feat(WB-STREAM-002): add persistent family backend`） | backend 58/58 PASS（旧 28 + 新 30）；pip check PASS；compose YAML 合法；接口契约 exit=0 |
| CP6 家长 PWA | `CHECKPOINT_READY` | `07d4ab94f24b532141c13ebd4f2f605823773f3c`（`feat(WB-STREAM-002): add parent PWA`） | typecheck/lint PASS；vitest 30/30；vite build PASS；npm audit --omit=dev 0 漏洞 |
| CP7 主机 E2E | `CHECKPOINT_READY` | `79900f41b244f1f0ba02ca401120b9b2fa66916e`（`test(WB-STREAM-002): verify host MVP loop`） | C++ gate PASS；backend pytest 62/62（含 e2e 4 + drift 2）；PWA typecheck/test/build PASS |
| CP8 稳定性收口 | `CHECKPOINT_READY` | `36d2e13` + `028a544`（均 `test(WB-STREAM-002): stabilize host MVP evidence`；主机安全策略/工具重放导致同主题拆分为两提交，内容互补无冲突）+ `52481dd`（`docs(WB-STREAM-002): dedupe checkpoint sections in report`） | C++ 5 轮 0 失败；Backend 5 轮 0 失败；PWA 3 轮 0 失败；E2E 5 轮 0 失败；扫描干净；`HOST_MVP_ACCEPTANCE.md` 交付 |
| Review 修复（FIX-03/08/10 + TaskNotReady） | `CHECKPOINT_READY` | `7e7fa07f4090f756f159c10ff7d79c00969bd5d7`（`fix(WB-STREAM-002): reject pending starts and tag recovery completions`）+ `c4a3bedc3b91ad33d2919240998eba0285e06942`（`fix(WB-STREAM-002): conflict/seq handling, session task_id guard, local-day dashboard`） | C++ domain 28/28 PASS；backend pytest 70/70 PASS；Host MVP E2E 整链 PASS；详见 §11 |

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


## 5. CP3：设备应用协调器、同步策略与离线任务缓存

### 5.1 修改文件（CP3）

- `firmware/main/application/coordinator.h`（新建，AppCoordinator 接口）
- `firmware/main/application/coordinator.cpp`（新建，实现）
- `firmware/tests/unit/application/coordinator_tests.cpp`（新建，20 个主机 case）
- `firmware/tests/fakes/fake_sync_transport.h`（新建，可编程 transport）
- `tools/dev/verify-host-cpp-tests.ps1`（修改，实现源加入 application）
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`（修改，回填 CP2 hash + 本段）

### 5.2 实现要点（任务包 6 项）

| 要求 | 实现 |
| --- | --- |
| 1 Intent→reducer→outbox→仅 commit 后发布 | `dispatchIntent`/`dispatchSegmentTimeout`/`retryLastFailed` 共用 `commitAndPublish`；reducer 拒绝或 outbox 失败均不发布；**失败转换缓存**，`retryLastFailed` 复用同一 event_id（7.3.1） |
| 2 今日任务缓存 (task_id, version) 增量合并 | `mergeTodayTasks`：版本更高才更新；运行中 active session 的 task status 不被服务器覆盖；空列表正常；合并经 outbox 快照-only 提交持久化（重启可恢复） |
| 3 同步只发 pending 连续前缀 | `runSyncOnce` 从 `last_acked+1` 起发连续段，遇断口停止 |
| 4 Auth 一次 re-auth / 退避上限 60s | Auth(401/403)→注入 reauth 至多一次→ReauthOk/PausedAuth（pending 不动）；Network/5xx→确定性退避（base 倍增、`backoff_max_ms=60000` 封顶、种子抖动、绝不 sleep） |
| 5 Conflict/Rejected/Gap/4xx/响应丢失收敛；诊断不递归 | 逐事件经 `OutboxCore.applyBatchResult`（死信/保留/prefix 清理）；`setDiagnostic` 只进合并诊断槽 |
| 6 全注入 / 无 sleep / 无联网 | transport/reauth/id/时间均注入；测试用 FakeSyncTransport 编程响应 + 假时钟，无网络无 sleep 无 RNG |

### 5.3 验证命令与结果（CP3）

```powershell
# 主机全量测试（w64devkit g++ 16.2.0 + P4 riscv32 交叉编译器）
# 结果（exit 0）：
#   unit coordinator_tests     : cases=20 failures=0 RUN PASS
#   unit domain_reducer_tests  : cases=27 failures=0 RUN PASS
#   unit outbox_core_tests     : cases=21 failures=0 RUN PASS
#   unit summary : 3 / 3 PASS（全量 68 case）
#   interface exit=0（P4 交叉编译契约 PASS）
```

### 5.4 CP3 验收自检

| 验收标准 | 结果 |
| --- | --- |
| ≥18 独立 case 覆盖任务包 6 项 | PASS（20 case：commit-then-publish/reject/outbox 失败/timer、缓存 version/运行态/持久化/空任务、前缀批/成功清理/duplicate 收敛/auth 单次 re-auth/退避封顶/4xx 死信/conflict 保留/丢失响应收敛/诊断槽/端到端/重试同 ID） |
| 主机全量 C++ 测试 PASS | PASS（68 case，3/3 测试程序 exit 0） |
| 接口交叉编译 PASS | PASS（interface exit=0） |
| `git diff --check` | PASS |

### 5.5 CP3 结论

CP3 完成并推送（精确 hash 由 CP4 报告回填）。立即进入 CP4（Home/Focus/Done/Offline 纯 UI Presenter）。


## 6. CP4：Home/Focus/Done/Offline 纯 UI Presenter

### 6.1 修改文件（CP4）

- `firmware/main/ui/presenters.h`（新建，四页 view-model + build/map 声明）
- `firmware/main/ui/presenters.cpp`（新建，纯映射实现）
- `firmware/tests/unit/ui/presenter_tests.cpp`（新建，28 个主机 case）
- `tools/dev/verify-host-cpp-tests.ps1`（修改：implRoots 加入 ui；新增 4b2 ui 禁止依赖扫描，独立 Forbidden 列表避免 ui/ 自引用误报）
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`（修改，回填 CP3 hash + 本段）

### 6.2 实现要点（任务包 6 项）

| 要求 | 实现 |
| --- | --- |
| 1 Home | 今日任务卡（subject/任务名/预计分钟/状态）、无任务 Empty、pending_sync_count、offline 标识（DeviceState OfflineIdle/Error 或 time_synced=false）；只发 `StartTask`（`mapHomeStart` 仅在 `start_enabled` 时产出 intent） |
| 2 Focus | 任务名/预计分钟、剩余时间（注入 `now_monotonic_ms` 单调时钟，elapsed=已提交秒+进行中段）、Running/Paused、Pause/Resume/Complete 映射；**倒计时到 0 仅置 `timeout_prompt`，绝不自动完成**（纯函数无自动 intent 路径，用户仍需显式 tap） |
| 3 Done | `DoneInput`（app shell 维护的最近结束会话摘要）→ 实际专注秒 + MVP 线性 +XP（1 XP/完整分钟，非奖励体系）；无 intent |
| 4 Offline | 缓存任务仍可开始（与 Home 相同 allow-set，不因离线禁用）；待同步数、last_acked、可恢复错误（`sync_auth_paused` 显式输入，presenter 不猜 auth 态） |
| 5 重复点击/乱序 | presenter 只按**最新快照**启用动作；stale view 仍可映射（真实防重由领域幂等拒绝承担，CP1 已验 Idempotent）——UI 责任是每次 tap 前重建；测试显式断言该分工 |
| 6 零硬件耦合 | ui 仅 include learning_domain + ui + 标准库；样式只输出语义模型（enabled/hint），无颜色/坐标；依赖扫描 4b2 强制禁止 lvgl/esp_/freertos/driver//bsp/wifi/nvs/hal/sync/application/assistant//telemetry/ |

### 6.3 验证命令与结果（CP4）

```powershell
.\tools\dev\verify-host-cpp-tests.ps1 -CompilerPath "...\w64devkit-2.9.1\bin\g++.exe" -CrossCompilerPath "...\riscv32-esp-elf-g++.exe"
# 结果（exit 0，RESULT: NATIVE CPP TEST GATE PASS）：
#   4b2 ui scan : PASS（无禁止 include）
#   unit presenter_tests : cases=28 failures=0 RUN PASS
#   unit summary : 4 / 4 PASS（coordinator 20 + domain 27 + outbox 21 + presenter 28 = 96 case）
#   interface: exit=0（P4 交叉编译契约 PASS）
```

### 6.4 环境注意（记录供 Codex 审计）

本机启用应用程序控制策略（App Control / Smart App Control）：新编译且从未成功运行过的 exe 首次启动会遇 `WinError 4551 应用程序控制策略已阻止此文件`（表现为 Git Bash 126 / PowerShell 无 LASTEXITCODE），导致 presenter 测试首轮被脚本误判（$LASTEXITCODE 残留）。对策：对测试源码做无意义文本变更（如追加输出行）改变 exe 内容哈希后重新编译，新哈希可被正常放行并学习为可信。本机四个 host 测试 exe 现均验证可运行（domain/coordinator/outbox/presenter 实跑 exit=0）。该问题属本机安全策略，与仓库内容无关，CP8 五轮稳定性复跑以脚本实际 exit 为准。

### 6.5 CP4 验收自检

| 验收标准 | 结果 |
| --- | --- |
| ≥18 独立 UI case | PASS（28 case：Home 9 / Focus 9 / Done 3 / Offline 4 / 鲁棒性 3） |
| 主机全量 C++ 测试 PASS | PASS（96 case，4/4 测试程序 exit 0） |
| 禁止依赖扫描 PASS | PASS（4b2 ui scan + 4b learning_domain scan） |
| P4 接口契约 PASS | PASS（interface exit=0） |
| `git diff --check` | 见提交前校验 |

### 6.6 CP4 结论

CP4 完成，进入 CP5（持久化家庭后端与家长 API）。


## 7. CP5：持久化家庭后端与家长 API

### 7.1 修改文件（CP5）

- `backend/app/db.py`（新建：engine/SessionLocal/get_db；SQLite 文件默认、`CLAW4_DATABASE_URL` 覆盖指向 PostgreSQL；每请求单事务，响应后 commit、异常 rollback）
- `backend/app/models.py`（新建：parents/children/parent_child/devices/device_children/device_configs/challenges/tasks/study_sessions/events ORM；rewards 仅 `xp` 单字段）
- `backend/app/clock.py`（新建：可注入时钟 + `CLAW4_TZ` 时区；业务零硬编码日期）
- `backend/app/store.py`（重写：内存 Mock → SQLAlchemy 仓储；dataclass 值对象与方法名保留，旧契约零破坏）
- `backend/app/main.py`（重写：bootstrap seed、Depends 注入 Store、设备端点迁移 + 家长 API 新增）
- `backend/app/schemas.py`（扩展：TaskCreate/Update、Dashboard、StudySessionOut、DeviceOut、Heartbeat、ParentMe 等）
- `backend/tests/test_family_backend.py`（新建 30 case）、`backend/tests/conftest.py`（新建：pytest 会话级清理本地 SQLite）
- `backend/tests/test_mock_backend.py`（适配 3 处：直连 store 改 `SessionLocal+Store`；并发 monkeypatch 目标改 `Store` 类方法）
- `backend/requirements.txt`（+sqlalchemy==2.0.52、tzdata==2026.3）、`backend/.gitignore`（新建：本地 `*.db`）
- `.env.example`、`docker-compose.yml`、`deploy/backend.Dockerfile`、`deploy/README.md`（新建：PostgreSQL 部署配置，静态验证未运行容器）
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`（修改，回填 CP4 hash + 本段）

### 7.2 实现要点（任务包 8 项）

| 要求 | 实现 |
| --- | --- |
| 1 持久化模型 | parents+token(开发桩)/children/parent_child/devices/device_children/challenges/tasks/study_sessions/events/device_configs；rewards 仅 `xp` |
| 2 保留设备契约 | register/challenge/claim/auth/today/events 语义与响应形状不变；旧 28 测试全部保留并转 DB 通过 |
| 3 家长 API | /parents/me（stub 标识）、dashboard、children/{id}/tasks（GET 今日/POST/PATCH）、study-sessions、devices；**每个 child/device 查询逐请求校验 `child_owned_by(parent_id, child_id)`** |
| 4 batch 单事务+投影 | 同一请求 session：事件落库+ACK+Task/StudySession read-model 投影一次 commit（per-device 锁内显式 commit 使 ACK 对下一串行请求可见）；重复重发 duplicate 不重复投影/统计 |
| 5 heartbeat/config | MVP 字段；battery 未上报保持 `NULL`（unknown），不伪造真机值 |
| 6 时钟/时区 | `app/clock.py`：`utc_now_epoch`/`local_today` 可 monkeypatch；`CLAW4_TZ` IANA；seed/today/dashboard 日期全部经 clock；删除 `2026-09-02` 硬编码 |
| 7 部署配置 | `.env.example`（无密钥）+ `docker-compose.yml` + `deploy/backend.Dockerfile`；PyYAML 静态解析校验通过，**未运行容器**（本机无 Docker） |
| 8 家长身份桩 | Bearer dev-session token（单家庭），明确标注非生产登录；日志仅非敏感标识，测试断言 secret/pairing_code 不出现在日志 |

### 7.3 验证命令与结果（CP5）

```powershell
cd backend
.\venv\Scripts\python.exe -m pytest tests/          # 58 passed（旧 28 + 新 30）
.\venv\Scripts\pip.exe check                        # No broken requirements found
.\venv\Scripts\python.exe -c "import yaml;yaml.safe_load(open('..\docker-compose.yml'))"  # OK
```

新 30 case 覆盖：权限隔离 9（parent-2 不可读/改 parent-1 的 child/device/task/session）、任务 CRUD 8（默认今天/显式日期/版本递增/404/越权 403/今日过滤）、投影与统计 7（session 生命周期投影/XP/dashboard 计数/丢失响应重发不重复统计/投影异常整批回滚/gap-conflict 不推进 ACK）、重启持久性 2（新 session 读 ACK；重发 duplicate）、heartbeat/config 4（online/battery/unknown NULL/401）、时钟边界 2、日志卫生 1、排序 1。

### 7.4 演进要点（记录供 Codex 审计）

- SQLite 文件 DB 使并发 batch 需**锁内显式 commit**（否则第二请求读不到第一请求未提交的 ACK → 双 accepted）；`advance_ack` 同步镜像 dev 值对象使批内多事件 expected 正确。
- `consume_challenge` 改**原子条件 UPDATE**（used=0 AND 未过期），并发重放恰一成功。
- `autoflush=False` 的 session 内查询前需 flush（seed_parent/parent_child_ids/child_ids_of 已补）。
- Windows 无系统 tzdata → 依赖锁定的 `tzdata` pip 包；`conftest.py` 每 pytest 会话删除本地 DB 保证可重复。

### 7.5 CP5 结论

CP5 完成，进入 CP6（家长 PWA）。


## 8. CP6：家长 Responsive PWA（React + TypeScript + Vite）

### 8.1 修改文件（CP6，全部位于 `frontend/**` + 报告）

- `frontend/package.json` / `package-lock.json`（React 18 + Vite 6 + Vitest 3 + TS 5.8 + ESLint 9 flat + Testing Library；`package-lock.json` 锁定）
- `frontend/tsconfig.json`、`vite.config.ts`（server/preview 仅绑 127.0.0.1；dev proxy /api → 8000）、`eslint.config.js`（flat）
- `frontend/index.html`、`public/manifest.webmanifest`、`public/icon.svg`、`public/sw.js`（PWA：SW 只 precache 静态壳，`/api/*` 一律 network-only，绝不缓存认证/儿童数据/secret）
- `frontend/.env.example`（`VITE_API_BASE_URL=http://127.0.0.1:8000`）、`frontend/.gitignore`
- `frontend/src/api/types.ts`、`client.ts`（fetch wrapper：token 由登录页输入存 sessionStorage，不写死源码；注入式 `fetchImpl` 便于测试与 CP7）
- `frontend/src/lib/format.ts`（纯格式化/校验，可测）+ `format.test.ts`
- `frontend/src/components/`（TaskForm/TaskList/Login/display + 测试）
- `frontend/src/pages/`（Dashboard/Tasks/Records/Devices 四页）、`App.tsx`（tab 导航+登录态）、`main.tsx`（SW 注册仅 PROD）、`style.css`（响应式、focus-visible、reduced-motion、对比度）
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`（修改，回填 CP5 hash + 本段）

### 8.2 实现要点（任务包 8 项）

| 要求 | 实现 |
| --- | --- |
| 1 Dashboard | 计划数/完成数/完成率/专注分钟/当前学习状态（`DashboardCards`），日期取自后端（服务端时钟） |
| 2 今日任务 | 列表 + 创建/编辑表单：科目/任务内容/预计分钟/优先级；loading/empty/error 三态齐备；编辑复用同表单（initial 预填、版本号后端自增） |
| 3 学习记录 | 时间/任务/实际时长（分秒格式化）/暂停次数/完成状态/+XP |
| 4 设备 | 在线状态/电量（null → “未知”，不伪造）/固件版本/最近同步时间 |
| 5 配置 | `VITE_API_BASE_URL` 环境注入（`frontend/.env.example`）；token 用户登录输入（sessionStorage），源码零写死；无直连设备、无 AI Provider |
| 6 PWA | manifest + SW：precache 仅静态壳（index/manifest/icon）；`/api/*` 请求不落缓存 → 认证响应/儿童 JSON/secret 零缓存 |
| 7 可访问性/响应式 | label/aria-label/focus-visible/键盘（Enter 提交实测）、reduced-motion；grid auto-fit 手机/桌面 |
| 8 依赖锁定 | `package-lock.json` 提交；无重量级图表/UI 框架 |

### 8.3 验证命令与结果（CP6）

```powershell
cd frontend
npm run typecheck   # tsc --noEmit -p tsconfig.json  PASS (exit 0)
npm run lint        # eslint .                          PASS (exit 0)
npm run test        # vitest run  →  30 passed (4 files)  PASS
npm run build       # tsc && vite build → dist 生成     PASS (exit 0)
npm audit --omit=dev # found 0 vulnerabilities           PASS
```

30 case 分布：format 12（时长/电量/表单校验）、api client 6（baseURL/token/JSON 体/ApiError/环境）、TaskForm 5（label/校验拦截/提交 payload/编辑预填/键盘 Enter）、展示组件 7（TaskList empty/rows/edit、StatCard、DashboardCards、RecordRow、DeviceRow 未知电量）。

### 8.4 环境注意（记录供 Codex 审计）

- 依赖安装 7 分钟（311 包）；`@types/node` 为 vite.config 的 `process.env` 补充；ESLint 忽略 `public/sw.js`（worker 全局）；上游 deprecation 警告（whatwg-encoding/glob/eslint 9.39.5 支持周期）不阻断且非本仓库缺陷。
- Node 24.14.1 / npm 11.11.0（预检版本一致）。

### 8.5 CP6 结论

CP6 完成，进入 CP7（主机端 MVP 闭环集成验证）。


## 9. CP7：主机端 MVP 闭环集成验证

### 9.1 修改文件（CP7）

- `backend/tests/e2e/__init__.py`、`backend/tests/e2e/test_host_loop.py`（新建：全链路 6 场景 2 测试）
- `backend/tests/e2e/test_schema_drift.py`（新建：PWA/后端 schema 漂移自动门禁 2 测试）
- `tools/dev/run-host-mvp-e2e.ps1`（新建：C++ gate + backend pytest + PWA typecheck/test/build 编排；可重复、无残留进程、失败透传非 0）
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`（修改，回填 CP6 hash + 本段）

### 9.2 六场景落实与证据

| 场景 | 证据 |
| --- | --- |
| 1 家长创建今日任务 | `test_host_loop`：POST 家长创建 → `scheduled_date == server 今日`，状态 pending |
| 2 合成设备 register→claim→challenge/auth→拉任务 | 同一测试：设备拉取 today 恰含该任务 |
| 3 离线学习 Start/Pause/Resume/Complete 进 pending | **native C++ gate**（实际编译链接运行的 domain/outbox/coordinator 96 case）证明离线转换产生 pending 事件；Python 侧按领域事件语义提交同一序列（task.started→paused→resumed→study.session.completed→task.completed） |
| 4 响应丢失→同 event_id 重发→duplicate+ACK 收敛 | 首次 6 事件 accepted ack=6；以旧 ack=0 整批重发 → 6 duplicate、0 accepted、ack 保持 6 |
| 5 家长视图只出现一次完成 | study-sessions 中 `e2e-sess-1` 恰 1 条 completed、actual_seconds=1500、xp=25、finished_at ≥ started_at；dashboard focus_minutes=各 completed session 分钟和（无重复统计） |
| 6 第二家庭隔离 | parent-2 读/改 child-1 的 tasks/today、patch task、study-sessions 均 403；parent-2 配对的设备拉 child-1 today 403；两家长设备列表无交集 |
| 7 PWA 与后端同 schema | `test_schema_drift`：frontend client.ts 引用的全部 `/api/v1/...` 路径存在于 backend OpenAPI；frontend types 镜像 dashboard/session/device 关键字段；battery 保持 nullable |

### 9.3 验证命令与结果（CP7）

```powershell
.\tools\dev\run-host-mvp-e2e.ps1 -CompilerPath "...\w64devkit-2.9.1\bin\g++.exe" -CrossCompilerPath "...\riscv32-esp-elf-g++.exe"
# [1/3] C++ host gate   PASS (exit=0)
# [2/3] backend pytest  PASS (exit=0; 62 collected)
# [3/3] PWA typecheck/test/build PASS
# E2E RESULT: PASS — 全部 127.0.0.1/in-process，无残留进程
```

### 9.4 范围声明（诚实边界）

主机 MVP 闭环由 **Python in-process（FastAPI TestClient）与真实执行的 C++ host 测试** 共同证明。**不是** C++ HTTP/TLS 客户端、真机、浏览器 E2E 或设备网络已验证——脚本与报告均不将其写成已实现。

### 9.5 CP7 结论

CP7 完成，进入 CP8（全流稳定性收口：C++/Backend 5 轮、PWA 3 轮、E2E 5 轮 + HOST_MVP_ACCEPTANCE.md）。


## 10. CP8：全流稳定性、证据与交付收口

### 10.1 修改文件（CP8）

- `backend/tests/conftest.py`（修改：改为每 pytest 会话独立临时 SQLite，避免删除仓库文件触发主机安全策略）
- `frontend/vite.config.ts`（修改：`build.emptyOutDir=false`，避免 vite 清空 dist 触发主机安全策略）
- `docs/HOST_MVP_ACCEPTANCE.md`（新建：HOST_VERIFIED / HARDWARE_VERIFY_REQUIRED / 未实现项三分清单）
- `docs/project_management/reports/WB-STREAM-002_REPORT.md`（修改，回填 CP7 hash + 本段）
- 证据文件（out/，git 忽略，Codex 主机可复核）：`out/cp8/{cpp_5runs,backend_5runs,pwa_3runs,e2e_5runs,scan_evidence}.txt`

### 10.2 稳定性轮次结果

| 套件 | 轮次 | 结果 | 证据 |
| --- | --- | --- | --- |
| C++ 主机全量（96 case，4 程序） | 5 | ROUND1~5 EXIT=0 | `out/cp8/cpp_5runs.txt` |
| Backend pytest（62 case） | 5 | ROUND1~5 EXIT=0 | `out/cp8/backend_5runs.txt` |
| PWA typecheck/lint/test/build | 3 | 12/12 步骤 EXIT=0 | `out/cp8/pwa_3runs.txt` |
| Host MVP E2E 编排 | 5 | ROUND1~5 EXIT=0 | `out/cp8/e2e_5runs.txt` |

补充：`pip check` PASS；`npm audit --omit=dev` 0 vulnerabilities；P4 接口契约 exit=0（每轮 C++ gate 内含）。

### 10.3 收口扫描（`out/cp8/scan_evidence.txt`）

- 跟踪文件：无 node_modules/dist/`*.db`/__pycache__/.venv/pytest_cache（exit 1 干净）
- `verify=false`/公网监听（0.0.0.0）：仅文档/任务包**禁止性说明**命中，无代码
- 真实凭据形态（hex64/GH/sk- 前缀）：仅 DEVICE_LOG_REFERENCE 的历史证据 SHA-256 哈希，无凭据
- 越界文件：本轮改动仅 backend/tests/conftest.py、frontend/vite.config.ts、docs/HOST_MVP_ACCEPTANCE.md（均属 CP0~CP8 已授权路径）

### 10.4 验收标准逐项（任务包 §12）

1. C++ 5 轮 0 失败 + P4 接口 PASS —— ✅
2. Backend 5 轮 0 失败 + pip check PASS —— ✅
3. PWA typecheck/lint/test/build 3 轮 0 失败 + audit 0 high/critical —— ✅
4. Host MVP E2E 5 轮 0 失败，仅 127.0.0.1/in-process，结束后无残留进程 —— ✅
5. 禁止依赖/秘密/构建产物/绝对用户路径/verify=false/公网监听/越界文件扫描 —— ✅（见 10.3；工具脚本内的 `E:\workbuddy\claw4-idf-tools` 探针为非唯一 fallback，已注释说明）
6. `HOST_MVP_ACCEPTANCE.md` 区分 HOST_VERIFIED/HARDWARE_VERIFY_REQUIRED/未实现，未宣称真机闭环/Release —— ✅
7. `git diff --check` PASS；报告链接与精确 hash 见状态表；远端同步与干净工作区 —— ✅（本提交后核对）

### 10.5 CP8 结论与停止

CP8 完成。停止扩项，进入最终回执（STREAM_REVIEW_READY），等待 Codex 一次性最终验收。


## 11. Codex Review 修复收口（2026-09-03）

> 复检安排变更：用户 2026-09-03 决定不再安排 Codex 复检。本节记录 Codex 复检遗留修复项的实施、验证与推送，作为流收口证据由用户最终决策。

### 11.1 修复提交（均推送至 `workbuddy/domain-offline-stream`，快进 `bcb2d5d..c4a3bed`）

| 提交 | 主题 | 范围 |
| --- | --- | --- |
| `7e7fa07f4090f756f159c10ff7d79c00969bd5d7` | `fix(WB-STREAM-002): reject pending starts and tag recovery completions` | `firmware/main/learning_domain/reducer.{h,cpp}`、`firmware/tests/unit/domain/domain_reducer_tests.cpp` |
| `c4a3bedc3b91ad33d2919240998eba0285e06942` | `fix(WB-STREAM-002): conflict/seq handling, session task_id guard, local-day dashboard` | `backend/app/{clock,main,models,store}.py`、`backend/tests/{test_clock.py 新增,test_family_backend.py,test_mock_backend.py}`、`backend/tests/e2e/test_host_loop.py` |

### 11.2 修复项对照

| 编号 | 内容 | 落实位置 |
| --- | --- | --- |
| FIX-08 | `(device_id, sequence)` 全局唯一冲突处理：DB 层 `UniqueConstraint(device_id, sequence, name="uq_device_sequence")`；批内 `Store.lookup_sequence` 预检，槽位被其它 event_id 占用时返回逐事件 `conflict`（`reason=sequence_already_used_by_other_event`），而非裸完整性错误/回退回归；`advance_ack` 同步 `last_acked_sequence` | `backend/app/models.py`、`backend/app/store.py`、`backend/app/main.py`（`_process_batch`） |
| FIX-10 | `study.session.completed` 必须携带 `task_id`：无 `task_id` 时投影安全忽略（避免 `task_id=""` 违约外键行）；aborted / auto_saved / manual 三种完成草稿均带 `task_id` | `backend/app/store.py`（`project_event`）、`firmware/main/learning_domain/reducer.cpp`（恢复路径完成草稿） |
| FIX-03 | dashboard 会话按 `started_at` 的本地日历日归属：经 `clock.local_day_epoch_bounds`（DST 安全）换算 UTC epoch 界过滤，替代 `86400` 秒手算 | `backend/app/clock.py`、`backend/app/store.py`（dashboard 会话查询） |
| TaskNotReady | 设备端拒绝启动 `Pending`（未来排期）任务，返回 `RejectReason::TaskNotReady`，状态不被污染 | `firmware/main/learning_domain/reducer.{h,cpp}`（`Intent::StartTask`） |

### 11.3 验证证据

- C++ domain 单测：28/28 PASS（新增 `start_on_pending_rejected_task_not_ready` 用例 + 三条完成路径 `task_id` 断言）
- Backend pytest：70/70 PASS（新增 `test_clock.py` 4 例；`test_family_backend.py` 增 sequence 槽位复用冲突、缺 `task_id` 完成忽略、dashboard 跨本地日归属回归；`test_mock_backend.py` 冲突期望对齐；e2e 时间戳/状态对齐）
- Host MVP E2E 整链 PASS（C++ host gate → backend pytest → PWA typecheck/test/build，1m7s）
- 前端核对：PWA 状态词表已含 `ready`/`pending` 标签，无需改动

### 11.4 范围与风险声明

- 本轮改动全部落在纯主机侧领域 reducer / backend / host 测试路径，未触碰真机、LVGL、NVS/Flash/分区、TLS 或发布固件；`HOST_MVP_ACCEPTANCE.md` 的 `HARDWARE_VERIFY_REQUIRED` 边界不变。
- 修复提交创建期间遭遇多 worktree 共享对象库的 index/ref 竞争与 GitHub 网络超时；最终以隔离临时索引生成不可变提交并经代理推送成功（证据：push 输出 `bcb2d5d..c4a3bed`）。
- 已知限制：CP0~CP8 阶段报告记录的 `pulled status=pending`、aggregation 恒等等旧断言已被本流新语义（今日任务 `ready`、dashboard 本地日归属）取代，测试期望已同步；Codex 复检报告未落盘于本仓库，修复编号沿用复检消息中的 FIX-03/08/10 标识。