# T1 缺陷的业界先例检索（prior art）：这类问题叫什么、成熟实现怎么解

**目的**：确认 A05-DEVICE-T1 的 `ACK_PIPELINE_FAIL` 不是"我们特有的怪问题"，而是**有名的问题类**；
找出被大规模生产验证过的解法，用它来校验/修正我们自己的修复（方案 B+C）。
**检索时间**：2026-09-19。**结论**：我们的缺陷是经典的 **poison message（毒消息）在有序 at-least-once 队列上造成的 head-of-line blocking**；
业界有 **3 种**标准机制，其中 **1 种与我们的方案 B 完全对应**（而且是被协议明文规定的），另外 **1 种我们仍然缺失**。

---

## 1. 问题分类：我们踩的是哪一类

| 我们的实现 | 对应的业界概念 |
|---|---|
| `OutboxCore::applyBatchResult()` 只删「连续前缀内 Accepted/Duplicate 且 `seq <= last_acked`」的行 | **低水位 ACK / committed offset**（Kafka 的 committed offset 语义完全一致：**offset 不能跳**） |
| 队首一条 `conflict` 永久阻断整条队列 | **poison message / poison pill blocking a partition（FIFO group）** |
| 每 15 s 原样重发同一批 | **crash-loop / retry forever**，Kafka 文档原话：*"the consumer restarts, reads the same offset, crashes again — lag grows unbounded"* |
| UI 仍显示"已同步" | 缺失 **DLQ depth / blocked 计数器** 这类一等公民指标（Temporal、Vetora 均强调这是首要告警信号） |

> 检索原话（AWS SQS FIFO）：*"a poison head no longer stalls the group forever"* —— 与我们实测到的 livelock 一字不差。

## 2. 业界三种标准解法

### 机制 ① 有界尝试 + 移入 DLQ（evict），然后**提交 offset 继续**
- **Kafka/Spring Kafka**：`DefaultErrorHandler(recoverer, backOff)` + `DeadLetterPublishingRecoverer`，
  重试耗尽后由 recoverer 发到 `<topic>.DLT`，**`setCommitRecovered(true)` 让消费者提交 offset 并继续**；
  对**非瞬时**错误（反序列化/校验失败）用 `addNotRetryableExceptions(...)` **直接进 DLT、不重试**。
- **AWS SQS FIFO**：`RedrivePolicy.maxReceiveCount`（**典型 3–5**）——收到 N 次仍未删除 ⇒ 自动移到 DLQ，**同组后续消息解锁**；
  并强调别把 `maxReceiveCount` 设太大（毒消息浪费重试），太小（瞬时故障误进 DLQ）。
- 检索到的调优结论：`3–5` 是常态；DLQ 保留期给到最大（SQS 14 天）以便事后 replay/redrive。

### 机制 ② 跳过并前进（seek past the offset）
- Kafka **KIP-334** 特意让 `RecordDeserializationException` 带上 `topicPartition()` 与 `offset()`，
  **就是为了让客户端能写 `consumer.seek(tp, offset + 1)`** —— 粗暴但有效。
- ⚠️ Kafka **故意不自动跳过**（"in a financial or audit context 'skip silently' is data loss"）——
  这一点和我们项目"dead-letter 行保留以便 replay"的取向一致，**跳过必须是显式策略，不能是默认行为**。

### 机制 ③ 服务端基线失效 ⇒ 客户端**全量重同步（resync）**
- **RFC 6578（WebDAV/CalDAV 增量同步）**：同步令牌由服务端发放；**服务端有权作废旧令牌**，
  作废时返回 `DAV:valid-sync-token` 前置条件失败（实现升级、改动历史被截断等），
  **客户端必须回退到"用空令牌做全量同步"**。
- 实现侧（`fast-dav-rs` 的 `SyncSession`）：*"a stale token (410 Gone, 403 + valid-sync-token) resets to a full initial sync flagged `resynced`"*。
- **意义**：这是一条**协议明文承认"客户端与服务端的基线会分歧"**、并**规定客户端必须能自愈**的先例。
  ⇒ 我们的**方案 B（把本地序号空间重基到服务端基线）就是机制 ③ 的等价物**，不是自创的偏方。

### ⚠️ 机制 ① 的已知代价（我们必须写进报告）
SQS FIFO + DLQ 的官方说明明确承认：**一旦允许把失败消息移出，严格顺序保证就被打破**
（原文例子里 message 3 在 message 2 失败后被处理）。想要严格顺序只能"整组一起失败"（all-or-nothing per group）。
⇒ **我们的方案 B 在顺序语义上优于机制 ①**：重基是**按原相对顺序**重编号，**不改变事件先后**。

## 3. 逐项对照：我们的候选 ↔ 业界

| 业界机制 | 我们的候选（修复前） | 我们的修复（B+C） | 是否达标 |
|---|---|---|---|
| ① 有界尝试 + 移出（evict） | **完全没有**（dead-letter 行只打标、保留、且**不跳过**） | **仍未提供** | ❌ **仍缺失** → 见 §4 |
| ② 显式跳过前进 | 无 | 无（且**故意不做**：与"保留以便 replay"的设计一致） | ⚪ 有意不做，需 Codex 确认 |
| ③ 基线失效 ⇒ resync | **完全没有**（服务端 ack 领先被当成"上限"后丢弃） | **方案 B 已实现**（`rebaseSequences`，无损、单 commit、fail-closed） | ✅ 对齐协议先例 |
| 可观测性（DLQ depth / blocked 计数） | 无（假 `Synced`） | **新增 `SyncOutcome::Blocked` + `sync_failed` 诊断** | ✅ |

## 4. 🚩 检索暴露出的**剩余缺口**（必须让 Codex 知道）

方案 B 的触发条件是 **`服务端 ack 领先本地`**。因此有一个场景它**救不了**：
**队首那条永久不可确认的行，服务端 ack 并不领先**——典型是**业务非法事件**（服务端稳定返回 `422 invalid_event`，
永远不会接受它），此时：
```
服务端 ack 不动  →  server_ahead = false  →  不触发重基  →  永久 Blocked
```
⇒ 修复后这种队列**不再静默**（会如实报 `Blocked` + `sync_failed`），但**依然不会自愈**。
这正是业界机制 ① 存在的原因：**必须有"尝试次数上限 + 移出/隔离"这条最终安全阀**。

**建议（留给 Codex 决策，不由我单方面改设计）**：
- 在 outbox 行上增加**尝试计数**，超过上限（业界共识 3–5）后把该行**移入隔离区（DLQ 语义）**并**允许 ACK 跨过**；
- 与现有"dead-letter 行保留以便 replay"的设计**存在冲突**（现在是"保留且继续阻断"），属**设计变更**，需 Codex 拍；
- 若采纳，必须同时接受 SQS 文档点明的**顺序代价**，或采用"整组 all-or-nothing"的变体；
- 隔离区必须是**可观测且可 replay** 的（对齐 SQS 的 redrive / Kafka 的 DLT 重放思路）。

## 5. 检索来源

- Kafka 毒消息 / DLQ：<https://www.ggorantala.dev/kafka-serializationexception-poison-pill>、<http://www.bethecoder.com/applications/tutorials/messaging/apache-kafka/kafka-dead-letter-queue.html>、<https://blog.devops-monk.com/tutorials/spring-kafka/error-handling-basics>、<https://codefarm.in/guides/backend-engineer/05-messaging-async-workflows/kafka-reliability-and-scaling>
- SQS FIFO / maxReceiveCount / 顺序代价：<https://rahulpnath.com/blog/amazon-sqs-fifo-error-handling-dotnet>、<https://kloudvin.com/article/sqs-sns-fan-out-fifo-ordering-dlq-poison-message-handling>、<https://vetoralabs.com/system-design/concepts/messaging/dead-letter-queues>
- RFC 6578 同步令牌作废 ⇒ 全量 resync：<https://www.greenbytes.de/tech/specs/rfc6578.html>、<https://docs.rs/fast-dav-rs/latest/fast_dav_rs/webdav/sync/struct.SyncSession.html>、<https://deepwiki.com/aluxnimm/outlookcaldavsynchronizer/5.1-caldav-and-carddav-data-access>
- Outbox 模式本身与"单调序列/水位"判重（与本项目 `event_id + sequence` 判重完全同构）：<https://microservices.io/patterns/data/transactional-outbox.html>、<https://www.decodable.co/blog/revisiting-the-outbox-pattern>
