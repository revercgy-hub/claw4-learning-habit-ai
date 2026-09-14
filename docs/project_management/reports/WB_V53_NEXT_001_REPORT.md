# WB-V53-NEXT-001 WorkBuddy 实施报告

> 本文件按 checkpoint 追加。CP0 已提交；CP1～CP5 状态见各节的"状态"行。

## 0. 报告状态总览

| Checkpoint | 内容 | 状态 | 提交 |
| --- | --- | --- | --- |
| CP0 | 领取、基线核对、Host Gate、A03 文件清单 | 完成（CHECKPOINT_READY） | `78c2963` |
| CP1 | A03 网络请求与状态提交分离 | **部分完成（CP1a 协调器核心）**，设备侧仍待做 | 见 §10 |
| CP2 | A04 服务端快照不覆盖离线终态 | 未开始 | — |
| CP3 | B02 音频与提醒仲裁 Host | 未开始 | — |
| CP4 | B03 Reminder Host 与持久化恢复 | 未开始 | — |
| CP5 | 主机联合 Gate 与审查交付 | 未开始 | — |

## 1. 元信息

- 任务 ID：WB-V53-NEXT-001
- 结果：`CHECKPOINT_READY`（CP0 完成；CP1 部分完成，见 §10）。**非 `REVIEW_READY`：全包未完成，设备侧并发改造与 CP2～CP5 未做。**
- 分支：
  - 本地：`workbuddy-v53-next-001-reliability`
  - 远端：`workbuddy/v53-next-001-reliability`（任务书要求的名称，push 时用显式 URL 直接建远端引用）
  - **偏差说明**：任务书字面要求本地也用 `workbuddy/...`。本机 git 无法创建含 `/` 的引用（详见 §7.1），故本地改用等价的无斜杠分支名。这是环境强制，不是范围变更。
- 工作区：`E:/workbuddy/claw4-wb-v53-next-001`（独立克隆，不写控制目录、不占用 `E:/c` 镜像）
- Base SHA：`7b3df6c5f8c53c1cf2d0486c62e01a3a2ec16e8c`
- 冻结代码 SHA：`e70c2830d1f7ea43629a61ce01a33f78b6f11128`
- 执行时间：2026-09-14

## 2. 输入与范围

已读取输入：

- 根 `AGENTS.md`、`docs/project_management/TASK_BOARD.md`、`docs/project_management/tasks/WB-V53-NEXT-001.md`
- `docs/project_management/reports/CODEX_V53_WAVE1_CLOSEOUT_2026-09-14.md`
- `docs/project_management/tasks/CODEX-V53_AGENT_WORK_PACKAGES.md`（A03/A04/B02/B03）
- `docs/project_management/tasks/V53_A03_RUNTIME_CONCURRENCY_ADR.md`
- 实际源码：`firmware/main/application/coordinator.{h,cpp}`、`firmware/main/sync/{http_transport.h,scheduled_http_transport.cpp,learning_backend_session.{h,cpp}}`、`integration/metalio_claw4/device/app/learning_runtime.{h,cpp}`、`integration/metalio_claw4/device/ports/metalio_http_transport.cpp`、`integration/metalio_claw4/device/learning_screen/learning_screen.cc`、`integration/metalio_claw4/host_glue/learning_app.h`、`integration/metalio_claw4/patches/project-ca3aa3fa.json`

允许修改路径（本包）：按任务书 §3～§7 各 checkpoint 白名单。CP0 只写 `docs/project_management/reports/WB_V53_NEXT_001_REPORT.md`。

实际范围偏差：仅 1 项，且为环境强制（分支名，见 §1 / §7.1）。

## 3. CP0 交付一：基线核对与首批三包确认

### 3.1 冻结 SHA → 交接 HEAD 差异核对（任务书条件验证）

任务书 §1 规定："相对冻结代码SHA只允许本次文档/看板收口变化，额外代码变化须先报告"。

`git diff --name-status e70c2830d1f7ea43629a61ce01a33f78b6f11128 7b3df6c5f8c53c1cf2d0486c62e01a3a2ec16e8c` 实测结果：

| 变化 | 文件 |
| --- | --- |
| M | `.workbuddy/INSTRUCTIONS.md` |
| A | `.workbuddy/INSTRUCTIONS_PRE_V53_20260914.md` |
| M | `README.md` |
| M | `docs/project_management/TASK_BOARD.md` |
| A | `docs/project_management/reports/CODEX_V53_WAVE1_CLOSEOUT_2026-09-14.md` |
| M | `docs/project_management/tasks/WB-V53-NEXT-001.md` |

**结论：6 项全部为文档/看板，无任何源码或测试变化。条件满足，未发现额外代码变化，无需先行报告。**

### 3.2 远端复核

`git ls-remote --heads https://github.com/revercgy-hub/claw4-learning-habit-ai.git` 实测：

- `refs/heads/codex/v53-foundation-wave1` = `7b3df6c5f8c53c1cf2d0486c62e01a3a2ec16e8c`
- `refs/heads/codex/v53-architecture-task-plan` = `1ae2a0c546615666a57abdf601b343bc88a043c4`

与本地交接 HEAD 一致，远端未推进。

### 3.3 首批三包验收范围（引用收口报告，未重新验收）

| 任务 | 主 agent 结论 | 边界 |
| --- | --- | --- |
| A01 | ACCEPTED（工具/取证） | 不是完整 IDF 构建证明 |
| A02 | ACCEPTED（源码/Host） | 未操作真实 NVS，未做本批真机页面验证 |
| B01 | ACCEPTED（Host + RV32 语法） | 未接 SNTP 或校时硬件 |

### 3.4 A01 补丁清单与仍待 A05 的硬件边界

A01 最终上游补丁（`integration/metalio_claw4/patches/project-ca3aa3fa.json`）：

- `upstream_commit` = `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950`
- `patch_sha256` = `58bfbe5266a9fa00980be5e5078b570b9915165e199981e6dd54dcb21aa81937`
- 四文件：`main/display/screen/home_screen/home_screen.cc`、`main/CMakeLists.txt`、`main/display/lv_adapter_display.cc`、`main/audio/audio_service.cc`

仍待 A05 验证的硬件边界（不在本包范围）：

- 无 IDF 完整构建、无设备运行、无串口、无 Flash、无真实 NVS 验证
- NVS shim 只验证适配器逻辑，不证明 ESP-IDF ABI / Flash 原子性
- 已有 D5 double-free 修复与音频 yield 有效性须在 A05 同一候选上验证
- 原有 CMake 补丁尚未登记新增 `time` 模块，本批功能不能被宣称已进入固件

## 4. CP0 交付二：Host Gate 基线

工具链（按收口报告指定的实际路径，未凭 PATH 推断）：

- native：`E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe` → g++ (GCC) 16.2.0
- cross：`E:/workbuddy/claw4-idf-tools/tools/riscv32-esp-elf/esp-14.2.0_20260121/riscv32-esp-elf/bin/riscv32-esp-elf-g++.exe`

命令：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/dev/verify-host-cpp-tests.ps1 `
  -CompilerPath E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe `
  -CrossCompilerPath E:/workbuddy/claw4-idf-tools/tools/riscv32-esp-elf/esp-14.2.0_20260121/riscv32-esp-elf/bin/riscv32-esp-elf-g++.exe `
  -OutputDir out/cp0-baseline2
```

结果（`out/cp0-baseline2/host_result.txt`）：

```
unit summary : 20 / 20 PASS
interface: exit=0 (see above; 0 == PASS)
RESULT: NATIVE CPP TEST GATE PASS
```

关键套件：`coordinator_tests` 26/26、`domain_reducer_tests` 28/28、`time_authority_tests` 14 cases 0 failures、`learning_app_glue_tests` 7/7、`presenter_tests` 28/28、`outbox_core_tests` 22/22。

### 4.1 首次运行的瞬时拦截（如实记录，未假通过）

第一次运行（`out/cp0-baseline`）得到 `18 / 20`，两个套件报：

```
LAUNCH FAIL: 使用"0"个参数调用"Start"时发生异常:"应用程序控制策略已阻止此文件。"
unit backend_client_tests : RUN FAIL (exit=-1)
unit learning_backend_session_tests : RUN FAIL (exit=-1)
```

**根因是"链接后立即启动"的瞬时竞态**，不是代码缺陷，也不是被绕过的策略：

- 直接在 shell 重跑这两个 exe：`backend_client_tests: all PASS`（exit 0）、`learning_backend_session_tests: all PASS`（exit 0）
- 随后整轮重跑：20/20 PASS

门禁的失败哨兵在这次事件中**正确工作**（`exit=-1` 而非沿用上一条编译成功退出码）；未修改任何全局脚本，未用旧退出码假通过，未关闭或放宽应用控制策略。

## 5. CP0 交付三：A03 精确文件清单（CP1 白名单）

对照 `V53_A03_RUNTIME_CONCURRENCY_ADR.md` 与实际源码。

### 5.1 当前两个阻塞点的确切位置

| 编号 | 位置 | 事实（源码读取，非硬件测量） |
| --- | --- | --- |
| B1 | `integration/metalio_claw4/device/ports/metalio_http_transport.cpp:40-46` | `CreateMetalioHttpTransport()` 用 `Application::GetInstance().Schedule()` 把真实 `PerformRequest`（DNS/connect/read/close）**投递到主循环**，而 `ScheduledHttpTransport::request()` 在调用方阻塞等回调 → 主循环自己执行阻塞 I/O |
| B2 | `integration/metalio_claw4/device/app/learning_runtime.cpp:78-85` | `RunOnlineCycle()` 在整个 `backend_->runOnlineCycle()` 期间持 `state_mutex_`；`learning_screen.cc` 的 `RefreshUi()`/`OnPrimary()`/`OnSecondary()` 取同一把锁 → UI 与触控被网络等待挡住 |
| B3 | `firmware/main/sync/learning_backend_session.cpp:52-122` | session 持有 `AppCoordinator& app_` 并在 I/O 前后直接读写 coordinator；`authenticate/pullToday/syncOnce` 没有 prepare/apply 分界 |
| B4 | `firmware/main/sync/scheduled_http_transport.cpp:33-36` | `wait_for(timeout+1000ms)` 超时**不取消**已排队回调；回调仍会稍后执行（ADR §1 已记录） |

### 5.2 CP1 计划修改文件

| 文件 | 计划改动 |
| --- | --- |
| `firmware/main/application/coordinator.{h,cpp}` | 拆出 `prepareSync()` / `applySyncResult()`；引入 `generation`；`runSyncOnce()` 保留为薄 wrapper（旧 Host 调用兼容） |
| `firmware/main/sync/learning_backend_session.{h,cpp}` | 去掉跨 I/O 的 coordinator 引用；改为 prepare → I/O → apply；保留 auth_pause / 一次 reauth / backoff / 批次 ACK / 存储失败不丢事件契约 |
| `firmware/main/sync/scheduled_http_transport.cpp` | 停止把 I/O 投递到主循环；请求队列有界；超时后完成回调不得再写状态 |
| `firmware/main/sync/worker_http_transport.{h,cpp}`（新增，纯 C++） | 独占 client、单飞、有界队列的 worker 请求边界；平台任务创建由注入的 spawn 函数提供 |
| `integration/metalio_claw4/device/ports/metalio_http_transport.{h,cpp}` | 用 worker 任务替换 `Application::Schedule`；I/O 在调用方（已是 backend worker）或独立 worker 上执行，绝不进 LVGL 回调 |
| `integration/metalio_claw4/device/app/learning_runtime.{h,cpp}` | `RunOnlineCycle()` 改为"短临界区 prepare → 无锁 I/O → 短临界区 apply + 发布不可变快照"；`BackendDiagnostics()` 读不可变快照而非实时查询 |
| `integration/metalio_claw4/device/learning_screen/learning_screen.cc` | 仅把 state/diagnostics **读取入口**切到快照；不改 voice/UI 外观 |
| `firmware/tests/unit/application/coordinator_tests.cpp`、`firmware/tests/unit/sync/*`、`firmware/tests/fakes/*` | 新增 barrier/latch 确定性并发用例、generation 迟到响应、并发新增 pending + 旧 ACK、401/backoff、存储失败 |

### 5.3 UI 读取入口清单（精确到行，来自源码 grep）

| 位置 | 现状 | CP1 目标 |
| --- | --- | --- |
| `learning_screen.cc:179` `RefreshUi()` | 取 `Rt().stateMutex()` 后读 live `app.state()` | 读不可变快照 |
| `learning_screen.cc:293` | 调 `Rt().BackendDiagnostics()`（内部取锁） | 读不可变诊断快照 |
| `learning_screen.cc:312` `OnPrimary()` | 取锁 + dispatch | 保留本地命令短临界区，不等待网络 |
| `learning_screen.cc:332` `OnSecondary()` | 同上 | 同上 |
| `learning_screen.cc:444` `OnVoiceButton()`、`:551` `OnVoicePhrase()` | 取锁 | 同上（不改语音逻辑） |
| `learning_screen.cc:167,174,297` | 读 `coordinator().lastAcked()/pendingCount()` | 走快照 |

### 5.4 vendor / BSP / sdkconfig 结论

**确认 CP1 不需要修改 `vendor/`、BSP、`sdkconfig`、partition、bootloader。** 依据：

- ADR §1 已给出本地 `E:/c` 依赖的只读事实（`EspNetwork::CreateHttp()` 每次创建独立 `HttpClient`），据此可设计 Wi-Fi 下独占 client 的 worker，无需改上游
- ADR §5 明确"A03 独占 runtime/http transport/backend session"，"不允许新增 vendor 补丁"
- 底层 `EspTcp::Connect()` 不继承 HTTP timeout、`Disconnect()` 可能等待 10s —— 这属于"底层总 deadline"限制，按 ADR §4 在报告中单列，不通过改 managed component 解决

## 6. CP0 验收标准自检

| 验收项 | 结果 | 证据 |
| --- | --- | --- |
| 确认首批三包审查通过及准确 SHA | PASS | §3.3；收口报告 §1 冻结 SHA `e70c283` |
| 运行现有 Host Gate | PASS | §4，20/20 PASS + interface exit=0 |
| 读取 A01 补丁清单 | PASS | §3.4；`project-ca3aa3fa.json` |
| 记录仍待 A05 的硬件边界 | PASS | §3.4 |
| 锁定 A03 接口与 UI 读取入口，给出精确文件列表 | PASS | §5.1～§5.3 |
| 确认无需修改 vendor/BSP/sdkconfig | PASS | §5.4 |
| 交付报告初始段与源基线 | PASS | 本文件 |
| 从指定 SHA 建独立分支/工作区 | PASS（分支名偏差见 §7.1） | 独立克隆 `E:/workbuddy/claw4-wb-v53-next-001`，Base `7b3df6c` |

## 7. 风险、阻塞项与环境缺陷

### 7.1 【严重 / 需 Codex 与用户注意】本机 git 无法创建含 `/` 的引用，并会递归删除父目录

**现象（已最小复现，非推测）：**

```
$ mkdir -p .git/refs/heads/probe && echo <sha> > .git/refs/heads/probe/manual   # 目录与文件创建成功
$ git branch probe/leaf <sha>                                                   # rc=0，无任何报错
$ ls .git/refs/heads/
main                       # probe/ 目录（含无关的 manual 引用）被递归删除
```

- 任何含 `/` 的引用名（`git branch x/y`、`git update-ref refs/heads/x/y`、`git checkout -b x/y`）都**静默 no-op**（rc=0、不建引用、不报错）
- 顶层无斜杠引用名（`git branch zzz`、`git update-ref refs/heads/zzz`）**完全正常**
- 复现于**全新克隆**，说明是机器级缺陷，不是控制仓损坏
- 用绝对路径 `git.exe` + 显式 `GIT_DIR` 复现同样结果，排除 PATH shim 因素
- 该行为会**递归删除父目录**，因此是**数据丢失风险**，不只是不方便

**已造成的损伤与修复：**

- 现象发生前，`refs/heads/codex/{v53-architecture-task-plan, v53-foundation-wave1, v53-a01-build-provenance, v53-a02-reset-protection, v53-b01-time-authority}` 全部消失，4 个 worktree 显示 `0000000`
- 对象库完整（`git cat-file -t 7b3df6c` = commit），**无数据丢失**
- 已通过手工写入 `.git/packed-refs` 恢复全部 5 个 `refs/heads/codex/*` 引用；备份在 `.git/packed-refs.bak-20260914`
- 修复后实测：`for-each-ref refs/heads` 6 个引用齐全、`git worktree list` 4 个 worktree SHA 正确、HEAD 回到 `codex/v53-foundation-wave1` @ `7b3df6c`、工作树 clean、`git fsck --connectivity-only` 仅 1 个无害 dangling blob

**规避措施（本包强制遵守）：** 全程只使用无斜杠的顶层分支名。

**建议 Codex：** 任何后续在本机执行的 `git branch/update-ref/checkout -b` 都要避免含 `/` 的引用名；另外注意被恢复的引用是 **packed-only**，对该分支执行提交时 git 需写 loose 引用，可能再次触发该缺陷并静默不推进引用——**建议 Codex 在本机对 `codex/*` 分支持续提交前先确认该缺陷是否已解除**。

### 7.2 【严重】本机 shell 环境 PATH 缺失 coreutils

默认 PATH 不含 PortableGit 的 `usr/bin`，`ls/head/sed/awk/find/dirname` 全部 not found；同时 `shell-runtime-bash-env.sh:3` 报 `dirname: command not found`。本包通过每次显式 `export PATH=<PortableGit>/usr/bin:...` 规避。属于同批环境退化现象，与 §7.1 可能同源。

### 7.3 【已记录，未解决】底层网络无总 deadline

ADR §1/§4 已确认：`EspTcp::Connect()` 直接 `gethostbyname` + 阻塞 `connect`，不继承 HTTP timeout；`Disconnect()` 可能等待 10s。因此 CP1 只能交付"UI 不堵"的证据，**不能**宣称"底层 DNS/connect/request 有总截止时间"。按任务书 §3 将在报告中拆分这两类证据，未解决不得宣称完整 Device Gate PASS。

### 7.4 阻塞项

- 无当前阻塞项。
- `HARDWARE_VERIFY_REQUIRED`：CP1 的设备侧触控 P95<100ms 与最大停顿、DNS/connect/headers/body/close 耗时边界，均须在 A05 同一候选设备上测量，本包不做。

## 8. 给 Codex 的复检重点（CP0）

1. §3.1 的 `e70c283 → 7b3df6c` 差异是否确实只有 6 个文档/看板文件（可重跑 `git diff --name-status`）。
2. §4.1 的瞬时拦截判定：是否接受"链接后立即启动的应用控制策略竞态、直接重跑全 PASS、门禁哨兵正常工作、未绕过策略"这一结论与证据链。
3. §5.1 对 B1～B4 四个阻塞点的定位是否与 ADR 一致；§5.2 的文件清单是否越出 A03 白名单（特别是我把 `scheduled_http_transport.cpp` 与新增纯 C++ worker DTO 放在 `firmware/main/sync/` 下是否符合 ADR §5）。
4. §5.3 的 UI 读取入口是否只涉及 state/diagnostics 读取、是否误伤 voice/UI 外观。
5. §7.1 的 git 缺陷与已做的 packed-refs 恢复是否会影响 Codex 后续提交——这是我唯一无法自行闭环的风险。

## 9. 下一步

- CP1 剩余部分（CP1b）：`scheduled_http_transport` / `metailo_http_transport` 不再把 I/O 投递到主循环、`LearningRuntime::RunOnlineCycle` 释放跨 I/O 的 `state_mutex_`、学习屏读取切快照。
- 之后按 CP2 → CP3 → CP4 → CP5 顺序推进，每 checkpoint 单独提交、普通 push。
- 建议 Codex 优先确认 §7.1，因为它会影响你后续在自己分支上的提交动作。
- 不启动 SNTP、Light Sleep、NAS、MCP 设备注册、AI 增强；CP5 后停止并交 `REVIEW_READY`。

## 10. CP1a（A03 协调器核心）实施记录

### 10.1 状态

**部分完成。** 本次只交付了 A03 中**可被 Host 门禁确定性验证**的核心：协调器的 prepare / 无锁 I/O / apply 拆分、会话 generation 所有权、ACK 作用域收敛。

**明确未做（CP1b，属设备侧，本机无法 Host 验证）：**

- `integration/metalio_claw4/device/ports/metalio_http_transport.{h,cpp}` 仍在用 `Application::Schedule()` 把真实 `PerformRequest` 投递到主循环（§5.1 B1）
- `integration/metalio_claw4/device/app/learning_runtime.{h,cpp}` 的 `RunOnlineCycle()` 仍在整个网络周期持 `state_mutex_`（§5.1 B2）
- `learning_screen.cc` 的读取入口仍读 live state（§5.3）
- `firmware/main/sync/scheduled_http_transport.cpp` 超时不取消排队回调（§5.1 B4）未改
- 因此**"UI 不堵"的端到端结论尚未成立**，不得据此宣称 A03 完成或 Device Gate PASS

### 10.2 修改文件

| 文件 | 改动 |
| --- | --- |
| `firmware/main/application/coordinator.h` | 新增 `SyncOutcome::StaleResult`；新增 `SyncRequestEnvelope` / `SyncApplyOutcome` DTO；新增 `prepareSync()` / `applySyncResult()` / `generation()` / `beginNewSession()`；新增私有 `scopeBatchToSent()` 与 `generation_` 成员 |
| `firmware/main/application/coordinator.cpp` | 把原 `runSyncOnce` 拆为 `prepareSync`（纯、无 I/O、捕获不可变请求信封）+ `applySyncResult`（短事务、无 I/O、不做 reauth）+ 薄 wrapper；新增 `scopeBatchToSent` 与 `beginNewSession` |
| `firmware/tests/unit/application/coordinator_tests.cpp` | 新增 6 个 CP1 用例（26 → 32 cases） |
| `firmware/tests/host/virtual_device_app.cpp` | `SyncOutcomeName()` 补 `StaleResult` 分支（该 switch 在 `-Werror` 下必须穷尽） |

未新增头文件：DTO 直接放在 `coordinator.h`（`SyncRequestEnvelope` / `SyncApplyOutcome`）。ADR §5 允许把纯 C++ 请求/结果 DTO 放 `firmware/main/sync/`，但放在 `application::` 层可让 DTO 与产出它的类同处一个头，避免多一个只有两个 struct 的头文件；如需拆出请 Codex 指示。

### 10.3 关键契约

| 契约 | 实现方式 |
| --- | --- |
| prepare 不可变请求 | `prepareSync() const` 只从 storage 快照出连续 pending 前缀，捕获 `generation`、`first_sent_sequence`、`max_sent_sequence`；不持引用、不改状态 |
| 无状态锁 I/O | `runSyncOnce` 与未来设备 worker 都在 `prepareSync()` 返回后、`applySyncResult()` 之前做网络 |
| 校验 generation/scope 后应用 | `applySyncResult` 先比 `envelope.generation != generation_` → `StaleResult`，`applied=false`，不写任何状态 |
| 旧会话回应不得写新状态 | `beginNewSession()` 递增 generation；任何旧信封必被拒 |
| ACK 只删实际已发送且被合法确认的连续前缀 | `scopeBatchToSent()` 丢弃 `sequence` 不在 `[first_sent_sequence, max_sent_sequence]` 的结果行，并把 `last_acked_sequence` 上限压到 `max_sent_sequence` |
| 新 pending 保留 | 因 ACK 上界被压到已发送最大序号，`prepareSync()` 之后提交的事件不可能被删除 |
| auth_pause / 一次 reauth / deadletter 失败不丢数据 | 逻辑原样保留；但 reauth 回调**移出** `applySyncResult`，由调用方在锁外执行，`applySyncResult` 只返回 `ReauthOk` 信号 |
| 不引入 recursive_mutex 掩盖跨 I/O 锁 | 本段未引入任何新锁 |
| 旧 Host wrapper 兼容 | `runSyncOnce(SyncTransport&, ReauthFn)` 签名与语义不变，内部改为调用拆分 API |

### 10.4 验收自检（仅 CP1a 可验证部分）

| 验收项 | 结果 | 证据 |
| --- | --- | --- |
| barrier 挂起网络期间本地命令/UI 快照继续 | **未验证**（需 CP1b + 设备侧并发测试） | — |
| 并发新增事件 + 旧 ACK，新 pending 不丢 | PASS（Host） | `a03_old_ack_keeps_events_queued_after_prepare` |
| 越界/重放 ACK 不得删除未发送事件 | PASS（Host） | `a03_out_of_range_ack_cannot_delete_unsent_events` |
| 切换会话后迟到响应被拒 | PASS（Host） | `a03_stale_generation_result_is_rejected`（`ack_calls == 0`） |
| 401 / 一次 reauth / auth_pause 后 transport 零调用 | PASS（Host） | `a03_apply_never_performs_reauth` + 既有 4 个 auth 用例 |
| apply 内部不做网络、不调 reauth | PASS（Host） | `a03_apply_never_performs_reauth` |
| 拆分路径与旧 wrapper 等价 | PASS（Host） | `a03_prepare_apply_matches_wrapper_outcome` |
| 旧 wrapper / wire / Virtual Device 故障矩阵不回归 | PASS | 全量 Gate 20/20 + `interface: exit=0` |
| 存储失败不丢数据 | PASS（既有用例） | `sync_deadletter_storage_failure_blocks_cleanup` 未回归 |
| UI 不堵（端到端） | **未验证** | 见 §10.1 |
| 底层总 deadline（DNS/connect） | **未解决** | 见 §7.3，须在 CP1b/设备候选处理 |

### 10.5 验证命令与结果

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/dev/verify-host-cpp-tests.ps1 `
  -CompilerPath E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe `
  -CrossCompilerPath E:/workbuddy/claw4-idf-tools/tools/riscv32-esp-elf/esp-14.2.0_20260121/riscv32-esp-elf/bin/riscv32-esp-elf-g++.exe `
  -OutputDir out/cp1-coord2
```

```
== WB-STREAM-002 CP3 application coordinator tests ==
cases=32 failures=0
unit coordinator_tests : RUN PASS (exit=0)
...
unit summary : 20 / 20 PASS
interface: exit=0 (see above; 0 == PASS)
RESULT: NATIVE CPP TEST GATE PASS
```

新用例清单：`a03_prepare_is_pure_and_frozen`、`a03_old_ack_keeps_events_queued_after_prepare`、`a03_out_of_range_ack_cannot_delete_unsent_events`、`a03_stale_generation_result_is_rejected`、`a03_apply_never_performs_reauth`、`a03_prepare_apply_matches_wrapper_outcome`。

### 10.6 给 Codex 的复检重点（CP1a）

1. `scopeBatchToSent()` 的两个过滤条件是否会误杀合法结果（尤其服务器把已 ACK 序号重复回报的场景）。我认为丢弃 `sequence < first_sent_sequence` 是安全的，因为那些行早已不在 pending；请确认。
2. `applySyncResult` 不再调用 reauth，只返回 `ReauthOk` 信号 —— 这是为了让设备侧能在**不持状态锁**的前提下刷新凭据。请确认 `runSyncOnce` 的等价性（一次 reauth 预算、失败后 pause、pause 后零 transport 调用），以及这是否符合 ADR "reauth callback 需显式所有权" 的要求。
3. `beginNewSession()` 的调用点：目前**只在测试中调用**。CP1b 必须在 `LearningRuntime::ConfigureBackend()` / 重新配置路径上调用它，否则 generation 机制形同虚设。这是 CP1a 与 CP1b 的接口交接点。
4. `SyncOutcome` 新增枚举值后，除 `virtual_device_app.cpp` 外是否还有别的穷尽 switch（我 grep 过 `SyncOutcome::`，只有那一处 switch）。
5. DTO 放 `coordinator.h` 而非新建 `firmware/main/sync/` 头文件是否可接受（§10.2 已说明理由）。
