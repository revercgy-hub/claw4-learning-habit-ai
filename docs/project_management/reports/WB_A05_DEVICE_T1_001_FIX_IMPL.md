# A05-DEVICE-T1-001-FIX 实施报告：ACK 队列永久阻塞（ACK_PIPELINE_FAIL）修复

**性质**：源码修复实施（Taskbook 授权范围外的一次**已授权扩展**：用户明确下达「继续修」）
**基线**：`b10dfde`（T1 报告 + 修复方案 + 主机复现；本地未推送）→ 本次修复在此基础上新增提交
**Candidate 影响**：**本仓库源码已变 ⇒ 现有冻结 Candidate `claw4-v53-m0-a05-2c8f58f` 不再对应本仓库 HEAD**；
按 §31 需走「冷构建 → 尺寸硬门禁 → 新 Candidate → 刷机 → 重跑 A05-DEVICE」才会产生可刷的固件。
**本次未做**：未 rebuild、未 reflash、未改 sdkconfig/partition/CMake、未改设备 NVS、未推送远端。

---

## §1 改动清单

| # | 文件 | 改动 | 为什么 |
|---|---|---|---|
| 1 | `firmware/main/sync/outbox_storage.h` | 新增 `virtual CommitStatus rebaseSequences(int64_t new_base)`；**默认实现返回 `StorageError`（fail-closed）** | 提供"把本地序号空间重基到服务端基线"的**单次原子写**原语。之所以必须放在存储层：`removeAcked()` 是低水位删除，核心层只能用「追加 + 低水位删」两个操作，做不到单 commit 的无损重编号 |
| 2 | `firmware/main/sync/event_sink.h` | `PersistResult` 追加 `int acked_removed` / `bool ack_advanced`（附加字段，非破坏性） | 让上层能区分「已清理」与「提交了但队列没动」——这正是静默假成功的根源 |
| 3 | `firmware/main/sync/outbox_core.h/.cpp` | 新增 `rebaseToServerBaseline()`；`applyBatchResult()` 落盘时如实填 `acked_removed` / `ack_advanced`；`rit == rend()`（无需清理）分支同样如实返回 0/false | 把"实际发生了什么"变成可观测事实 |
| 4 | `firmware/main/application/coordinator.h` | 新增 `SyncOutcome::Blocked`（**追加在末尾**，不改动既有枚举数值）；`SyncApplyOutcome` 追加 `bool rebased`；`CoordinatorOptions` 新增 `bool allow_baseline_rebase = true` | 诚实上报 + 让重基行为显式可关（便于评审/AB 对比） |
| 5 | `firmware/main/application/coordinator.cpp` | ① `applyBatchResult()` **提前到无条件执行**（保住 FIX-V4-03 的 dead-letter 语义）；② 新增 `responseCoversAllSent()` 判据；③ 新增"队首永久阻塞 + 服务端基线领先 ⇒ 无损重基"分支；④ 阻止"提交了但队列没动"被报成 `Synced` | 修复本体 |
| 6 | `firmware/tests/fakes/fake_outbox_storage.h` | 实现 `rebaseSequences()`（单 commit 重编号，保持 event_id/payload/type/timestamp 与相对顺序）+ `FakeDisk::rebase_calls` 计数 | 让修复可在 Host 门禁里被真实验证（含失败注入路径） |
| 7 | `integration/metalio_claw4/device/ports/nvs_outbox_storage.h/.cpp` | 实现 `rebaseSequences()`：`load → 重编号 → 单次 Save()`（= 一次 `nvs_set_blob` + 一次 `nvs_commit`） | 设备侧真正生效的那一份。单次 commit ⇒ 掉电只会留下"旧快照"或"已重基快照"，**行不会丢** |
| 8 | `firmware/tests/host/virtual_device_app.cpp` | 枚举 switch 增加 `case SyncOutcome::Blocked: return "blocked";` | 该 switch 无 `default:`，新增枚举值在 `-Werror` 下会编译失败（必须补） |
| 9 | `firmware/tests/unit/application/coordinator_tests.cpp` | **2 处既有期望更新**（详见 §2） | 期望值编码的正是"静默假成功"这一缺陷行为 |
| 10 | `firmware/tests/unit/application/ack_queue_recovery_tests.cpp` | **新增测试套件（6 用例）** | 把缺陷固化成回归测试，含无损性、fail-closed、RF2 保护 |

## §2 行为变更（评审要点）

### 2.1 新增 `SyncOutcome::Blocked`
语义：**交换完成了，但队列没动**——要么是完整且 id 匹配的回答确认不了任何一行，要么是解锁它的重基不可用。
- 追加在枚举末尾 ⇒ **既有枚举数值不变**（`contract_tests.cpp` 的 `static_assert` 之类不受影响）
- `LearningBackendSession::runOnlineCycle()` 只把 `Synced`/`NoPending` 视为成功 ⇒ `Blocked` 会返回 `false`。
  **已确认这是安全的**：设备侧 worker 循环（`learning_runtime.cpp:137-145`）**忽略**该返回值、每 15 s 无条件重试，
  所以不会因为 `Blocked` 而停止同步。
- 诊断槽在 `Blocked` 时置 `sync_failed = true`（`sync_recovered = false`）⇒ UI 不再显示"已同步"。

### 2.2 两处既有测试期望更新（安全断言一字未改）

| 用例 | 原期望 | 新期望 | 理由 |
|---|---|---|---|
| `sync_conflict_keeps_pending_no_cleanup` | `Synced` | **`Blocked`** | 该用例构造的正是缺陷签名（全 conflict、无移除、ACK 不动）。`pendingCount()==2` / `lastAcked()==0` 两条安全断言**保持原样** |
| `sync_business_4xx_deadletter_keeps_pending` | `Synced` | **`Blocked`** | 同上（全 422 业务拒绝、dead-letter 已打标、队列不动）。`dead_letter_reason` 与 `pendingCount()==2` 断言**保持原样** |
| `sync_deadletter_storage_failure_blocks_cleanup` | 末次 `Synced` | **`Blocked`** | 首次 `Backoff`（FIX-V4-03）**保持不变**；恢复后的那次同样是"已打标但队列不动" |

**关键设计取舍（为避免过度改动）**：`Blocked` 分支的成立条件是
```
acked_removed == 0 && !ack_advanced && complete && pendingCount() > 0
```
- `complete`（每个发出的行都拿到了 id 匹配的回答）：**部分/伪造/缺行的回答一律不触发**，RF2 行为原样保留
  （`a03_ack_stops_on_wrong_event_id`、`a03_ack_stops_on_missing_result`、`a03_ack_stops_before_conflict_gap` 全部不动）
- `pendingCount() > 0`：**"队列已清空"不算阻塞**——那是同一响应被幂等重放的情形，仍报 `Synced`
  （`a03_duplicate_response_is_idempotent` 因此不受影响）

### 2.3 执行顺序修正（重要）
`applyBatchResult()` 在阻塞判定**之前**无条件执行。原因：它的第一步是**业务 4xx 的 dead-letter 落盘**，
若把它挪到阻塞判定之后，`sync_deadletter_storage_failure_blocks_cleanup` 依赖的 FIX-V4-03 语义
（dead-letter 写失败 ⇒ 整批中止 ⇒ `Backoff`）就会被绕过。**这是首版实现踩到的真实回归，已在本次修正。**

### 2.4 未改动的既有保证（逐条确认）
- RF2/RF3 的连续前缀 + id 匹配 + Accepted/Duplicate 三重校验：**未动**
- 不因服务端 `last_acked_sequence` 单独删除本地行：**未动**（重基前仍走同一套 scope 判定）
- `FIX-V4-03`（dead-letter 落盘失败 ⇒ 整批中止）：**未动**
- 401/403 不计入 dead-letter、网络/5xx 走确定性退避：**未动**

## §3 新增测试（`ack_queue_recovery_tests.cpp`，6 用例）

| 用例 | 钉住什么 |
|---|---|
| `blocked_queue_is_not_reported_as_synced` | 缺陷签名 ⇒ 必须 `Blocked`，`removeAcked` **零调用**、`pendingCount` 不变、ACK 不动、诊断槽置 failed |
| `desync_is_rebased_losslessly_then_drains` | 服务端基线领先 ⇒ **重基 1 次**；`last_acked` 采用基线（7）；4 行被重编号为 8..11；**event_id 集合相等（无损）**；`type/version/timestamp/timestamp_source/payload` 逐字段相等；`next_sequence == 12`；下一轮 `Synced` 且队列清空、ACK 到 11 |
| `rebase_disabled_keeps_old_behaviour` | `allow_baseline_rebase=false` ⇒ 不重基、`rebase_calls==0`、序号仍 1..4、ACK 仍 0（保留旧行为供 AB 对比） |
| `rebase_failure_is_fail_closed` | 注入写失败 ⇒ `Blocked`、`!rebased`、**状态零变化**（原子性） |
| `partial_answer_never_triggers_rebase` | 部分回答 / 首行伪造 id ⇒ `rebase_calls==0`、ACK 与行数不变（RF2 保护） |
| `success_still_works` | 正常批次仍 `Synced`、清空、ACK=4、`rebase_calls==0`（无回归） |

## §4 原子性与 fail-closed 论证

- **原子性**：重编号与基线采用发生在**同一次** `Save()` 里（NVS：一次 `nvs_set_blob` + 一次 `nvs_commit`；
  fake：一次内存写入）。掉电后要么是旧快照、要么是完整重基后的快照，**不存在"行已被删但副本未写"的窗口**。
  这是把原语放在存储层、而不是在核心层用「追加 + 低水位删」两个 commit 拼出来的根本原因。
- **fail-closed**：接口默认实现返回 `StorageError`；任何未实现该原语的存储 ⇒ 协调器回落到 `Blocked`，
  **绝不假装成功**，也绝不删任何行。

## §5 ⚠️ 未被 Host 门禁覆盖的部分（诚实披露）

| 部分 | 覆盖情况 |
|---|---|
| `firmware/main/sync/**`、`firmware/main/application/**` | ✅ 门禁编译 + 运行（含新增 6 用例） |
| `firmware/tests/fakes/fake_outbox_storage.h` | ✅ 门禁使用 |
| **`integration/metalio_claw4/device/ports/nvs_outbox_storage.cpp`** | ⚠️ **设备侧 TU，不在 Host Gate 的编译集内**（与 `learning_runtime.cpp` 同类）。本次只做了人工对照 `removeAcked`/`Save` 的写法与语义，**尚未被任何门禁编译或执行**；其正确性只能由**冷构建 + 真机复测**覆盖。**报告不得据此声称设备侧已验证。** |

## §6 方案 D（启动耦合）**本次未实施**及原因

`LearningRuntime::Init()`（内含每 15 s 同步 worker）全树唯一调用点是学习页
`integration/metalio_claw4/device/learning_screen/learning_screen.cc:607`，导致复位后同步不自愈。

想把调用点补到应用启动路径，需要改 **`application.cc`**——而 `application.cc`
**不在本仓库内**（本仓库只含 `integration/metalio_claw4/{device,host_glue,patches,candidates}`；
`application.cc` 属 pinned 上游源码树）。修改它等于**修改 A05-BUILD 的构建输入**（需要新的输入清单冻结 +
新的 `host-reuse-fingerprint` 基线），属 A05-BUILD 级别的动作。

⇒ **建议**：作为**独立小任务**处理（或与本修复一起进同一次冷构建，由 Codex 在任务书里明确 patch 层改动范围）。
本次不动，避免把"修复管线缺陷"和"改上游补丁层"混成一件事。

## §7 Host 门禁结果（实测，全绿）

```
工具 : tools/dev/verify-host-cpp-tests.ps1 -OutputDir out\a05-device-t1-host4
       -CrossCompilerPath <riscv32-esp-elf\...\riscv32-esp-elf-g++.exe>
       （PATH 必须同时含 w64devkit 与 riscv32-esp-elf；只放 w64devkit 会让第 5 步交叉语法门禁 FAIL）

unit summary : 30 / 30 PASS
interface    : exit=0   (PASS，头文件 49/49 交叉语法)
RESULT       : NATIVE CPP TEST GATE PASS
gate_exit    = 0
```

关键套件：

| 套件 | 结果 |
|---|---|
| `ack_queue_recovery_tests`（本次新增，6 用例） | **RUN PASS** |
| `coordinator_tests`（含 §2.2 的 3 处期望更新） | **RUN PASS** |
| `outbox_core_tests` / `learning_backend_session_tests` / `final_concurrency_cleanup_tests` / `production_path_gate_tests` / `sync_executor_tests` / `wire_codec_tests` | **RUN PASS** |

### 7.1 迭代记录（门禁确实拦住了我，这是证据）

**首次运行**（`out/a05-device-t1-host`）`coordinator_tests` **FAIL**，4 条失败：

| 失败用例 | 暴露的问题 | 处理 |
|---|---|---|
| `sync_deadletter_storage_failure_blocks_cleanup`（期望 `Backoff`） | 我把阻塞判定**放在了 `applyBatchResult()` 之前**，绕过了 FIX-V4-03 的 dead-letter 落盘失败语义 | **调换执行顺序**（§2.3） |
| `a03_ack_stops_on_wrong_event_id`（期望 `Synced`） | 阻塞谓词**过宽**，把"伪造/不完整回答"也判成阻塞 | 加 `complete`（逐行 id 匹配）门 |
| `a03_duplicate_response_is_idempotent`（期望 `Synced`） | 谓词把"队列已清空的幂等重放"误判成阻塞 | 加 `pendingCount() > 0` 门 |
| `sync_business_4xx_deadletter_keeps_pending`（期望 `Synced`） | 这条**确实是缺陷行为**，期望值本身要改 | 更新期望为 `Blocked`（安全断言不变） |

⇒ 前 3 条是**我自己的实现缺陷**，第 4 条是**测试固化了错误行为**。两条都在修完后重跑全绿。

### 7.2 一次与代码无关的环境性 FAIL（留档）

`dispatcher_tests` 曾报 `应用程序控制策略已阻止此文件`（应用控制策略瞬时拦截新链出的 exe，rc=126 类），
等待后重跑即 PASS。**不是代码问题**，但重跑门禁时若见到单条此类失败，先排除策略拦截再怀疑代码。

## §8 业界先例检索结论（见 `evidence/a05-device/T1-PRIOR-ART-RESEARCH.md`）

- 本缺陷 = 经典 **poison message 在有序 at-least-once 队列上的 head-of-line blocking**；
  我们的「低水位 ACK」与 **Kafka committed offset** 语义完全同构（offset 不能跳）。
- 业界三机制：① 有界尝试 + 移入 DLQ 后提交 offset（Kafka `DeadLetterPublishingRecoverer` + `setCommitRecovered`；
  SQS FIFO `maxReceiveCount` 3–5）；② 显式 seek 跳过（KIP-334）；③ **服务端基线失效 ⇒ 客户端全量 resync
  （RFC 6578 `DAV:valid-sync-token`）**。
- **方案 B 与机制 ③ 等价**，且是协议明文承认的正常情形 ⇒ 修复方向站得住。
  方案 B 在**顺序语义上优于机制 ①**（按原相对顺序重编号，不改变事件先后；而 SQS 文档明确承认 DLQ 会打破顺序）。
- 🚩 **检索指出的剩余缺口**：方案 B 只在「服务端 ack 领先本地」时触发；对**业务非法事件**（服务端稳定 422、
  ack 永不动）这一类，修复后**不再静默但也不会自愈**。业界对这类只能靠机制 ①（有界尝试 + 移出/隔离）。
  这条与现有「dead-letter 行保留以便 replay」的设计冲突，属**设计变更**，**需 Codex 拍板**，本次未改。

## §9 后续步骤（需 Codex 授权后执行）

1. **冷构建**（`tools/dev/run-a05-cp2-build.ps1`，隔离树短路径 `E:\a05c\s` / `E:\a05c\b`，需在普通宿主终端跑）
2. ⚠️ **固件尺寸硬门禁**：`ota_0` 余量仅 **165,424 B / 1.75%**，必须重算 app 尺寸 vs `ota_0`
3. 新 Candidate 冻结（CP4 流程：Candidate Manifest + Freeze Report + 5 件产物 sha256）
4. 刷机（A05-DEVICE D0 只读预检 → D1 → D2）
5. **重跑 T1**：建议用经 Codex 同意的**干净基线库**；判据仍是 §7 三层证据
6. 若采纳方案 D：另开任务，明确上游 patch 层改动范围
7. 🚩 **待 Codex 决策**：是否补上 §8 指出的最终安全阀（有界尝试 + 隔离区 / 业务非法事件不再永久阻断队列）。
   这是**设计变更**（与"dead-letter 行保留以便 replay"冲突），本次刻意未做。

## §10 范围声明

- **改了产品源码**（`firmware/main/sync/**`、`firmware/main/application/**`、`integration/.../nvs_outbox_storage.*`）
  与**测试**（1 个新套件 + 3 处既有期望更新 + 1 处 switch 补 case）
- **未改**：CMake、sdkconfig、partition、设备 NVS、`/today` 契约、Backend API schema、`DomainState.time_synced`
- **未做**：rebuild、reflash、推送远端、开始 C01
