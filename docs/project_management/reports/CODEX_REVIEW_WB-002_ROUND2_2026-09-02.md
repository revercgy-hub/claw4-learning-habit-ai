# Codex 复检报告：WB-002 MVP 架构定义 Round 2

- 日期：2026-09-02
- 复检对象：远端 `workbuddy/wb-002-architecture` @ `6251486ce88d841cfc40f23ef4f048ec751b7191`
- 修订基线：`bfa5f5e76270c2eed475faf0293ee3986c360007`
- 主线基线：`32cf6bfb2d1075783a9c7e3c9f87846a49e9d7a7`
- 复检人：Codex
- 结论：`CHANGES_REQUIRED`
- 下一任务：仍为 `WB-002` 窄范围契约修订；不授权源码、构建、硬件、串口、Flash 或外部服务操作

## 1. 已通过项目

- 修订提交是 Codex 同步基线 `bfa5f5e` 的单一直接后继；`origin/main` 是该提交的祖先。
- 修订相对 `bfa5f5e` 只修改 `docs/ARCHITECTURE.md` 与 `docs/project_management/reports/WB-002_REPORT.md`。
- `git diff --check bfa5f5e..6251486` 无输出。
- 本地两个未跟踪交付副本的 blob 与远端 `6251486` 对应 blob 完全一致；复检以远端对象为准。
- 10 个 JSON 代码块经 `ConvertFrom-Json` 独立复验全部通过。
- 21 个 Markdown 本地链接独立复验全部存在。
- CR-WB002-01 已基本闭环：领域快照与关键事件使用 outbox 原子顺序；关键事件写入失败时不提交领域状态；队列压力先处理非关键 telemetry；同步诊断不递归进入业务队列。
- CR-WB002-02 的连续 ACK、逐事件结果、关键 4xx 保留和 401/403 不删除事件规则已建立。
- CR-WB002-03 的 Task 与 StudySession 分层、Timer 不自动完成 Task、显式完成规则已建立。
- CR-WB002-04 的 `/events/batch` 唯一写入入口、设备—儿童逐请求校验、短期 token 和 nonce 防重放方向已建立。
- 报告记录了修订位置、实际验证、剩余风险和范围边界。

## 2. 必须修订的问题

### CR-WB002-06：重复投递与 sequence 校验顺序冲突

`docs/ARCHITECTURE.md` §5.4 同时规定：

- 所有事件逐项校验 `sequence == last_acked+1`，重复/回退返回 `rejected`；
- 已存在的 `(device_id, event_id)` 返回成功语义 `duplicate`。

典型 at-least-once 场景是：服务端已接受 sequence 42 并推进 ACK，但响应丢失；设备会用同一 `event_id` 重发 sequence 42。此时 sequence 已小于或等于服务端 ACK。若先执行严格 sequence 检查会拒绝，若先执行幂等检查则应返回 `duplicate`。当前文档没有定义优先级，后续 Mock 与设备端可能实现出两种不兼容行为。

修订要求：

1. 明确服务端逐事件处理顺序：完成认证与归属校验后，先查 `(device_id, event_id)` 幂等记录，再对新事件做 sequence 连续性检查。
2. 已存在且关键不可变字段/载荷摘要一致的事件返回 `duplicate` 成功语义，并返回当前连续 ACK；客户端可按现有“两条件”规则删除。
3. 同一 `event_id` 但 sequence、type 或载荷摘要不同必须作为冲突拒绝并告警，不能伪装成 duplicate。
4. 仅对未见过的新事件应用连续性规则：`sequence == last_acked+1` 才可处理；大于期望值返回 gap；小于或等于 ACK 且 event_id 未见过返回回退/冲突，不推进 ACK。
5. 在文档与报告中加入“服务端落库成功但响应丢失后重发”的可测试示例。

### CR-WB002-07：nonce 有验证规则，但没有可调用的 challenge 获取步骤

`docs/ARCHITECTURE.md` §6.3 的 `/devices/auth` 请求要求设备提交“服务端下发的一次性 nonce + 签名”，§6.4.3 也声明 nonce 由服务端下发；但端点表和最小流程没有设备获取 nonce 的请求/响应。该契约无法直接实现或编写 Mock 测试。

修订要求：

1. 二选一并统一全文：新增明确的 challenge 端点，或把 `/devices/auth` 定义为有清晰请求/响应的两阶段交互。
2. challenge 响应至少包含可关联的一次性 nonce/challenge_id 与过期时间；签名输入至少绑定 `device_id`、challenge 标识和 nonce，避免跨设备/跨 challenge 复用。
3. 明确 challenge 只能使用一次、过期/重放返回 401，成功认证后立即作废；不得在日志记录 nonce、secret 或签名原文。
4. 补充可解析的最小 JSON 示例和失败矩阵；不得引入真实凭据。

### CR-WB002-08：claim 未建立“已认证家长 → 获授权儿童”的服务端信任链

`/devices/claim` 示例仍由请求体提交 `parent_id`，而文档只笼统写“家长 PWA”。没有规定该端点必须使用已认证家长会话，也没有规定 `child_id` 必须属于该家长。服务端若信任请求体，可把设备绑定到任意自报 parent/child，未满足 CR-WB002-04 的最小家长授权边界。

修订要求：

1. 明确 `/devices/claim` 必须由已认证家长会话/短期家长 token 调用。
2. `parent_id` 必须从服务端认证上下文派生，不接受请求体自报；从示例中删除 `parent_id`。
3. 服务端必须校验目标 `child_id` 属于当前已认证家长的授权范围，才可建立 device-parent-child 绑定。
4. 未授权儿童、无效配对码和不存在对象继续使用不泄露儿童存在性的通用外部错误；服务端内部保留脱敏审计原因。
5. 更新最小 JSON、权限规则和失败矩阵，使 claim、token 声明与逐事件校验形成一条完整信任链。

### CR-WB002-09：损坏快照的 `aborted/auto_saved` 仍有两套语义

§4.3 写“快照损坏 → session 标 `aborted`/`auto_saved`”，但 §4.4、§4.5 和 §10.3 已写成“快照损坏 → `aborted`；快照完整并由用户结束时可 `auto_saved`”。验收标准要求状态机前后一致，目前仍不能作为唯一测试预期。

修订要求：

1. 全文统一为：快照损坏/不可恢复只产生 `aborted`；快照完整、恢复同一 session 后由用户选择结束/保存才可产生 `auto_saved`。
2. `auto_saved` 与 `aborted` 均不得自动完成 Task；保持现有显式 `task.completed` 不变量。
3. 更新报告的残留语义扫描并列出实际命中结果。

### CR-WB002-10：报告与复验

1. `WB-002_REPORT.md` 增加 CR-WB002-06～09 的逐项修订位置、测试向量和剩余风险。
2. 重新运行 JSON、链接、章节、证据标签、旧语义残留和 `git diff --check`。
3. 修订仍只允许两个原文件；不得创建实现代码或额外文档。
4. 新提交继续使用普通快进 push；不得 force、rebase、reset、删除分支或改写 `main`。

## 3. Codex 独立验证记录

```text
远端任务提交：6251486ce88d841cfc40f23ef4f048ec751b7191
远端 main：32cf6bfb2d1075783a9c7e3c9f87846a49e9d7a7
main 为任务提交祖先：PASS
修订范围（bfa5f5e..6251486）：仅两个允许文件
git diff --check：PASS
JSON：10/10 PASS
Markdown 本地链接：21/21 PASS
本地/远端交付 blob：2/2 MATCH
旧语义扫描：发现 §4.3 损坏快照 aborted/auto_saved 残留
契约反例：已接受 seq=42 但响应丢失；同 event_id/seq=42 重发时处理优先级未定义
认证反例：设备没有文档化的 nonce/challenge 获取调用
授权反例：claim 请求体自报 parent_id，未规定 child 归属校验
```

## 4. 调度结论

- `WB-002` 继续为 `CHANGES_REQUIRED`；WorkBuddy 下一轮只处理 CR-WB002-06～10。
- 不合入未验收的架构提交到 `main`。
- 不发布 WB-BRINGUP-S1 或业务开发任务；架构信任链与幂等重放必须先成为唯一、可测试的契约。
- WorkBuddy 完成并推送后声明 `REVIEW_READY`，由 Codex 执行 Round 3 复检。
