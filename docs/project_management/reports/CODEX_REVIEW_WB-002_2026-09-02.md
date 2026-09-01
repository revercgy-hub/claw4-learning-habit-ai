# Codex 复检报告：WB-002 MVP 架构定义

- 日期：2026-09-02
- 复检对象：远端 `workbuddy/wb-002-architecture` @ `7902378dc844f1855f93708b14cb25c0c3b16fa4`
- 调度基线：`69f0ef57790dbbf9924f441e0bd668009f95874b`
- 复检人：Codex
- 结论：`CHANGES_REQUIRED`
- 下一阶段：仅修订架构契约；不授权源码、构建、硬件或外部服务操作

## 1. 已通过项目

- 提交是调度基线的单一直接后继。
- Git diff 只有 `docs/ARCHITECTURE.md` 和 `docs/project_management/reports/WB-002_REPORT.md`。
- `git diff --check origin/main...origin/workbuddy/wb-002-architecture` 无输出。
- 本地主工作区两个交付文件的 blob 与远端 Git blob 完全一致；复检以远端对象为准。
- 12 个必需章节齐全。
- 9 个 JSON 代码块经 `ConvertFrom-Json` 独立复验全部通过。
- 21 个 Markdown 本地链接独立复验全部存在。
- 证据标签、模块层次、MVP非目标、硬件未知项和不写源码门禁总体清晰。

## 2. 必须修订的问题

### CR-WB002-01：队列满时缺少“状态 + 事件”的原子持久化，可能丢完成事实

当前文档同时规定：

- 断网时仍可完成任务；
- 队列满后“隔离新业务事件”；
- 完成事件不可丢。

如果领域状态先变成 `completed`，随后事件因队列已满而被隔离或无法写入，后端永远收不到完成事实，违反不变量 I3。

修订要求：

1. 定义本地 transactional outbox/等价原子顺序：验证 intent → 持久化新领域快照与对应事件 → 提交成功后才向 UI 确认状态变化。
2. 如果关键事件无法持久化，不得提交 Task/StudySession 完成状态；UI 必须显示可恢复错误，重试不得生成新 event_id。
3. 为 `task.completed`、`study.session.completed` 等关键事件预留容量；队列压力下先合并/丢弃非关键 telemetry，不得隔离或丢弃关键业务事件。
4. `sync.failed`/`sync.recovered` 不得在同一已满业务队列中递归制造更多事件；明确它们是本地可合并诊断，或使用独立受限通道。

### CR-WB002-02：标量 ACK、跳号接收和死信规则不能同时保证 at-least-once

当前规则允许后端接受跳号事件，又用单个 `last_acked_sequence` 删除所有 `sequence <= N` 事件，同时把4xx事件移入死信。若序号42被拒绝、43被接受并返回 ACK=43，客户端可能越过42；即使死信保留原文，完成事实也没有进入权威数据库。

修订要求：

1. 把 ACK 明确定义为“最高连续、已持久化且业务处理成功的 sequence”，不得以批次最大成功序号代替。
2. `/events/batch` 返回逐事件结果或明确 rejected/gap 列表；客户端只删除明确成功/重复且位于连续 ACK 前缀内的 pending 事件。
3. 4xx业务拒绝的关键事件必须持久保留并可在修复后以**同一 event_id**重放，或进入明确的人工补偿流程；不得被当作已 ACK。
4. 401/403 是认证/授权失败，不是单个业务事件的死信条件；业务事件保持 pending，刷新/重新配对失败时暂停同步。
5. 明确 sequence 跳号、重复、回退和多批并发时的服务端事务规则；MVP可选择单设备串行批次以降低复杂度。

### CR-WB002-03：重启、timeout 与 auto_saved 的 Task/StudySession 语义互相矛盾

文档一处写重启后 Task 回到 `in_progress` 并让用户“重新点开始/完成”，另一处写 StudySession `ABORTED`，又写未完成 session 标 `auto_saved`。`timeout` 还被描述为自动落账，但不变量 I2 要求 Task 只能由孩子/家长完成。

修订要求：

1. 分别定义 Task 状态和 StudySession 状态，不使用 `completion_type` 替代 session status。
2. 明确重启恢复唯一流程：若快照完整，恢复同一 `session_id` 并让用户选择继续或结束；若快照损坏，session 标 `aborted/auto_saved` 并生成对应事件，Task 不得自动 completed。
3. Timer 到时只能结束/暂停专注段并提示用户，不得自动把 Task 标为 completed；只有显式孩子操作（未来可含家长）生成 `task.completed`。
4. 明确 `normal/manual/timeout/auto_saved` 分别作用于 StudySession 还是专注段，以及它们对 Task 状态的影响。
5. 更新状态图、异常矩阵、数据契约和不变量，使其只有一套可测试语义。

### CR-WB002-04：设备—儿童授权绑定和设备写入入口不明确

API 使用路径/载荷中的 `child_id` 与 `device_id`，但没有规定服务端如何把设备凭据绑定到获授权儿童。设备注册示例也允许客户端自报 `device_id`，缺少 pairing/claim、nonce 重放防护和事件归属校验。另有 `/events/batch` 与 `/study-sessions/{id}/finish` 两种设备写路径，可能双重落账。

修订要求：

1. 选择 `/events/batch` 为 MVP 设备业务写入的唯一权威入口；移除 `/study-sessions/{id}/finish` 的设备可调用替代方案，或明确它不属于设备MVP接口。
2. 定义注册 → 家长配对/claim → 设备认证 → 短期 token 的最小流程；device identity不得仅信任客户端自报字符串。
3. token/服务端会话必须携带允许的 device_id/child_id 绑定；后端对任务查询和每个事件重新校验绑定，拒绝载荷冒充其他设备/儿童。
4. 定义 nonce/challenge 的一次性、过期和防重放规则；注册/配对失败不得泄露儿童是否存在。
5. 明确 PWA 家长权限与设备绑定的最小授权边界；多家长细节可以继续待定。

### CR-WB002-05：报告与复验

1. `WB-002_REPORT.md` 增加 CR-WB002-01～04 的逐项修订位置、验证结果和剩余风险。
2. 记录初次提交 `7902378dc844f1855f93708b14cb25c0c3b16fa4`，修订回执给出新的本地/远端 HEAD。
3. 重新运行 JSON、链接、章节、证据标签和关键契约扫描；不得只写“预期”。
4. 修订仍只允许两个原文件；不得创建实现代码或新文档。

## 3. 下一阶段调度结论

- `WB-002` 进入 `CHANGES_REQUIRED`，WorkBuddy 只处理 CR-WB002-01～05。
- 当前不发布新的实现任务；上述问题直接影响离线不丢事件和儿童授权安全，必须在架构基线中先闭环。
- 修订期间不得构建、下载依赖、连接硬件、操作串口、修改固件、启动后端或外部服务。
- 修订通过后，Codex 将在 G2/G3 门禁内选择下一任务；业务源码仍为 `HOLD`。
