# A05-DEVICE-T1 — 缺陷根因与修复方案（完整报告 · 供 Codex 裁定）

**性质**：T1 现场发现的**根因分析 + 修复方案设计**（本文件**不含任何源码改动**，改动需 Codex 授权后另开任务）
**基线提交**：`d5e64c5`（+ 本地 `1a3d3fe` 证据提交，未推送）
**Candidate**：`claw4-v53-m0-a05-2c8f58f`（`c035e1c09ebe472f5f14b490c93844aa3e298cd11b4e30f7fe1055e5b9278d36`）
**T1 结果**：`NOT PASSED`，分类 `ACK_PIPELINE_FAIL`（证据见 `reports/WB_A05_DEVICE_T1_001_REPORT.md`、`evidence/a05-device/T1-FINDING-ACK-PIPELINE-FAIL.md`）
**本文件回答**：**根因是什么 / 为什么现在必然发生 / 有几种修法 / 各自代价与风险 / 怎么验证**

---

## §1 结论摘要（一屏）

| 问题 | 答案 |
|---|---|
| 是网络问题吗？ | **不是**。Host Gate 全过（`192.168.3.26:18765` LISTEN、`/health` 200、防火墙已放行），服务端每轮都返回 `200` |
| 是解码问题吗？ | **不是**。用设备**实际收到的原样 3364 B** 喂设备自带 `DecodeBatchResponse()` ⇒ `TRUE`、`last_acked=21`、20 条映射全对 |
| 根因是什么？ | **`AppCoordinator::scopeBatchToSent()` 的"连续前缀走查"遇第一条不可确认行即 `break`**，把整个 ACK 判为"未确认"，回落到设备自己的 `0`；而 outbox 的 ACK 是**低水位删除**（`removeAcked` 删 `≤ up_to` 的全部），于是**一条都不删** ⇒ 每 15 s 重发同一批 ⇒ **永久 livelock** |
| 触发条件苛刻吗？ | **不苛刻**。只要**队首存在任何一条被服务端永久拒绝的行**（`conflict`，或打上 dead-letter 的业务 4xx）就会永久卡死。真实世界触发路径：服务端 DB 回滚/换库、设备重新 provisioning、NVS 被擦、历史库与设备 outbox 不同源 |
| 会静默吗？ | **会**。`applySyncResult()` 无论是否清除了行都回 `SyncOutcome::Synced` 并 `setDiagnostic(false,true)`，UI 显示"已同步/已恢复"（设备侧 `R|1` 实证） |
| 有解吗？ | **有，3 类**：① 终端行可跳过（最小、**有损**）② 序号重基/重排队（**无损**，推荐）③ 启动耦合修复（独立小修）+ 必须配套"不报假成功" |
| 已在主机侧复现了吗？ | **是，确定性复现**：用**真实 wire 字节**驱动**真实 `AppCoordinator`+`OutboxCore`**，6 项断言全中（详见 §9；harness 与运行输出已入库） |
| 只换干净库能过 T1 吗？ | 能，但**不修任何缺陷**——属环境绕过，不构成修复（见 §7） |

---

## §2 故障链条（行号级，全部有源码依据）

设：设备本地 `last_acked_sequence = A_local = 0`；pending = `seq 1..20`；
服务端 `last_acked_sequence = 21`（设备自身事件把 13→20；随后本轮的诊断探针又把它推到 21，见 T1 报告 §9），
且服务端 `seq 1..13` 槽位属于**别的 event_id**，`seq 14..20` 属于本设备（已落库）。
> 注：服务端 ack 的具体数值（20/21）**不影响**本故障——失败点在 `scoped.last_acked_sequence` 被回落为**设备自己的 0**（见 ③）。

```
① prepareSync()                            coordinator.cpp:354
   first_sent_sequence = A_local + 1 = 1   :370
   从 pending[0] 起按连续前缀收集 → 20 条   :372-387
   max_sent_sequence = expected - 1 = 20    :391
   ⇒ 发出去的请求：last_acked_sequence = 0，events = seq 1..20   【wire 捕获实证，逐轮逐字相同】

② 服务端逐事件判定（main.py:_process_batch 321-372）
   seq 1..13 : lookup_sequence 命中「别的 event_id」⇒ conflict(409)
               reason = sequence_already_used_by_other_event   (FIX-08)      :364-371
   seq 14..20: lookup_event(event_id) 命中且 digest 一致 ⇒ duplicate     :346-351
   响应：{last_acked_sequence: 21, results:[13×conflict, 7×duplicate]}   【wire 捕获实证】

③ scopeBatchToSent()                       coordinator.cpp:298
   先按 sequence 建索引（in-range + event_id 匹配）                        :305-318
   再走连续前缀：
     for ev in envelope.request.events:                                    :322
       row.outcome != Accepted && != Duplicate  ⇒  break                   :326-329
     ← 第 0 次迭代就 break（seq=1 是 Conflict）
   any_confirmed = false                                                   :321,330
   ⇒ scoped.last_acked_sequence = envelope.request.last_acked_sequence      :334-336
                                = A_local = 0
   scoped.results 转发全部 in-range 行（20 条，含 13 条 Conflict）            :346-350

④ applyBatchResult()                       outbox_core.cpp:146
   (1) 只给「Rejected + 400≤http<500，且非 401/403」打 dead-letter          :154-163
       ← 本次 13 行是 Conflict（EventOutcome::Conflict），**不打标记**
       （⇒ 实测 dead_letter=0 是预期，不是 bug）
   (2) 从 results 尾部向前找「ok_outcome 且 sequence ≤ scoped.last_acked(=0)」 :169-174
       seq20..14 都是 Duplicate 但 20..14 ≤ 0 不成立；seq13..1 不满足 ok_outcome
       ⇒ rit == rend() ⇒ **"nothing to remove"，且返回 Committed（成功！）**   :175-180
   ⇒ 一条不删；本地 A 仍为 0                                    【NVS 实证 E=20 Q=21 A=0】

⑤ applySyncResult() 收尾                  coordinator.cpp:450-472
   retry_count_=0 / pending_backoff_ms_=0 / reauth_attempted_=false
   setDiagnostic(false, true)  ← 诊断槽标记"已恢复"
   返回 SyncOutcome::Synced    ← **假成功**
   ⇒ UI 显示"已同步"                                    【设备侧 R|1 实证】

⑥ 下一周期重复 ① ⇒ 每 ~15 s 原样重发，永续
```

### §2.1 两个被"顺手踩到"的结构性问题

- **`removeAcked(up_to)` 是低水位语义**（`ports/nvs_outbox_storage.cpp:120-132`）：
  `erase_if(sequence <= up_to)` 且 `last_acked = max(cur, up_to)`。
  ⇒ **不可能只删 seq 14..20 而保留 1..13**。因此"队首被拒"必然导致**它后面所有已确认的行也一起被扣住**。
  这是 §4 里任何方案都必须先回答"被拒行怎么处置"的原因。
- **`dead_letter_reason` 写了没人读**（全树只有 `outbox_codec.cpp:149,167` 序列化 + `nvs_outbox_storage.cpp:140` 写入）：
  dead-letter 行**仍在 `pending` 里**，`prepareSync()` 仍会带上它（`coordinator.cpp:373` 不检查该字段）
  ⇒ "保留以便重放"的设计与"低水位 ACK"叠加 = **永久阻塞**。业务 4xx 场景同样会卡死（不只是 `conflict`）。

---

## §3 设计意图 vs 缺陷边界（为什么这么写、在哪一步走偏）

**原设计意图（源码注释原文）**：`coordinator.cpp:287-297` 说明该函数是 **A03/RF2** 的产物——
> "The server's own `last_acked_sequence` is treated as an upper bound only; it is never sufficient on its own to delete local pending rows."

即：**不信任服务端 ACK 单独删除本地行**，必须由设备按"连续前缀 + event_id 匹配 + 结果合格"三重校验后自行判定。
`applyBatchResult` 的注释（`outbox_core.cpp:165-168`）也明确"只有前缀内的 Accepted/Duplicate 可被删除"。

**这是一个合理且必要的防御设计**（防止服务端 ACK 越界删掉本地还没发出去的事件）。它的问题**不在意图，而在缺少三条边界处理**：

| # | 缺失的边界 | 后果 |
|---|---|---|
| 1 | **没有区分"永久不可确认"与"暂时不可确认"** | `Conflict`（槽位归属分歧，永久）与"缺结果/未匹配"（暂时）走同一条 `break` 路径 ⇒ 一条永久坏行 = 全队列永久阻塞 |
| 2 | **没有"失同步"这个概念与恢复动作** | 设备基线 `A=0` 与服务端 `A=20` 的分歧**没有**任何检测点、也没有重基/重排队原语；协议里也没有"服务端告知设备应重基"的字段（本次响应的 `last_acked_sequence` 事实上就是这个信号，但被 §③ 的回落逻辑吃掉了） |
| 3 | **没有把"未能推进"当成失败上报** | `applySyncResult()` 一律返回 `Synced`（第 462-471 行），诊断槽还被清成"已恢复" |

**为什么在本次必然发生（不是巧合）**：
`prepareSync()` 只从 `A_local + 1` 起取**连续前缀**（第 370-374 行）⇒ 一旦 `A_local` 落后于服务端且
服务端前若干槽位被历史事件占用，**首条必然 `Conflict`** ⇒ ③ 必然回落为 0 ⇒ ④ 必然删 0 条 ⇒ ⑤ 必然报 `Synced`。
整条链路是**确定性**的，不是概率性竞态。

**契约层面的结论（供 Codex 判断）**：服务端把 `(device_id, sequence)` 做成**全局唯一**（FIX-08），
意味着"序号槽位"是**稀缺资源**，而设备侧把"序号"当作**可回落到本地值的本地资源**——
两侧对"序号是设备私有还是全局共享"的假设不一致，是本次缺陷的**架构级根因**。
修法本质就是**把设备侧改为承认序号的全局性**（方案 B：接受服务端基线并重排队），
或**接受该序号永久作废、丢弃该事件**（方案 A：终端行可跳过）。

---

## §4 修复方案（三类，可组合）

### 方案 A — 终端拒绝行"可跳过"（最小改动，**有损**）

**思路**：把"永久不可确认"的行与"暂时不可确认"的行区分开；前者允许 ACK 跨过。

**改哪里**
1. `coordinator.cpp:298 scopeBatchToSent()`：走查改三态
   - `Confirmed`（Accepted/Duplicate）→ 推进
   - `Terminal`（`Conflict`，或 outcome==Rejected 且 400≤http<500 且非 401/403，即 dead-letter 类）→ **不 break，记为 terminal，继续**
   - `Unknown`（缺行 / 未匹配 / Gap）→ **break**（保持原语义，不越过未知区）
2. `applyBatchResult()`：把 `up_to` 的选取从"最后一行的 sequence"扩展为"最后一个 Confirmed 或 Terminal 行"
3. 对 Terminal 行先 `markDeadLetter(event_id, "terminal_conflict:sequence_owned_by_other_event")`，并**只在 `sequence <= batch.last_acked_sequence`（服务端已越过）时**才允许跨过 —— 保证不会跨过服务端尚未确认的区域

**效果**：`up_to = 20` ⇒ `removeAcked(20)` ⇒ `removed=20>0`、`A: 0→20` ⇒ 队列解锁、T1 可 PASS

**代价 / 风险**
- ⚠️ **这 13 条事件被永久丢弃**（契约下它们在当前序号不可投递）。若其内容有业务价值 ⇒ 静默丢数据。
  缓解：dead-letter 标记 + 计数上报 + 只在服务端已越过该序号时才丢。
- 语义变化会影响 `applyBatchResult` 的所有既有 Host 测试断言（RF2/RF3 相关），需逐条复核不得放松"不越过未知区"的保证。

**代码量**：~25–40 行 + 新增 Host fixture（队首 Conflict + 后段 Duplicate）

---

### 方案 B — 序号重基 / 重排队（**无损，推荐**）

**思路**：识别"服务端基线领先本地"这一事实，把**未确认的本地行原样重编号**到服务端基线之后，再正常重发。

**改哪里**
1. 新增持久化原语（`outbox_storage.h` + `ports/nvs_outbox_storage.cpp`）
   ```cpp
   // 将 pending 中未确认的行按原相对顺序重编号到 new_base+1..new_base+n，
   // event_id / payload / type / version 全部不变；next_sequence 顺延。
   virtual CommitStatus rebaseSequences(int64_t new_base) = 0;
   ```
   实现要点：一次 `load` → 就地重写 `sequence` → `last_acked_sequence = max(cur, new_base)` →
   `next_sequence = new_base + n + 1` → 一次 `Save`（单次 commit，保证原子性）
2. `coordinator.cpp applySyncResult()`：在 `scopeBatchToSent()` 之后、`applyBatchResult()` 之前插入**失同步检测**
   ```
   if (batch.last_acked_sequence > envelope.request.last_acked_sequence
       && scoped.last_acked_sequence == envelope.request.last_acked_sequence) {
       // 服务端已越过 us，而我们一行都确认不了 ⇒ 基线失同步
       outbox_.rebaseSequences(batch.last_acked_sequence);
       out.outcome = SyncOutcome::Resync;
       return out;   // 下一周期从 server_ack+1 起发，全部是新槽位
   }
   ```
3. 可选加固：`Conflict` 出现在 `sequence <= batch.last_acked_sequence` 时**也**判定失同步
   （FIX-08 的 `sequence_already_used_by_other_event` 本身就是"槽位归属分歧"的定义）

**效果**：重基后 `prepareSync` 从 `server_ack+1` 起发 → 全部空槽 ⇒ 逐条 `Accepted`（其中 7 条同 `event_id` 的命中 `duplicate`，服务端不会重复落库）⇒ `removed>0`、`A` 推进 ⇒ T1 PASS
**数据丢失：0**（13 条以新序号投递成功，7 条按 event_id 去重）

**代价 / 风险**
- 新增持久化原语，需覆盖：**崩溃中断的原子性**（`Save` 单次 commit）、**幂等**（重复调用不叠加位移）、**与 generation/SessionLease 的并发**（复用现有 `envelope.generation != generation_` 陈旧判定即可挡住旧 worker）
- 语义变化面更大（引入了"设备可自改序号"的能力）⇒ 需 Codex 明确它是否符合契约（服务端 `(device_id, sequence)` 全局唯一，空槽可自由占用 ⇒ **契约上允许**）
- 需要"重基次数/上限"保护，避免与异常服务端形成重基风暴（可加：同一 generation 内最多重基 1 次）

**代码量**：~80–120 行 + 2 个原语 + 单测（含中断/幂等）

---

### 方案 C — 必须配套：不报假成功（可观测性）

**问题**：`applySyncResult()` 在"什么都没清"的情况下仍返回 `Synced` 并清诊断槽 ⇒ 缺陷静默。
**改哪里**：让 `OutboxCore::applyBatchResult()` 返回"实际清除条数 / 是否推进 baseline"（`PersistResult` 加字段），
`applySyncResult()` 据此：
- 有清除或无工作 ⇒ `Synced`（原语义）
- 未清除且 `scoped.last_acked` 未推进 ⇒ 新 outcome `SyncOutcome::Blocked`，`setDiagnostic(true,false, reason="queue_blocked_<n>")`

**收益**：UI/诊断槽能如实显示"同步受阻"，T1 这类问题当场可见而不是靠读 flash 才能发现。**代价极小**（~20 行 + 测试断言）。

---

### 方案 D — 启动耦合修复（独立小修）

**问题**：`LearningRuntime::Init()`（内含每 15 s 的同步 worker）**全树唯一调用点** = `display/screen/learning_screen/learning_screen.cc:607`
⇒ 复位/掉电/OTA 后**同步不自愈**（实测双向确认：读 flash 复位后后端日志停滞 ↔ 打开学习页 3 秒内恢复）。
**改哪里**：在应用启动序列（`main/application.cc::Start()` 或 board 初始化完成后）补一次
`LearningRuntime::Instance().Init()`；`learning_screen.cc:607` 的调用保留为幂等保护。

**风险**：低。需确认 Init 幂等、且不在无网络/无凭据时阻塞启动（可延后到首个网络就绪事件）。
**代码量**：~5–15 行 + 启动顺序单测（设备 TU，Host Gate 覆盖不到 ⇒ 报告须如实标注）

---

## §5 推荐组合

| 优先级 | 组合 | 说明 |
|---|---|---|
| **推荐** | **B + C + D** | 无损自愈 + 不再假成功 + 复位后可自愈。代码量最大但是唯一"产品级正确"的组合 |
| 折中（若需压任务量） | **A + C**（暂不做 D） | 能立即解锁队列、T1 可复测通关；代价是丢 13 条历史事件，且自愈能力仍缺失（换库/重 provisioning 仍会卡，但会被 C 如实报为 `Blocked`） |
| 不建议单独做 | 只做 C 或只做 D | C 只让缺陷可见不解锁；D 只解决"复位后不同步"，不解决 livelock |

**最小可行补丁（若 Codex 只批一个）**：**B**（重基）。它一条就同时解决"当前必卡"和"任何未来基线错位"。

---

## §6 验证计划（方案获批后执行；本文件不执行）

1. **Host 单测**（`tools/dev/verify-host-cpp-tests.ps1`，29 suites + interface contracts 必须全绿）
   新增/扩展用例：
   - 队首 `Conflict` + 后段 `Duplicate`（复现本缺陷，断言修复后 `removeAcked` 被调用且 `A` 推进）
   - 服务端 ack 领先本地（驱动 §4-B 的重基路径），断言重基后 `sequence` 连续、`event_id` 不变、`next_sequence` 顺延
   - **证伪检验**：把修复临时还原，同套用例必须 FAIL（本项目纪律）
   - 方案 C：断言未清除时返回 `Blocked` 而非 `Synced`
2. **接口契约门禁** `tools/dev/verify-interface-contracts.ps1`（改了 `outbox_storage.h` 必跑）
3. **冷构建**：runner `tools/dev/run-a05-cp2-build.ps1`（隔离树短路径 `E:\a05c\s` / `E:\a05c\b`）
   ⚠️ **尺寸硬门禁**：`ota_0` 现余量仅 **165,424 B / 1.75%** —— 新增代码后必须重算 app 尺寸 vs `ota_0`，超限即 FAIL
4. **新 Candidate 冻结**（CP4 流程：Candidate Manifest + Freeze Report + 5 件产物 sha256）
5. **刷机**：A05-DEVICE D0（只读预检）→ D1（刷前重算哈希 + 刷后全量回读）→ D2
6. **重跑 T1**：建议用**经 Codex 同意的 `ack=0` 干净库**做基线（见 §7 说明）；
   判据仍是 §7 三层证据（backend 收到 / `removed>0` + `A` 推进 / 三方 event_id 一致）
7. **候选完整性守护**：每轮联网后复读 `otadata` + `ota_0` 全量 sha256 + `ota_1` 头

---

## §7 ⚠️ 环境绕过 ≠ 修复（请 Codex 明确不要用这个当唯一手段）

给设备一个"干净服务端基线"（`ack=0`、无历史事件）时，`seq 1..20` 恰好都是空槽 ⇒ 全部 `Accepted` ⇒
`scoped.last_acked = 20` ⇒ `removeAcked(20)` ⇒ `removed=20>0`、`A` 推进 ⇒ **T1 会 PASS**。

但这**没有修任何东西**：
- `conflict` / dead-letter 行仍然会永久钉死队列；
- 任何真实世界的基线错位（服务端 DB 回滚、换库、设备重新 provisioning、NVS 被擦）都会**原样重现**；
- 而且它**静默**（UI 报"已同步"）。

⇒ 干净库只应作为**修复后复测的手段**，不应作为"让 T1 变绿"的手段。本次 `NOT PASSED` 建议保留。

---

## §8 待 Codex 裁定（可逐条回复）

| # | 问题 | 我的建议 |
|---|---|---|
| 1 | §8 分类是否认可 `ACK_PIPELINE_FAIL`？本 Candidate 是否直接按 §31 冻结为 `FAILED DEVICE CANDIDATE`？ | 认可 / 建议冻结 |
| 2 | 修法选 **A（有损跳行）/ B（无损重基）/ C（不报假成功）/ D（启动耦合）** 中的哪些？ | **B + C + D** |
| 3 | §4-B 的"设备自行重基序号"是否符合契约？（服务端 `(device_id, sequence)` 全局唯一、空槽可自由占用） | 契约上允许；需明确是否要求"同 generation 内最多重基 1 次" |
| 4 | 那些"服务端槽位已被别的 event_id 占用"的历史行，业务上**允许丢弃**还是**必须重排队投递**？ | 必须重排队（无损），除非确认内容是重复数据 |
| 5 | 第 D 条（`LearningRuntime` 只在学习页启动）是否同批修复，还是另开任务？ | 同批修（它是"设备上电即同步"的前提） |
| 6 | 复测环境用**还原后的旧库**（`ack=13/21`，会再次制造 conflict）还是**授权的 `ack=0` 干净库**？ | 干净库（并由 §8-B 覆盖错位场景的单测） |
| 7 | 本次取证对历史后端库的写入（探针把 ack 推到 21、占用 `(device,seq=21)`）是否需先回滚再复测？ | 复测前回滚（备份已就位） |

---

## §9 主机侧复现（**已执行**，2026-09-18 20:31）

**目的**：把本缺陷从"靠读 flash 才能发现"变成**主机侧确定性可复现**——这是修复任务必须先有的失败基线（本项目纪律：修工具/逻辑必须配能失败的测试）。

**做法**（零设备风险、零源码改动）：
1. 取设备**实际收到的原样响应字节** `T1-device-received-response.json`（3364 B）；
2. 用设备自带 `wire::DecodeBatchResponse()` 解码成 `SyncClient::Response`；
3. 用**真实** `AppCoordinator` + `OutboxCore` + `FakeOutboxStorage`（门禁同款实现），把 outbox 预置成实机状态
   （`pending = seq 1..20`、`last_acked = 0`、`next_sequence = 21`；`sequence/event_id` **逐条取自设备真实请求**）；
4. `FakeSyncTransport` 注入上述真实响应 → `coordinator.runSyncOnce()`。

**编译/运行 recipe**（可直接复用）：
```
# 实现对象 = 门禁的子集（最小必需：learning_domain + sync + application，9 个 .o）
g++ -std=c++17 -Wall -Wextra -Werror -I firmware/main -I firmware/tests -I integration \
    -c <每个实现 .cpp> -o obj/N.o
g++ -std=c++17 -Wall -Wextra -I firmware/main -I firmware/tests -I integration \
    -c host_repro_ack_pipeline_fail.cpp -o repro.o
g++ repro.o obj/*.o -o repro.exe
./repro.exe T1-device-received-response.json
```
> 踩坑记录（本次实际遇到）：① `/e/workbuddy/...` 传给原生 `mkdir`/`g++` 会落到 `E:\e\workbuddy\...`，
> 一律用 `E:/...`；② 把**全部** 18 个公共对象都链进来会 `STATUS_ILLEGAL_INSTRUCTION (0xC000001D)`（启动即崩、无输出）
> —— 缩到最小 9 个对象即可；③ 崩溃时 stdout 被块缓冲吞掉 → main 首行加 `setvbuf(stdout,NULL,_IONBF,0)` 才能定位；
> ④ 新链出的 exe 可能被策略瞬时拦截（rc=126/127）→ 等 1–2 分钟重跑。

**PHASE 1 结果（真实数据，缺陷复现）**  —— 6/6 断言全中，0 失败：
```
REAL wire response: bytes=3364 last_acked=21 results=20
  runSyncOnce outcome     = 0 (Synced)
  requests sent           = 1
  request last_acked      = 0          <- 带的是设备自己的 0
  request events          = 20
  removeAcked called      = 0 time(s)  <- 从未调用 = 一条没删
  pending after           = 20
  last_acked after        = 0
  diagnostic sync_failed  = 0  sync_recovered = 1   <- 诊断槽还标成"已恢复"
```

**PHASE 2 结果（模拟方案 B 的反事实）** —— 3/3 断言全中：
```
(把 pending 重编号到服务端基线之上，再正常刷一轮；服务端行为由 fake 模拟)
  runSyncOnce outcome     = 0 (Synced)
  removeAcked called      = 1 time(s)
  pending after           = 0          <- removed = 20 > 0
  last_acked after        = 41
result: AS EXPECTED (0 failed assertion(s))
```

**结论与边界**：
- PHASE 1 = **真实证据**：缺陷在主机侧确定性成立，且**不依赖设备**，可直接作为修复任务的失败基线测试。
- PHASE 2 = **模拟**：只证明"清空槽位后管线会正常清理"，**不**证明服务端会接受这些新序号。
  要拿到真实服务端响应，需按 §6 用**副本库 + 另起一个 uvicorn**（`CLAW4_DATABASE_URL` 指向副本，端口 8001）
  做一次 scratch 回放——这一步**会写库**，因此**不写历史库**、并在报告披露。

**入库产物**：`evidence/a05-device/host_repro_ack_pipeline_fail.cpp`（harness 源码）、
`evidence/a05-device/host-repro-result.txt`（运行输出原文）。

---

## §10 证据索引

| 内容 | 位置 |
|---|---|
| T1 报告（§12 输出块 + 三层证据逐项核对） | `docs/project_management/reports/WB_A05_DEVICE_T1_001_REPORT.md` |
| 根因发现（wire 级） | `docs/project_management/evidence/a05-device/T1-FINDING-ACK-PIPELINE-FAIL.md` |
| 开测前只读预检（含 ACK 基线错位预判） | `docs/project_management/evidence/a05-device/T1-PRECHECK.md` |
| wire 捕获（请求摘要 + 响应体，无凭据） | `.../T1-wire-capture.jsonl`、`.../T1-wire-capture-pre-pageopen.jsonl` |
| 设备实际收到的响应原样字节 | `.../T1-device-received-response.json` |
| Host 判据 harness（设备解码器 × 真实字节） | `.../decode_probe.cpp` |
| 设备侧 outbox 解码器 / 后端 ack 基线探针 | `.../decode_outbox_blob.py`、`.../backend_ack_baseline.py` |
| 最终设备侧证据（含候选完整性） | `.../_t1-final-evidence.txt` |
| 涉及源码（只读引用） | `learning/application/coordinator.cpp:298,334,354,372,450`；`learning/sync/outbox_core.cpp:146`；`learning/sync/outbox_storage.h:22`；`learning/metalio_claw4/device/ports/nvs_outbox_storage.cpp:120,134`；`display/screen/learning_screen/learning_screen.cc:607` |

**本轮范围声明**：本文件是**方案与裁定请求**，未修改任何产品源码 / CMake / sdkconfig / partition / 设备 NVS，未 rebuild、未 reflash。
