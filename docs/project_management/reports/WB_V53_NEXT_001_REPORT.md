# WB-V53-NEXT-001 WorkBuddy 实施报告

> 本文件按 checkpoint 追加。CP0 已提交；CP1～CP5 状态见各节的"状态"行。

## 0. 报告状态总览

| Checkpoint | 内容 | 状态 | 提交 |
| --- | --- | --- | --- |
| CP0 | 领取、基线核对、Host Gate、A03 文件清单 | 完成 | `78c2963` |
| CP1 | A03 网络请求与状态提交分离（CP1a 协调器核心 + CP1b 设备侧接线） | 完成 | `7624546`、`ed046b8` + CP1b 提交 |
| CP2 | A04 服务端快照不覆盖离线终态 | 完成 | 见 §12 |
| CP3 | B02 音频与提醒仲裁 Host | 完成 | 见 §13 |
| CP4 | B03 Reminder Host 与持久化恢复 | 完成 | 见 §14 |
| CP5 | 主机联合 Gate 与审查交付 | 完成 | 见 §15 |

**全包状态：`REVIEW_READY`**（详见 §15；设备侧并发收益与触控 P95 仍需 A05 真机测量）

## 1. 元信息

- 任务 ID：WB-V53-NEXT-001
- 结果：**`REVIEW_READY`**（CP0～CP5 全部提交并推送，等待 Codex 按不可变 SHA 复审）
- 分支：
  - 本地：`workbuddy-v53-next-001-reliability`
  - 远端：`workbuddy/v53-next-001-reliability`（任务书要求的名称，push 时用显式 URL 直接建远端引用）
  - **偏差说明**：任务书字面要求本地也用 `workbuddy/...`。本机 git 无法创建含 `/` 的引用（详见 §7.1），故本地改用等价的无斜杠分支名。这是环境强制，不是范围变更。远端分支名与任务书完全一致。
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

## 9. 下一步（仅建议，不自行启动）

- 下一候选建议：按看板 `A05-BUILD`（M0 候选冻结 + 已有 D5 回归），把本包新增的 `sync_executor` / `single_flight_http_transport` / `interaction_arbiter` / `reminder` 纳入镜像清单后，在同一候选上做设备侧测量。
- `A05-BUILD` 之前需要 Codex 决策的三件事：① `ScheduledHttpTransport` 现在已不被设备使用，是否删除或保留为 host fixture（我保留并标注，见 §11.5）；② `firmware/main/reminder` 是否要登记进镜像 CMake 清单；③ 既有 CMake 补丁尚未登记 `time` 模块，本包新增模块同样未登记。
- 建议 Codex 优先确认 §7.1，因为它会影响你后续在自己分支上的提交动作。
- 不启动 SNTP、Light Sleep、NAS、MCP 设备注册、AI 增强。

## 11. CP1b（A03 设备侧接线）

### 11.1 新增生产代码

| 文件 | 作用 |
| --- | --- |
| `firmware/main/sync/sync_executor.{h,cpp}`（新） | **A03 的核心生产路径**：一次同步 = ① 加锁 `prepareSync()` ② **完全不加锁** `transport.send()` ③ 加锁 `applySyncResult()`。reauth 回调也在锁外调用；`applySyncResult` 内部不做任何 I/O。锁以两个 callable 注入，因此设备与 Host 测试跑**同一份代码** |
| `firmware/main/sync/single_flight_http_transport.{h,cpp}`（新） | 取代"把 I/O 投到主循环再阻塞等待"的旧边界。在**调用方任务**上执行厂商 HTTP，并用单飞门禁保证同一 `HttpClient` 不会被两个任务并发使用/关闭；并发或重入调用立刻返回空响应（上游映射为 `SyncErrorClass::Network` → 有界 Backoff），不排队、不无界增长 |

### 11.2 设备侧修改

| 文件 | 改动 |
| --- | --- |
| `integration/.../device/ports/metalio_http_transport.{h,cpp}` | 删除 `Application::Schedule()` 投递；改为 `SingleFlightHttpTransport(PerformRequest)`，I/O 留在调用方（backend worker）任务上，绝不进 LVGL 回调 |
| `firmware/main/sync/learning_backend_session.{h,cpp}` | 新增 lock 注入版 `runOnlineCycle(lock, unlock)`：`authenticate` / `fetchToday` / 批量同步 / 唯一的凭据重试全部**不加锁**；只有 `applyTodaySnapshot` 与 ACK/backoff 应用进短临界区；计数器读取也走短临界区（它要读存储）。旧的单线程 `runOnlineCycle()` 保留，既有 Host 测试不变 |
| `integration/.../device/app/learning_runtime.{h,cpp}` | `RunOnlineCycle()` 改为三阶段：短锁取 session 指针 → **无锁**跑整个网络周期 → 短锁发布**不可变诊断快照**。`backend_` 改 `shared_ptr`，worker 在整个 I/O 期间持有对象，即使 `ConfigureBackend()` 并发替换也不会悬空。`BackendDiagnostics()` 现在只返回已发布的快照（无实时查询、无网络）。`ConfigureBackend()` 调用 `beginNewSession()` 递增 generation（§10.6 第 3 点的交接点已闭合） |
| `integration/.../device/learning_screen/learning_screen.cc` | **未修改**。`RefreshUi()` 的 diagnostics 读取入口已经是 `Rt().BackendDiagnostics()`，它现在返回快照，所以 A03 要求的"UI 读快照"由运行时侧闭合，避免改动 UI/语音逻辑 |

### 11.3 验收自检（CP1）

| 验收项 | 结果 | 证据 |
| --- | --- | --- |
| barrier 挂起网络期间本地命令/UI 快照继续 | **PASS（Host，确定性 latch）** | `sync_executor_tests::cycle_holds_no_state_lock_during_network`：worker 卡在 `send()` 里时，主线程 `try_lock` 状态锁成功，并成功提交一条本地 Pause 命令（pending 2→3），全程无 sleep |
| 并发新增事件 + 旧 ACK，新 pending 不丢 | PASS | `coordinator_tests::a03_old_ack_keeps_events_queued_after_prepare` |
| 越界/重放 ACK 不得删除未发送事件 | PASS | `a03_out_of_range_ack_cannot_delete_unsent_events` |
| 切换会话后迟到响应被拒 | PASS | `a03_stale_generation_result_is_rejected`、`sync_executor_tests::cycle_rejects_stale_generation_without_writing` |
| 401 / 一次 reauth / auth_pause 后 transport 零调用 | PASS | `a03_apply_never_performs_reauth`、`cycle_auth_pause_never_calls_transport`、`cycle_reauth_fail_pauses` |
| 网络超时/重复回调/响应丢失/无效 ACK/存储失败幂等可恢复 | PASS | 既有 12 个 sync 用例 + `a03_*` + `joint_restart_convergence_is_idempotent` |
| 旧 wrapper / wire / Virtual Device 故障矩阵不回归 | PASS | 全量 Gate 25/25 + `interface: exit=0` |
| 单飞/有界（不双写 client） | PASS | `single_flight_http_transport_tests`（6 cases，含并发与重入拒绝） |
| 设备触控 P95<100ms 与最大停顿 | `HARDWARE_VERIFY_REQUIRED` | 见 §15.3 |
| 底层 DNS/connect 总 deadline | **未解决（已明示）** | 见 §7.3；`EspTcp::Connect()` 不继承 HTTP timeout，本包不通过改 managed component 解决 |

### 11.4 验证

```
== WB-V53-NEXT-001 CP1 sync executor (A03) tests ==   cases=7  failures=0
== WB-V53-NEXT-001 CP1 single-flight transport (A03) tests ==  cases=6 failures=0
```

### 11.5 已知限制与偏差

1. `firmware/main/sync/scheduled_http_transport.{h,cpp}` **保留未删**，但设备已不再使用它（`CreateMetalioHttpTransport` 已切到单飞传输）。它的 `wait_for` 超时不取消排队回调这一已知问题因此不再影响设备路径。删除它会同时移除一个既有 Host 套件并改变门禁基线，故留给 Codex 决策（见 §9）。
2. 本包新增的 `firmware/main/reminder` 需要在 `tools/dev/verify-host-cpp-tests.ps1` 的 `implRoots` 登记（已加，见 §14.3）；它**尚未**登记进镜像 CMake 清单。
3. `learning_screen.cc` 未改：A03 通过运行时快照闭合，UI 锁持有时间被压到微秒级，但**没有**把 `app.state()` 引用式读取改成值语义快照。若 Codex 要求更强的"UI 零锁"，需要另开白名单。

## 12. CP2（A04）：服务端快照不覆盖离线执行终态

### 12.1 先写回归（任务书要求）

新增 5 个用例，先在未修复的实现上确认 `a04_offline_complete_not_revived_by_stale_snapshot` 会失败（现状是服务端 Ready 会把本地 Completed 复活），再实施修复。

### 12.2 最小修复

`firmware/main/application/coordinator.cpp::applyTodaySnapshot`：先从 `os.pending` 收集**终态事件尚未被 ACK** 的 task_id（`TaskCompleted` / `TaskSkipped`，payload 里的 `task_id`）。对这类任务，合并服务端行时**只取更新的描述性字段，永不接受服务端 status**——与既有的"活跃会话任务保护"同构。

### 12.3 验收自检

| 验收项 | 结果 | 证据 |
| --- | --- | --- |
| 待 ACK 的离线 Complete 不被旧 Ready 快照复活 | PASS | `a04_offline_complete_not_revived_by_stale_snapshot`（含更高 version 的旧快照） |
| 待 ACK 的离线 Skipped 不被复活 | PASS | `a04_offline_skip_not_revived_by_stale_snapshot` |
| 终态被 ACK 后保护解除（不能把状态焊死） | PASS | `a04_terminal_applies_again_after_ack`：同步后 version 9 的 Ready 修订可正常重开任务 |
| 空权威快照 ≠ 请求失败 | PASS | `a04_empty_snapshot_differs_from_request_failure` |
| active 改期/删除不丢会话 | PASS | `a04_active_reschedule_and_delete_keep_session` |
| 旧响应/401/重复 ACK/重启幂等 | PASS | 既有 4 个 auth 用例 + `lost_response_then_duplicate_converges` + `joint_restart_convergence_is_idempotent` |
| 仅调换 GET/POST 顺序 | 未采用 | 修复是状态合并规则，与请求顺序无关 |

### 12.4 有意保留的边界（请 Codex 确认）

当终态未 ACK 且服务端**已不再列出**该任务时，仍按既有语义**移除**该缓存行（既有用例 `snapshot_cleans_completed_after_session_ends` 明确要求如此）。移除不等于复活，但如果你认为"未 ACK 的终态也应当保留行"，这是行为变更，需要你放行后再改，我没有自行扩大范围。

### 12.5 验证

```
== WB-STREAM-002 CP3 application coordinator tests ==  cases=37 failures=0
```

## 13. CP3（B02）：音频与提醒仲裁 Host

### 13.1 交付

`firmware/main/interaction/interaction_arbiter.{h,cpp}`（新，纯 C++17 状态机）+ `firmware/tests/unit/interaction/interaction_arbiter_tests.cpp`。不接官方 `AudioService`、不播放真实音频、无 I/O。`ports/voice_session_port.h` 未改动（无需扩展）。

### 13.2 验收自检

| 验收项 | 结果 | 证据 |
| --- | --- | --- |
| 系统 Critical 优先且可恢复 | PASS | `critical_preempts_and_is_recoverable`（单播占用 + `normalSourcePreempted()` 提示可恢复） |
| 普通 TTS 等待本地提醒最多 3 秒预算 | PASS | `local_reminder_budget_then_preempts`：预算内不抢占；超预算本地铃**抢占**网络播报 |
| 预算内 TTS 结束时提醒立即接上 | PASS | `deferred_reminder_starts_when_tts_finishes_in_budget` / `..._late` |
| 不双播 | PASS | `never_two_sources_at_once`（单占用槽；第二条 TTS 与主动 AI 都只能 Deferred） |
| 迟到 session 消息拒绝 | PASS | `late_message_from_closed_session_is_denied`、`session_reset_clears_deferred_reminder` |
| 用户明确意图 + scope 匹配才允许 Snooze/ACK/Dismiss | PASS | `reminder_action_requires_intent_and_scope`（含 child/task/instance/generation 四类 scope 不匹配） |
| ACK 不是 Complete | PASS | `ack_is_not_completion`：ACK 获准后 Complete 仍因缺物理确认被拒 |
| 主动 AI 不能伪造授权 | PASS | `proactive_ai_cannot_authorize_anything`（即使伪造 `explicit_user_intent=true` 也拒绝） |

### 13.3 验证

```
== WB-V53-NEXT-001 CP3 interaction arbiter (B02) tests ==  cases=10 failures=0
```

### 13.4 边界

设备侧接线（把裁决接到真实播放器/Overlay）属 C03；本包按任务书只交付纯 Host 仲裁逻辑。

## 14. CP4（B03）：Reminder Host 与持久化恢复

### 14.1 交付

| 文件 | 作用 |
| --- | --- |
| `firmware/main/reminder/reminder_core.{h,cpp}`（新） | 提醒实例表 + 到期评估 + 持久化恢复。**复用 B01 `TimeAuthority`**（内部取 `status()`），不新建第二套时钟 |
| `firmware/main/ports/reminder_wake_port.h`（新） | 平台无关唤醒/滴答端口（`monotonicMs` / `scheduleWakeAt` / `cancelWake`），不引用 `esp_sleep` / LVGL |
| `firmware/tests/unit/reminder/reminder_core_tests.cpp`（新） | 10 个用例 |
| `tools/dev/verify-host-cpp-tests.ps1` | `implRoots` 登记 `firmware\main\reminder`（任务书允许的"验证脚本注册"） |

### 14.2 验收自检

| 验收项 | 结果 | 证据 |
| --- | --- | --- |
| TaskDue / Snooze / FocusEnd / DailyReview | PASS | `all_four_kinds_supported` |
| 稳定实例 ID | PASS | `stable_instance_id_dedupes_on_rebuild`（同一逻辑提醒重复 upsert 不产生第二实例，rebuild 后仍 1 个） |
| rebuild 不重置 Snooze | PASS | `rebuild_preserves_snooze` |
| 完成/改期/删除取消旧提醒 | PASS | `cancel_on_complete_reschedule_delete`（含整计划替换 `cancelForChild`） |
| 跨日/静默期/多提醒合并/上限 | PASS | `quiet_period_suppresses_but_keeps_pending`、`delivery_is_capped_and_marked_once`（按到期时间排序 + 每 tick 上限） |
| 时间失信/回拨/前跳不重响历史 | PASS | `untrusted_time_blocks_calendar_reminders`（未同步 / 超出 holdover 均不放行）、`forward_jump_does_not_replay_history`（超出 grace 直接退休，不补响） |
| 保存失败与呈现前后掉电的有界恢复 | PASS（有界） | `save_failure_rolls_back`（失败时内存不前进、不半应用）；未 `markDelivered` 的实例重启后按 grace 内重发一次，已 `markDelivered` 的**绝不重发** → 语义是 **at-most-once**，不承诺物理响铃严格恰好一次（与任务书一致） |
| 容量不被提醒遥测挤占 | PASS | `capacity_bounded_and_separate_from_outbox`：实例表有独立 `max_instances` 上限，超限拒绝并计数，不触碰 learning outbox 预算 |

### 14.3 验证

```
== WB-V53-NEXT-001 CP4 reminder core (B03) tests ==  cases=10 failures=0
```

### 14.4 边界

物理响铃的严格一次语义、真实 NVS 适配器、断电容错（真掉电）留 C03/A05。`reminder_wake_port.h` 目前只有接口与 Host fake，设备实现未写。

## 15. CP5：主机联合 Gate 与审查交付

### 15.1 联合 Gate

新增 `firmware/tests/unit/v53/joint_loop_tests.cpp`，把 CP1–CP4 串成一条**真实生产 C++** 链路（只 mock 平台边界）：

| Mock | 替换的真实边界 |
| --- | --- |
| `FakeOutboxStorage` | `sync::OutboxStorage`（NVS 适配器为设备专用） |
| `ScenarioTransport` | `application::SyncTransport`（真实 HTTP 在设备侧） |
| `FakeClockPort` | `ports::ClockPort`（esp_timer 适配器为设备专用） |
| `ScenarioReminderStore` | `reminder::ReminderStore`（NVS 适配器为设备专用） |

被真实执行的生产代码：`DomainReducer`、`AppCoordinator`（prepare/apply/applyTodaySnapshot）、`SyncExecutor`、`OutboxCore`、`TimeAuthority`、`InteractionArbiter`、`ReminderCore`。

场景：`Startup → 离线 Start/Complete → 旧 Ready 快照先到 → prepare/无锁 I/O/apply 同步 ACK → 提醒状态跨 rebuild 保留 + 静默/失信门禁 → 仲裁（本地铃超预算抢占、ACK≠Complete、主动 AI 被拒）`；第二个用例覆盖 `重启 + 重复响应幂等收敛`。

**不使用真实家庭后端、不使用 loopback socket、不使用真实数据、不接设备。**

### 15.2 全量 Host Gate（CP5 最终证据）

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/dev/verify-host-cpp-tests.ps1 `
  -CompilerPath E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe `
  -CrossCompilerPath E:/workbuddy/claw4-idf-tools/tools/riscv32-esp-elf/esp-14.2.0_20260121/riscv32-esp-elf/bin/riscv32-esp-elf-g++.exe `
  -OutputDir out/cp5-final2
```

结果：**25 / 25 suites PASS，`interface: exit=0`，`RESULT: NATIVE CPP TEST GATE PASS`**（门禁从 20 套件增至 25 套件：新增 sync_executor 7、single_flight 6、interaction_arbiter 10、reminder_core 10、joint_loop 2；coordinator 26→37）。

个别运行会偶发 `LAUNCH FAIL: 应用程序控制策略已阻止此文件`（见 §4.1）。最终一次运行时 `sync_diagnostics_tests` 被瞬时拦截，**直接原地重跑该 exe 为 `all PASS`（exit 0）**，其余 24 套件当次即 PASS；随后整轮重跑取到 25/25。未修改门禁来掩盖它，也未绕策略。

### 15.3 已知风险与 `HARDWARE_VERIFY_REQUIRED`

| 项 | 说明 |
| --- | --- |
| `HARDWARE_VERIFY_REQUIRED` | ① 断网/挂起网络下的触控 P95<100ms 与最大停顿；② DNS/connect/headers/body/close 分段耗时与资源边界；③ 20 轮语音窗口无 watchdog/heap 损坏；④ 真实 NVS 上的 snooze/delivered 恢复与掉电边界 |
| 底层总 deadline | `EspTcp::Connect()` 不继承 HTTP timeout、`Disconnect()` 可能等 10s。CP1 只交付"UI 不堵"，**未**解决底层总 deadline（§7.3） |
| 镜像清单 | 本包新增 4 个模块（sync_executor、single_flight_http_transport、interaction_arbiter、reminder）**尚未登记进镜像 CMake 清单**，不能宣称固件已包含本包功能；`time` 模块的既有缺口仍在 |
| 真实 NVS/Flash | 全部未触碰 |
| 环境缺陷 | §7.1 的 git 嵌套引用缺陷仍未解决，会影响后续在本机做的引用写操作 |

### 15.4 Codex 复检重点（全包汇总）

1. §7.1 git 缺陷（影响你自己的提交动作）——最高优先。
2. §10.6 的 CP1a 复检点，特别是 `scopeBatchToSent` 的误杀风险与 reauth 移出 apply 的等价性。
3. §11.5-1：`ScheduledHttpTransport` 是删是留。
4. §12.4：终态未 ACK 时服务端不再列出该任务 → 是否仍应移除缓存行。
5. §14.2 "有界恢复"的 at-most-once 语义是否满足 B03 验收（我按任务书"不能承诺物理铃声严格恰好一次"实现）。
6. 新增模块未进镜像清单（§15.3）——请给出登记方式或指示延后。

### 15.5 复核用提交链

见 §16。

## 10. CP1a（A03 协调器核心）实施记录

### 10.1 状态

**已完成（CP1a 协调器核心 + CP1b 设备侧接线，见 §11）。** 本节记录 CP1a；CP1b 记录在 §11。

CP1a 交付 A03 中可被 Host 门禁确定性验证的核心：协调器的 prepare / 无锁 I/O / apply 拆分、会话 generation 所有权、ACK 作用域收敛。

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
