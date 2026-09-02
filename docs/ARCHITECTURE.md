# Claw4 学习习惯终端 MVP 架构基线（WB-002）

- 任务：WB-002 MVP 架构定义
- 分支：`workbuddy/wb-002-architecture`
- 日期：2026-09-01
- 性质：**开发前架构设计文档**。本文只定义边界、契约与决策，不授权实现；所有"建议/推荐"均标 `ARCH_DECISION`，不得视为已存在代码。
- 依据：WB-001 平台映射、WB-HW-001/002 真机证据、官方源码只读核查（`vendor/MetalioClaw4` @ `ca3aa3fa`）、产品总规划（`项目总规划/AGENTS.md`）
- 门禁：G2/G3 未通过前，第 11 章所列实现任务全部保持 `HOLD`；本文不自动解除任何门禁。

## 0. 证据标签约定

| 标签 | 含义 | 判定权 |
| --- | --- | --- |
| `PRODUCT_REQUIRED` | 产品/规划明确要求 | 项目总规划 |
| `SOURCE_CONFIRMED` | 官方源码/配置声明 | WB-001 平台映射 |
| `DEVICE_LOG_CONFIRMED` | 实机日志确认 | WB-HW-001/002 |
| `ARCH_DECISION` | 本架构的设计决策（尚未实现） | 本文 |
| `HARDWARE_VERIFY_REQUIRED` | 需真机验证后才能作为事实 | 实机 |
| `UNKNOWN` | 当前无可靠证据 | — |

> 规则：`ARCH_DECISION` 只描述"计划怎么设计"，不描述"系统已有该能力"；`SOURCE_CONFIRMED` 不等于实机成立；`HARDWARE_VERIFY_REQUIRED`/`UNKNOWN` 的项不得写成已确认。

---

## 1. 范围与证据分层

### 1.1 MVP 唯一闭环（PRODUCT_REQUIRED）

```text
今日任务（后端下发）
  → 设备首页展示
  → 学生点"开始"（或未来语音 START_TASK）
  → 创建 StudySession → 进入专注计时
  → 专注/暂停/恢复/完成
  → 生成本地事件
  → 事件队列同步后端
  → 家长 PWA 可见：已完成、实际时长、完成时间
```

**唯一闭环判定标准**（`PRODUCT_REQUIRED`）：以上链路在任何单一 Wi-Fi 会话内跑通，且断网场景下事件不丢失（见第 7 章），即视为 MVP 闭环成立。

### 1.2 设备侧必须可复用 / 必须新建 / 禁止修改（继承 WB-001 §5）

| 类别 | 内容 | 证据 |
| --- | --- | --- |
| 可复用 | 板级初始化、显示/触摸/LVGL 适配、ESP-Hosted 网络栈、协议层、音频服务、存储与电量计抽象、OTA 基础设施 | `SOURCE_CONFIRMED` |
| 需新建 | `learning_domain`、`sync`（事件队列/离线缓存/重试/幂等）、学习 UI、面向家庭后端的 API 客户端 | `ARCH_DECISION` |
| 禁止修改 | `sdkconfig`、partition CSV、屏幕驱动选择/时序、PSRAM 参数、ESP-IDF 主版本、官方 BSP | 硬门禁（`AGENTS.md`） |
| 禁止操作 | 刷写/擦除真机 Flash、改分区表、改 Bootloader/Secure Boot/Flash Encryption/OTA 策略 | `BLK-FLASH-AUTH-001` |

### 1.3 MVP 非目标（PRODUCT_REQUIRED，明确不做）

4G/GPS、持续摄像与云端实时视频、情绪/人脸识别、本地大模型、复杂数字人、排行榜、社交、在线课程、自动批改、OCR 批量识别、复杂 Agent 自主操作、大额激励商城。

> 摄像头在 MVP **默认关闭**（`PRODUCT_REQUIRED`）；语音仅保留 intent 接口边界（第 9.4 节），不实现唤醒词/ASR/TTS 链路。

---

## 2. 系统上下文与数据所有权

### 2.1 依赖方向（ARCH_DECISION）

```text
┌──────────┐   HTTPS/WSS    ┌──────────────┐   HTTPS     ┌───────────┐
│  Claw4   │ ──────────────▶│  家庭后端     │───────────▶│ AI Gateway │
│ (设备)    │ ◀─────────────│ (身份/任务/   │◀───────────│ (LLM/ASR/ │
└──────────┘   REST/Event   │  session/统计)│             │ TTS/VLM)  │
                            └──────┬───────┘             └─────┬─────┘
                                   │ SQL                     │ Provider API
                                   ▼                          ▼
                            ┌──────────────┐          ┌──────────────┐
                            │    数据库     │          │ 外部 AI 服务   │
                            └──────────────┘          └──────────────┘
  家长 PWA ──HTTPS──▶ 家庭后端（禁止直连设备）
```

**硬约束（PRODUCT_REQUIRED + ARCH_DECISION）：**

1. **家长端不得直连设备**：PWA 只能经家庭后端读取设备状态与学习数据；设备不得对家长端开放监听端口。
2. **AI Provider 不得由固件直接访问**：设备不持有任何 AI Provider 凭据；所有 AI 调用经家庭后端代理（`AI Gateway`）。
3. **设备 ↔ 后端**：设备只与家庭后端通信；后端是设备唯一可信服务器。

### 2.2 数据权威所有者与设备缓存副本（ARCH_DECISION）

| 数据 | 权威所有者 | 设备侧 | 备注 |
| --- | --- | --- | --- |
| 用户/家长 | 家庭后端（DB） | 无 | 设备不保存家长身份 |
| 儿童档案 | 家庭后端 | 最小子集（child_id 等） | 见第 10 章最小化 |
| 设备凭据 | 家庭后端签发 + 设备安全存储 | device_id + secret | 见第 6.4 节 |
| **Task** | **家庭后端**（家长创建；设备不可创建权威任务） | 只读缓存副本（含今日任务） | 见第 5.1 节 |
| **StudySession** | **家庭后端**（由设备事件落库） | 本地待同步副本（事件队列） | 见第 5.2 节 |
| **Event** | **家庭后端**（幂等去重后入库） | 本地事件队列（权威 pending 状态） | 见第 5.3 节 |
| DeviceConfig | 家庭后端（下发）+ 设备本地默认 | 本地缓存 + 应用内生效值 | 见第 6.5 节 |
| 奖励/积分 | 家庭后端 | 只读展示（MVP 仅 +XP 提示） | `PRODUCT_REQUIRED` |

**设备缓存副本原则（ARCH_DECISION）**：设备只保存"当前会话可离线工作所需的最小副本"；服务器 JSON 不做整块覆盖式状态管理（见 5.4 节）。

---

## 3. 设备侧模块边界

### 3.1 模块总览（ARCH_DECISION）

```text
main/
├── device_services/    适配官方 BSP（网络/存储/电源/音频/显示/输入）
├── learning_domain/    纯业务领域（Task / StudySession / Timer / Reward）★禁止硬件依赖
├── sync/               API client / event queue / offline store / retry / time sync
├── ui/                 只消费状态、发出 intent
├── assistant/          MVP 仅 intent/router 边界
└── telemetry/          结构化日志 + 最小指标
```

### 3.2 `device_services` — 对官方 BSP/网络/存储/电源的适配边界

| 项 | 内容 |
| --- | --- |
| 职责 | 封装官方板级能力：Wi-Fi（经 ESP-Hosted/C5）、显示+触摸、存储、电源/电量、时间（SNTP）、OTA 状态查询（只读） |
| 允许依赖 | 官方 `boards/`、`display/`、`audio/`、ESP-IDF 组件 |
| 禁止依赖 | `learning_domain`（适配层不得反向依赖业务） |
| 输入/输出 | 提供无业务语义的能力接口：`WifiService`、`TimeService`、`StorageService`、`BatteryService` |
| 落点建议 | `main/device_services/`（新建目录，`ARCH_DECISION`），实现时直接调用官方组件（`SOURCE_CONFIRMED` 可复用点） |

> 边界：`device_services` 是**唯一**允许触碰 GPIO/外设驱动/网络栈的层；业务层不得直接访问 GPIO（`PRODUCT_REQUIRED`）。

### 3.3 `learning_domain` — 纯业务领域

| 项 | 内容 |
| --- | --- |
| 职责 | Task 状态、StudySession 生命周期、Timer 计时规则、Reward 规则（MVP 仅 +XP 展示）、领域不变量校验 |
| 允许依赖 | 标准库/数据结构、时间抽象接口（注入） |
| **禁止依赖** | **LVGL、Wi-Fi、GPIO、官方 BSP、FreeRTOS 任务**（保持纯 C++/可单测） |
| 输入/输出 | 纯函数/领域服务接口：`TaskService`、`SessionService`、`TimerService`；事件以领域事件形式输出 |
| 落点建议 | `main/learning_domain/`（新建目录，`ARCH_DECISION`）；对应 `项目总规划` PHASE 2 |

### 3.4 `sync` — 同步与离线

| 项 | 内容 |
| --- | --- |
| 职责 | HTTP API client（HTTPS）、事件队列（顺序/持久化）、离线缓存读取、重试/退避、时间同步（SNTP）协调、幂等键生成 |
| 允许依赖 | `device_services`（网络/存储/时间）、`learning_domain`（读事件语义） |
| 禁止依赖 | LVGL、`ui` |
| 输入/输出 | 入：领域事件、待同步队列；出：HTTP 请求/响应、同步状态（供状态机） |
| 落点建议 | `main/sync/`（新建目录，`ARCH_DECISION`） |

### 3.5 `ui` — 表现层

| 项 | 内容 |
| --- | --- |
| 职责 | Home/Focus/Done/Offline 四页渲染；把用户手势翻译成 **intent**（`StartTask`/`Pause`/`Resume`/`Complete`）；展示状态快照 |
| 允许依赖 | LVGL、`device_services`（显示/输入）、`learning_domain` 的只读状态对象 |
| 禁止依赖 | 直接写服务器数据、直接操作业务真相 |
| 输入/输出 | 入：状态快照；出：intent 消息（不直接调用业务方法） |
| 落点建议 | `main/ui/`（新建目录，`ARCH_DECISION`） |

> UI 原则（`ARCH_DECISION`）：UI 是状态的表现层，不拥有业务真相；状态变化经状态机派发，UI 只渲染。

### 3.6 `assistant` — MVP 仅 intent/router 边界

| 项 | 内容 |
| --- | --- |
| 职责 | 定义语音/命令 → intent 的路由边界（`START_TASK`/`PAUSE_TASK`/`RESUME_TASK`/`COMPLETE_TASK`/`QUERY_TODAY_TASKS`/`QUERY_REMAINING_TIME`） |
| MVP 范围 | **只定义接口与枚举，不接入 LLM/ASR/TTS**（`PRODUCT_REQUIRED`：语音链路不在本任务扩展） |
| 允许依赖 | `learning_domain` intent 定义 |
| 禁止依赖 | 不调用任何 AI Provider |
| 落点建议 | `main/assistant/`（新建目录，`ARCH_DECISION`） |

### 3.7 `telemetry` — 结构化日志与最小指标

| 项 | 内容 |
| --- | --- |
| 职责 | 结构化日志（DEVICE/NETWORK/SYNC/TASK/POWER/ERROR 等 tag）、最小运行指标采集（heap/栈水位/重连计数/API 时延） |
| MVP 范围 | 只写日志，不上传后端（`PRODUCT_REQUIRED`：Telemetry 上传为 V1） |
| 允许依赖 | ESP-IDF 日志、`device_services`（内存/时间） |
| 禁止依赖 | 业务真相、敏感数据（见第 10 章） |
| 落点建议 | `main/telemetry/`（新建目录，`ARCH_DECISION`） |

---

## 4. 状态机与不变量

### 4.1 状态分层（ARCH_DECISION）

设备状态分两层：

1. **设备一级状态**（设备生命周期，与官方 `device_state.h` 枚举对应关系见 4.2）：`BOOTING / PROVISIONING / CONNECTING / ONLINE_IDLE / OFFLINE_IDLE / FOCUSING / PAUSED / SYNCING / LOW_BATTERY / ERROR`
2. **学习会话子状态**（StudySession 内）：`IDLE / RUNNING / PAUSED / COMPLETED / ABORTED`

关系：一级状态 `FOCUSING` 持有子状态 `RUNNING`；`PAUSED` 对应子状态 `PAUSED`。

### 4.2 与官方 DeviceState 的对应（SOURCE_CONFIRMED）

官方 `main/device_state.h` 已有枚举：`kDeviceStateStarting/WifiConfiguring/Idle/Connecting/Listening/Speaking/Upgrading/Activating/AudioTesting/FatalError`（`SOURCE_CONFIRMED`，只读核查）。本架构的学习状态机**在官方枚举之上扩展**（`ARCH_DECISION`）：官方状态继续由 BSP 层维护；学习状态机独立维护业务状态，二者通过映射桥接（例如官方 `kDeviceStateIdle` + 学习 `FOCUSING` 并存）。不得把官方枚举直接替换为学习状态机。

### 4.3 Task 生命周期（ARCH_DECISION）

```text
pending ──▶ ready ──▶ in_progress ──▶ completed
   │         │            │
   │         │            └──▶ paused ──▶ in_progress（恢复）
   │         └──▶ skipped
   └──（后端删除/失效）
```

- 转换规则：`pending→ready` 由后端调度（当日可开始）；`ready→in_progress` 由**用户触发 Start**；`in_progress→completed` 由**用户触发 Complete**（见不变量 I2；Timer 到时/重启恢复均不得自动 completed）。
- 设备重启：走 4.4 的唯一恢复流程——快照完整 → 恢复同一 `session_id`，UI 提示用户选择"继续"或"结束"（用户显式选择结束/保存时 `completion_type=auto_saved`）；快照损坏/不可恢复 → session **只标 `aborted`** 并生成对应事件。`auto_saved` 与 `aborted` 均**不得自动完成 Task**（不变量 I2）。

### 4.4 StudySession 生命周期（ARCH_DECISION）

```text
CREATED(开始) ──▶ RUNNING ──▶ COMPLETED(显式结束)
   │                 │
   │                 └──▶ PAUSED ──▶ RUNNING（恢复，pause_count+1）
   └──▶ ABORTED（快照损坏/不可恢复，仅 `aborted`；不产生 auto_saved）
```

- **一个 Task 可产生多个 StudySession**（Task ≠ StudySession，不变量 I1）。
- **状态与完成方式分离**：`status`（`created/running/paused/completed/aborted`）描述 session 生命周期；`completion_type` 描述 session **如何结束**，**不替代** `status`。
- `completion_type`：`normal`（用户正常结束）/`manual`（用户手动结束）/`auto_saved`（快照完整、重启恢复同一 session 后**由用户显式选择"保存/结束"**时保留已累计时长）/`aborted`（快照损坏，仅保留可恢复元数据）。**无 `timeout` 值**：专注段到时（Timer 到 0）仅结束/暂停当前**专注段**并提示用户（session 保持 `RUNNING`/`PAUSED` 待用户决策：继续新专注段或结束），**不得自动**把 session 或 Task 标为 `completed`（不变量 I2）。`parent_confirmed` 为 V1 家长确认，MVP 不实现。
- **对 Task 的影响**：`task.completed` 只由显式孩子操作（未来可含家长）生成；任何 completion_type（含 `auto_saved`/`aborted`）都不自动完成 Task。

### 4.5 异常路径（ARCH_DECISION）

| 场景 | 行为 |
| --- | --- |
| 暂停/恢复 | 仅子状态在 `RUNNING↔PAUSED` 间切换；Timer 停止/继续；事件 `task.paused`/`task.resumed` 入队 |
| 重复点击完成 | 幂等：第二次 Complete 被领域层拒绝（状态已非 `in_progress`），不发重复事件 |
| 设备重启（专注中） | 快照完整 → 恢复同一 `session_id`，UI 提示用户选择"继续"或"结束"（结束时 `completion_type=auto_saved` 保留已累计时长）；快照损坏 → session 标 `aborted` + 对应事件，**Task 不自动 `completed`** |
| 专注段到时（Timer 到 0） | 仅结束/暂停当前专注段并提示用户（session 保持 `RUNNING`/`PAUSED` 待决策）；**不得自动**把 Task 标为 `completed`（I2） |
| 断网 | 本地继续（第 7 章），状态机进入 `OFFLINE_IDLE`/保持 `FOCUSING` |
| 时间未同步 | 时间戳用本地 RTC + 单调时钟；同步状态标记 `time_unsynced`，后端按到达时间处理（见 5.3 时间语义） |
| 后端拒绝（4xx/5xx） | 4xx 业务拒绝 → 关键事件入死信区持久保留（同一 `event_id` 重放/人工补偿，7.3.3）；5xx/网络类 → 可重试退避（6.7）；`sync.failed` 为本地可合并诊断 |
| 队列满 | 触发同步；压力下先合并/丢弃非关键 telemetry 腾容量，**关键业务事件（完成类）永不丢弃**；仍无法持久化 → 按 outbox 规则不提交完成状态并显示可恢复错误（第 7.3 节） |

### 4.6 不变量清单（ARCH_DECISION，实现必须保持）

| # | 不变量 |
| --- | --- |
| I1 | **Task ≠ StudySession**：Task 是计划实体，StudySession 是实际发生的学习行为；一个 Task 可对应多个 Session |
| I2 | **AI 不得完成任务**：`completed` 只能由孩子（或未来家长）触发；AI 输出不得写该状态 |
| I3 | **完成事件不可丢**：`study.session.completed`/`task.completed` 一旦入队，必须至少成功投递一次（at-least-once） |
| I4 | **UI 不直接改服务器真相**：UI 只发 intent；数据变更必须走领域服务 → 事件 → 同步链路 |
| I5 | **业务层不直接访问 GPIO**：只有 `device_services` 触碰硬件 |
| I6 | **设备不直连 AI Provider**；家长端不直连设备 |
| I7 | **事件具有单调顺序**：同一设备 `sequence` 严格递增 |
| I8 | **关键事件原子持久化（outbox）**：领域状态提交与对应事件持久化在同一原子顺序内完成；事件无法持久化时状态不得提交（见 7.3.1） |

---

## 5. 数据契约

> 以下 JSON 示例为**契约草案**（`ARCH_DECISION`），用于指导后续单元测试与 Mock Backend；不含任何真实数据。所有时间字段语义见 5.4。

### 5.1 Task（`ARCH_DECISION`）

```json
{
  "task_id": "uuid-string",
  "child_id": "uuid-string",
  "title": "完成练习册 P32",
  "subject": "math",
  "description": "课本第 3 章练习册 P32",
  "task_type": "practice",
  "estimated_minutes": 25,
  "priority": "high",
  "status": "ready",
  "scheduled_date": "2026-09-01",
  "scheduled_start": "2026-09-01T16:00:00+08:00",
  "deadline": "2026-09-01T21:00:00+08:00",
  "source": "parent",
  "parent_created": true,
  "ai_created": false,
  "version": 3,
  "created_at": "2026-08-31T10:00:00+08:00",
  "updated_at": "2026-09-01T08:00:00+08:00"
}
```

字段语义（MVP 必填性）：

| 字段 | 类型 | 必填 | 语义 |
| --- | --- | --- | --- |
| task_id | string(uuid) | 是 | 全局唯一 |
| child_id | string(uuid) | 是 | 归属儿童 |
| title | string | 是 | 任务名 |
| subject | enum | 否 | math/chinese/english/science/other |
| status | enum | 是 | pending/ready/in_progress/paused/completed/skipped |
| estimated_minutes | int | 是 | 计划时长（分钟） |
| priority | enum | 否 | high/medium/low |
| scheduled_date | date | 是 | 排期日（本地时区） |
| version | int | 是 | 乐观并发版本（冲突策略见 5.4） |

### 5.2 StudySession（`ARCH_DECISION`）

```json
{
  "session_id": "uuid-string",
  "task_id": "uuid-string",
  "child_id": "uuid-string",
  "device_id": "uuid-string",
  "planned_minutes": 25,
  "actual_seconds": 1500,
  "started_at": "2026-09-01T16:02:10+08:00",
  "ended_at": "2026-09-01T16:27:10+08:00",
  "pause_count": 1,
  "pause_seconds": 90,
  "status": "completed",
  "completion_type": "normal",
  "created_at": "2026-09-01T16:02:10+08:00",
  "updated_at": "2026-09-01T16:27:10+08:00"
}
```

- `status`：`created/running/paused/completed/aborted`——描述 session 生命周期；`completion_type` 是另一维度（如何结束），**不替代** `status`。
- `actual_seconds`：**由设备单调时钟累计**（不含暂停时间）；`pause_seconds` 由暂停段累计。
- `completion_type`：`normal/manual/auto_saved/aborted`（**无 `timeout`**；专注段到时仅结束/暂停专注段并提示用户，见 4.4/4.5）；`parent_confirmed` 为 V1，MVP 不含。

### 5.3 统一 Event envelope（`ARCH_DECISION`）

```json
{
  "event_id": "uuid-string",
  "device_id": "uuid-string",
  "child_id": "uuid-string",
  "sequence": 42,
  "timestamp": 1756718530,
  "timestamp_source": "rtc",
  "type": "study.session.completed",
  "version": 1,
  "payload": {
    "session_id": "uuid-string",
    "task_id": "uuid-string",
    "actual_seconds": 1500
  }
}
```

- `event_id`：客户端生成 uuid，**幂等键**（后端去重，见 5.4）。
- `sequence`：设备单调递增（**持久化计数**，重启不重置，见第 7 章）。
- `timestamp`：unix epoch 秒；`timestamp_source`：`rtc`（已 SNTP 同步）/`local`（未同步）。
- `type` 枚举（MVP 子集）：`device.booted`、`device.online`、`device.offline`、`task.started`、`task.paused`、`task.resumed`、`task.completed`、`task.skipped`、`study.session.started`、`study.session.completed`、`sync.failed`、`sync.recovered`。
- `version`：payload schema 版本（当前 1）。

### 5.4 一致性策略（`ARCH_DECISION`）

| 策略 | 规则 |
| --- | --- |
| **ACK 定义** | `last_acked_sequence` = **最高连续、已持久化且业务处理成功的 sequence**；**不得**以批次内最大成功序号代替（若 seq 42 成功而 41 失败，ACK 停在 40 及之前的连续前缀） |
| **逐事件处理顺序（MVP）** | 每个事件按固定顺序处理：① 认证与归属校验（6.4.1，失败 → `403`/`rejected`）→ ② **`(device_id, event_id)` 幂等查重**（已存在 → 走"重复事件处理"分支，**不再**检查 sequence 连续性）→ ③ 仅对**未见过的新事件**执行 `sequence` 连续性检查。此顺序保证"服务端已落库但响应丢失后重发"场景返回 `duplicate` 而非 `rejected` |
| **幂等** | 后端按 `(device_id, event_id)` 去重：已存在且**关键不可变字段与载荷摘要一致** → 返回成功语义（逐事件标 `duplicate`）并返回**当前连续 ACK**，不重复落库；已存在但 sequence/type/载荷摘要**不同** → 返回 `conflict`（拒绝 + 告警），不得伪装成 `duplicate` |
| **顺序** | 同设备按 `sequence` 升序处理；断点续传时设备只从 `last_acked_sequence+1` 开始补发（见 7.4） |
| **服务端事务（MVP）** | MVP 采用**单设备串行批次**：同设备批次按序处理；新事件校验 `sequence == last_acked+1` 才可处理；`sequence` 大于期望值 → 返回 `gap`（不处理其后事件）；`sequence` ≤ ACK 且 `event_id` 未见过 → 返回 `rejected`（回退/冲突），**不推进 ACK**；多批并发由后端按设备串行化（设备级锁），避免交叉乱序 |
| **投递语义** | at-least-once（`PRODUCT_REQUIRED`）；允许重复，靠幂等收敛 |
| **重复事件处理** | 已存在 `event_id` 且摘要一致 → `duplicate`（含当前连续 ACK）；摘要不一致 → `conflict` 拒绝并告警；新事件 `sequence` 跳号 → `gap`；`sequence` 回退 → `rejected`。客户端保留非 accepted/duplicate 的 pending 事件不删除 |
| **冲突策略** | Task 采用 `version` 乐观锁：设备提交依赖 `updated_at`/`version` 的变更；版本不符 → `409`，设备丢弃本地编辑并重新拉取（MVP 设备不改 Task，冲突概率低） |
| **状态覆盖禁令** | 设备不得用"整份服务器 JSON 覆盖本地状态"；Task 缓存更新按 `(task_id, version)` 增量合并，本地运行态（session/计时）不受远端快照影响 |

**可测试示例：服务端落库成功但响应丢失后的重发**（`ARCH_DECISION`，CR-WB002-06）

```text
1. 设备提交 seq=42（event_id=E1，type=study.session.completed）→ 服务端已持久化并推进 ACK=42，
   但响应在传输中丢失，设备端仍认为 E1 pending。
2. 设备以同一 event_id=E1、同一载荷重发 seq=42 → 服务端先做幂等查重：命中已存在记录，
   且关键字段/载荷摘要一致 → 返回 status=duplicate、last_acked_sequence=42。
3. 设备按"两条件"规则（status∈{accepted,duplicate} 且 sequence<=ACK）删除 E1，不产生新落库。
4. 若重发时 event_id=E1 但 payload 被篡改（摘要不同）→ 返回 conflict 并告警，设备保留 pending 待人工/修复处理。
```

**时间语义**：`timestamp_source=rtc` 才可用于统计；`local` 事件到达后端后由后端补 `server_received_at` 并标记；MVP 不要求设备时钟精确同步即可工作（`PRODUCT_REQUIRED`：断网/未同步也能开始学习）。

---

## 6. API 与同步协议边界

### 6.1 API 前缀与传输（ARCH_DECISION）

- 基础路径：`https://<backend>/api/v1`
- 传输：HTTPS（REST）+ WSS（设备实时通道，MVP 可选，第 6.6 节）
- 所有 JSON 均带 `Content-Type: application/json; charset=utf-8`

### 6.2 MVP 必需端点（ARCH_DECISION）

| 方法 | 路径 | 用途 |
| --- | --- | --- |
| POST | `/devices/register` | 设备注册（首次；服务端签发 device_id，见 6.4） |
| POST | `/devices/challenge` | **获取一次性 nonce/challenge**（认证第一步，见 6.4.3） |
| POST | `/devices/claim` | **家长配对/绑定**：已认证家长会话调用，建立 device ↔ parent/child 绑定（6.4.2） |
| POST | `/devices/auth` | 设备凭据 + challenge 签名换取短期访问 token（两阶段，见 6.4.3） |
| GET | `/devices/{device_id}/config` | 设备配置（含功能开关） |
| POST | `/devices/{device_id}/heartbeat` | 心跳 + 状态上报（在线/电量/固件） |
| GET | `/children/{child_id}/tasks/today` | 今日任务（后端校验 device-token 与 child 绑定） |
| GET | `/tasks/{task_id}` | 单个任务 |
| POST | `/events/batch` | **事件批量同步——设备业务写入的唯一权威入口（MVP）** |
| POST | `/sync` | 同步协商（`last_acked_sequence` 等，可选简化版） |

> **写入唯一入口（`ARCH_DECISION` + CR-WB002-04）**：MVP 中设备的**一切业务事实写入**（session 开始/结束、任务完成/跳过等）一律经 `/events/batch`；`/study-sessions/{id}/finish` **不属于设备 MVP 接口**（保留为 PWA/后端内部预留），设备不得调用，避免双重落账。`GET /sync`（项目总规划建议）MVP 可合并进 `/events/batch` 响应（返回 `last_acked_sequence` 与配置），`ARCH_DECISION` 允许收敛。

### 6.3 最小 JSON 示例（不含真实凭据/儿童数据）

**最小身份流程**（`ARCH_DECISION`，CR-WB002-04/07/08）：**注册 → 家长配对/claim → 设备 challenge → 设备认证 → 短期 token**。设备身份不得仅信任客户端自报字符串；`device_id` 由服务端签发。

**POST /devices/register** 请求（`ARCH_DECISION`；设备提供硬件安装标识，不自报 device_id）

```json
{
  "installation_id": "uuid-or-serial-string",
  "model": "metalio-claw-4",
  "fw_version": "2.0.51"
}
```

**POST /devices/register** 响应 `201`（`ARCH_DECISION`；服务端签发 device_id + 设备密钥 + 一次性配对码）

```json
{
  "device_id": "00000000-0000-0000-0000-000000000001",
  "device_secret": "opaque-secret-stored-in-secure-storage",
  "pairing_code": "A1B2C3",
  "pairing_code_expires_in": 900
}
```

**POST /devices/challenge** 请求（`ARCH_DECISION`；设备发起认证第一步，用 device_id 定位）

```json
{
  "device_id": "00000000-0000-0000-0000-000000000001"
}
```

**POST /devices/challenge** 响应 `200`（`ARCH_DECISION`；一次性 challenge_id + nonce + 过期时间）

```json
{
  "challenge_id": "cccccccc-1111-2222-3333-444455556666",
  "nonce": "8f14e45fceea167a5a36dedd4bea2543",
  "expires_at": 1756718590
}
```

**POST /devices/claim** 请求（`ARCH_DECISION`；**由已认证家长会话/家长 token 调用**，`parent_id` 从服务端认证上下文派生，不接受请求体自报；携带配对码与目标儿童）

```json
{
  "pairing_code": "A1B2C3",
  "child_id": "00000000-0000-0000-0000-000000000003"
}
```

**POST /devices/auth** 请求（`ARCH_DECISION`；设备用 device_secret 对 challenge 签名，签名绑定 device_id + challenge_id + nonce）

```json
{
  "device_id": "00000000-0000-0000-0000-000000000001",
  "challenge_id": "cccccccc-1111-2222-3333-444455556666",
  "nonce": "8f14e45fceea167a5a36dedd4bea2543",
  "challenge_signature": "base64(hmac-sha256(device_secret, device_id|challenge_id|nonce))"
}
```

> challenge/nonce 规则见 6.4.3：一次性、短 TTL、单次使用、重放返回 401，签名原文/secret/nonce 不得记入日志；注册/配对失败统一返回通用错误，**不泄露儿童是否存在**。

**GET /children/{child_id}/tasks/today**（响应）

```json
{
  "date": "2026-09-01",
  "tasks": [
    {
      "task_id": "00000000-0000-0000-0000-000000000002",
      "title": "完成练习册 P32",
      "subject": "math",
      "estimated_minutes": 25,
      "priority": "high",
      "status": "ready",
      "scheduled_date": "2026-09-01",
      "version": 3
    }
  ]
}
```

**POST /events/batch** 请求（核心契约，`ARCH_DECISION`）

```json
{
  "device_id": "00000000-0000-0000-0000-000000000001",
  "last_acked_sequence": 41,
  "events": [
    {
      "event_id": "aaaaaaaa-bbbb-cccc-dddd-eeeeffff0001",
      "device_id": "00000000-0000-0000-0000-000000000001",
      "child_id": "00000000-0000-0000-0000-000000000003",
      "sequence": 42,
      "timestamp": 1756718530,
      "timestamp_source": "rtc",
      "type": "study.session.completed",
      "version": 1,
      "payload": {
        "session_id": "11111111-2222-3333-4444-555566667777",
        "task_id": "00000000-0000-0000-0000-000000000002",
        "actual_seconds": 1500
      }
    }
  ]
}
```

**POST /events/batch** 响应 `200`（`ARCH_DECISION`）

```json
{
  "last_acked_sequence": 42,
  "server_time": 1756718535,
  "results": [
    {
      "sequence": 42,
      "event_id": "aaaaaaaa-bbbb-cccc-dddd-eeeeffff0001",
      "status": "accepted",
      "http_status": 200
    }
  ],
  "accepted": 1,
  "duplicates": 0,
  "rejected": [],
  "gaps": []
}
```

- `results[]`：**逐事件结果**（`accepted`/`duplicate`/`rejected`）；`rejected`/`gaps` 可并入 `results` 或独立列出。
- 客户端**只删除**同时满足 ① `status ∈ {accepted, duplicate}` 且 ② `sequence <= last_acked_sequence` 的 pending 事件；`rejected`、落在 gap 之后的事件一律保留，等待修复重放（同一 `event_id`）或人工补偿（7.3.3）。

### 6.4 安全传输与凭据（PRODUCT_REQUIRED + ARCH_DECISION）

| 项 | 规则 |
| --- | --- |
| 传输 | 设备↔后端、PWA↔后端一律 HTTPS；实时通道 WSS |
| 证书校验 | 必须校验 CA 链 + hostname；**禁止 `verify=false`** |
| 凭据 | **每设备独立凭据**（device_id + 设备密钥/secret）；**禁止所有设备共用永久 API Key**；AI Provider 凭据只存后端 |
| token | 后端签发**短期 token**（建议 ≤ 24h 可刷新）；**token 声明必须携带允许的 `device_id` 与 `child_id` 绑定集合**（见 6.4.1）；设备侧安全存储（NVS/安全分区） |
| 刷新 | token 失效 → 用设备凭据重新 auth（失败矩阵见第 10 章） |
| 正式版规划 | Secure Boot / Flash Encryption / Signed OTA（`PRODUCT_REQUIRED` 远期；当前 sdkconfig 未见使能，`SOURCE_CONFIRMED` 未确认） |

#### 6.4.1 设备—儿童绑定与逐请求校验（`ARCH_DECISION`）

- 绑定关系：`device ↔ parent ↔ child`（MVP 单家长、单设备可绑定一个或多个儿童；多家长细节待定，见 12.3 P5）。
- token/服务端会话**必须**携带允许的 `device_id` + `child_id` 集合；后端对**每个请求**（任务查询、事件、心跳）重新校验绑定：
  - 请求路径/载荷中的 `child_id` 必须 ∈ token 绑定集合，否则 `403`（拒绝冒充其他设备/儿童）；
  - `/events/batch` 中**每个事件**的 `child_id` 逐一校验；绑定不符的事件返回 `rejected` 且保持 pending（7.3.3）。
- 设备缓存的最小儿童子集（`child_id` 等）只来自服务端下发的绑定结果，不接受客户端自报。

#### 6.4.2 配对/claim 规则（`ARCH_DECISION`）

- **`/devices/claim` 必须由已认证家长会话/短期家长 token 调用**；`parent_id` 一律从服务端认证上下文派生，**不接受请求体自报**（CR-WB002-08）。
- 服务端必须校验目标 `child_id` 属于当前已认证家长的授权范围，才可建立 device ↔ parent ↔ child 绑定；不属于 → 拒绝（通用外部错误，见下）。
- 注册响应携带**一次性配对码**（短时效，建议 ≤ 15 分钟）；家长在时效内输入配对码与目标儿童完成 `claim`。
- 配对码：单次有效、短 TTL、防暴力尝试（限速/退避）；`claim` 成功后作废；设备 `device_secret` 仅在注册响应返回一次，设备侧安全存储。
- **未授权儿童、无效配对码与不存在对象统一返回通用外部错误**（不区分"儿童不存在"/"配对码错误"，**不泄露儿童是否存在**）；服务端内部保留脱敏审计原因。
- 未配对设备的业务能力：仅可心跳/拉配置；**不得**拉取任务、提交事件（拒绝绑定缺失）。

#### 6.4.3 challenge/nonce 防重放（`ARCH_DECISION`，CR-WB002-07）

设备认证为**两阶段交互**：

1. **获取 challenge**：`POST /devices/challenge`（请求 `device_id`）→ 响应 `challenge_id` + 一次性 `nonce` + `expires_at`（短 TTL，建议 5 分钟）。
2. **认证**：`POST /devices/auth` 携带 `device_id`、`challenge_id`、`nonce` 与 `challenge_signature`。

规则：

- **签名绑定**：`challenge_signature = sign(device_secret, device_id | challenge_id | nonce)`（MVP 建议 HMAC-SHA256，`ARCH_DECISION`）；绑定 device_id 与 challenge_id，避免跨设备/跨 challenge 复用。
- **单次使用**：challenge 只能使用一次；`auth` 成功后**立即作废**；重复使用同一 challenge → `401`。
- **过期**：`expires_at` 过后 → `401`；设备重新发起 challenge。
- **防重放**：服务端缓存已用/已过期 challenge_id，重放 → `401`。
- **日志脱敏**：**不得在日志中记录 nonce、device_secret、challenge_signature 原文**（10.2）。
- **注册/配对/认证失败统一返回通用错误**（不区分"儿童不存在"/"配对码错误"/"设备不存在"），**不得泄露儿童是否存在**。

### 6.5 配置与心跳（ARCH_DECISION）

- `GET /devices/{id}/config` 返回：功能开关（摄像头 enabled=false、语音 enabled=false）、同步间隔、任务拉取间隔、电量上报阈值。
- `POST /devices/{id}/heartbeat`：设备在线状态、电量（若可得，`HARDWARE_VERIFY_REQUIRED`）、固件版本、`last_acked_sequence`。
- 配置变更后设备增量应用，**不整包覆盖本地运行态**。

### 6.6 WSS 通道（可选，ARCH_DECISION）

- MVP 可用"轮询拉取任务 + 批量事件"替代实时通道；WSS 仅在后端重启推送/家长即时反馈需要时引入。
- 若引入：使用与 REST 相同的 token；断线退避（第 7.5 节），禁止无限重连风暴。

### 6.7 状态码与重试分类（ARCH_DECISION）

| 状态码类别 | 处理 |
| --- | --- |
| 2xx | 成功；按**连续前缀**推进 `last_acked_sequence`（逐事件结果见 6.3） |
| 401/403 | **认证/授权失败，不是单事件死信条件**：刷新凭据重新 auth → 重试一次；仍失败（含重新配对失败）→ **暂停同步**，业务事件保持 pending 不动（7.3.3/10.3） |
| 404 | 设备未注册/任务不存在 → 重新 register 或拉取任务；同步事件保持 pending，按 4xx 处理（见下） |
| 409 | 版本冲突 → 丢弃本地编辑，重新拉取 |
| 4xx（业务拒绝） | 关键事件**持久保留原文**（死信区，不无限重试、不删除、不视为 ACK），**以同一 `event_id` 修复后重放**或进入明确人工补偿流程（7.3.3） |
| 5xx/网络/DNS/超时 | **可重试**：指数退避 + 抖动，上限后暂停（第 7.5 节） |

---

## 7. 离线与持久化策略

### 7.1 离线能力（PRODUCT_REQUIRED）

断网时设备仍可：查看缓存今日任务、开始任务、专注计时、暂停、恢复、完成、记录 StudySession、获得本地反馈（+XP 提示）。所有行为产生事件入队，网络恢复自动同步。

### 7.2 持久化落点（ARCH_DECISION，不修改分区表）

- **任务缓存**：NVS（键值）或既有 FAT 分区（`system`/`storage`，`SOURCE_CONFIRMED` 存在）；具体实现选择列 `ARCH_DECISION`，**MVP 不把 microSD 作为强依赖**（`SOURCE_CONFIRMED`：SD 热插拔 UNKNOWN）。
- **事件队列**：NVS（小体积、掉电安全）优先；容量策略见 7.3。
- **不做**：新增分区、修改分区表、依赖 microSD 存在性。

### 7.3 事件队列规则（ARCH_DECISION）

| 规则 | 说明 |
| --- | --- |
| 写入顺序 | 按 `sequence` 追加；`sequence` 计数器持久化（掉电恢复后继续） |
| **原子持久化（outbox）** | **验证 intent → 同一原子顺序持久化"领域快照变更 + 对应事件" → 提交成功后才向 UI 确认状态变化**（详见 7.3.1） |
| 确认删除 | 仅删除满足**两者**的 pending 事件：① 后端逐事件返回明确成功/重复；② 位于**连续 ACK 前缀**内（`sequence <= last_acked_sequence`）。未获逐事件成功或落在 gap 之后的 pending 事件一律保留（at-least-once 收敛） |
| 掉电恢复 | 启动时重放未 ack 事件；NVS 写入采用"先写日志/标记、后提交"的幂等落法，避免半写 |
| 容量上限 | MVP 队列上限建议 200 条（`ARCH_DECISION`，约 KB 级）；**为完成类关键事件预留容量**：压力下先合并/丢弃非关键 telemetry，**不得隔离或丢弃关键业务事件**（见 7.3.2 与失败矩阵 10.3） |
| 退避 | 指数退避 + 抖动（建议 1s→2s→4s→…→上限 60s） |
| 死信 | **仅业务性 4xx**（校验/语义类拒绝，非 401/403）关键事件持久保留原文 + 拒绝摘要，**以同一 `event_id` 修复后重放**或进入明确人工补偿流程，**不得视为已 ACK、不得删除**（详见 7.3.3） |

#### 7.3.1 原子持久化（transactional outbox，`ARCH_DECISION`）

每次业务变更按以下顺序执行，任何一步失败都不得进入下一步：

1. **验证 intent**：领域层校验状态机允许该转换（如 `Complete` 仅在 `in_progress`/`paused` 有效）。
2. **持久化快照 + 事件**：在同一原子写序列（NVS 日志式提交/等价事务）内写入"新领域快照"与"对应事件"（含 `event_id`、`sequence`）。
3. **提交成功后确认**：两步都持久化成功，才向 UI 发布状态变更；UI 才显示完成/继续等反馈。

- 关键事件（`task.completed`、`study.session.completed`、`task.started` 等）无法持久化 → 领域状态**不提交** `completed`；UI 显示可恢复错误（重试按钮）；重试**复用原 `event_id`**，绝不生成新 `event_id`。
- 由此保证不变量 I3/I8：完成事实要么已持久化（可重放），要么未提交（不算完成），不存在"状态完成但事件丢失"的中间态。

#### 7.3.2 容量策略与关键事件优先（`ARCH_DECISION`）

- 事件分级：**关键**（`task.completed`/`study.session.completed`/`task.started`/`task.skipped` 等业务事实）与**非关键**（telemetry、可合并诊断）。
- 队列压力下优先动作：合并/丢弃**非关键**事件腾出容量；**关键事件永不隔离、永不丢弃**。
- 若腾容后仍无法写入关键事件（存储故障等），按 7.3.1 处理：不提交完成状态，UI 显示可恢复错误。

#### 7.3.3 死信、诊断事件与递归抑制（`ARCH_DECISION`）

- 死信区仅收**业务性 4xx**（校验/语义拒绝）关键事件：保留原文 + 拒绝响应摘要；后续**以同一 `event_id`** 重放（修复后）或进入明确的人工补偿流程；**不得删除、不得视为已 ACK**。
- **401/403 不是死信条件**：是认证/授权失败，业务事件保持 pending；重新 auth 失败（含重新配对失败）→ **暂停同步**（见 6.7/10.3），队列事件不动。
- `sync.failed`/`sync.recovered` 是**本地可合并诊断**（只保留最新一条状态），**不入业务队列、不消耗队列容量**，避免在已满业务队列中递归制造更多事件；可选独立受限通道承载。

### 7.4 断点续传（ARCH_DECISION）

设备记录 `last_acked_sequence`（连续前缀）；同步时携带该值，后端从下一序号继续。重复投递由 `event_id` 幂等吸收；未获逐事件成功或落在 gap 之后的 pending 事件保留重发（5.4/7.3）。

### 7.5 网络恢复（ARCH_DECISION）

- 恢复信号：Wi-Fi 重连成功（`device_services` 上报）+ SNTP 重新同步。
- 恢复后：先 `auth` → 拉今日任务（增量）→ 批量同步事件 → 心跳。
- 重连风暴防护：全链路退避上限、每次启动最多 N 次无网络探测（建议 ≤3）。

---

## 8. UI、并发与性能边界

### 8.1 页面与 intent 流转（ARCH_DECISION）

```text
HOME（今日任务列表）
  │ StartTask
  ▼
FOCUS（倒计时/暂停/完成）
  │ Pause ──▶ PAUSED 视图（Resume）
  │ Complete ──▶ Done 动画（+XP）
  ▼
DONE（反馈页：任务完成、实际时长）
OFFLINE（断网提示：缓存任务可用、事件待同步）
```

- 页面只发 intent；状态机决定页面切换（`ARCH_DECISION`）。

### 8.2 并发与线程归属（ARCH_DECISION）

| 规则 | 说明 |
| --- | --- |
| LVGL 线程 | 渲染只在 LVGL 任务内执行；**其他任务不得直接调用 LVGL API** |
| 跨任务消息 | 一律经消息队列/事件组传递（FreeRTOS queue/event group），禁止共享可变状态裸读写 |
| 定时器 | Timer 属 `learning_domain`，回调只发领域事件；UI 定时器只做渲染刷新 |
| 网络回调 | 网络/同步回调不得操作 UI；仅向状态机/UI 消息队列投递 |
| 状态真相 | 状态机是唯一状态权威；UI 持有快照引用（只读） |

### 8.3 性能预算（DEVICE_LOG_CONFIRMED + ARCH_DECISION）

已确认设备事实（`DEVICE_LOG_CONFIRMED`）：

| 项 | 值 | 证据 |
| --- | --- | --- |
| CPU | 双核 360 MHz | `cpu freq: 360000000 Hz` |
| PSRAM | 32 MB @ 200 MHz | `Found 32MB PSRAM device / Speed: 200MHz` |
| Flash | GD 芯片 qio（容量 UNKNOWN） | `spi_flash: detected chip: gd / flash io: qio` |
| 固件 | xiaozhi v2.0.51（compile 2026-08-18，ELF `fec753506`） | `app_init` 行 |

**预算（ARCH_DECISION，目标值不是实测结论）**：

| 指标 | 目标 | 状态 |
| --- | --- | --- |
| 正常 UI FPS | ≥ 30 | 未实测（`HARDWARE_VERIFY_REQUIRED`） |
| 重负载 FPS | ≥ 15 | 未实测 |
| Touch feedback P95 | < 100 ms | 未实测（触摸控制器实机 UNKNOWN） |
| Wi-Fi 恢复 | < 5 s | 未实测 |
| 24h 无 crash | — | 未验证 |
| PSRAM 稳态余量 | ≥ 6 MB | 未实测 |

**约束（`PRODUCT_REQUIRED` + `ARCH_DECISION`）**：MVP 不做全屏 alpha blend/粒子动画；卡片+局部动画+静态图标优先；不硬编码 480 MHz（以实机频率读取为准）。

> 屏驱 SKU（NV3051F/FL7707N）、触摸、Flash 容量、PSRAM 实际分配量均保持 `HARDWARE_VERIFY_REQUIRED`/`UNKNOWN`，本文不写成实机已确认（继承 WB-001/WB-HW 证据边界）。

### 8.4 后续可测指标（ARCH_DECISION，未达成前不宣称）

FPS 采样、触摸事件延迟 P95、事件队列耗时、HTTP 时延、堆/PSRAM 水位、WebSocket 重连计数、24h 运行时长。

---

## 9. 后端/PWA/AI 边界

> 本章全部为 `ARCH_DECISION`（推荐目录与组件职责），**未实现**；不得将推荐结构当作现有代码。

### 9.1 家庭后端职责（ARCH_DECISION）

- 身份：用户、家长-儿童关系、设备注册与认证（签发短期 token）、**device↔parent↔child 绑定与逐请求/逐事件校验**（见 6.4.1）。
- 业务：Task 管理（家长创建）、今日任务下发、StudySession 落账、事件接收与幂等去重（`/events/batch` 为设备写入唯一入口）、统计（MVP：完成数/专注分钟/完成率）。
- 权限：家长对儿童/设备的读写权限（MVP 单家长边界；多家长见 12.3 P5）。
- AI 编排：作为唯一调用方调用 AI Gateway（MVP 阶段 AI 调用可空跑/桩实现）。

推荐目录（`ARCH_DECISION`，来自 `项目总规划` §二十五，未实现）：

```text
backend/
  app/api/          # 路由
  app/models/       # ORM 模型
  app/schemas/      # Pydantic 契约
  app/services/     # 业务服务
  app/repositories/ # 数据访问
  app/ai/           # AI Gateway 客户端
  app/device/       # 设备注册/认证/心跳
  app/sync/         # 事件批处理/幂等
  app/websocket/    # WSS（可选）
  app/core/         # 配置/安全/日志
  migrations/
  tests/
```

推荐技术栈（`ARCH_DECISION`，`PRODUCT_REQUIRED` 允许）：FastAPI + PostgreSQL +（可选 Redis）+ WebSocket + SQLAlchemy/SQLModel + Alembic + Pydantic + Docker Compose。**本文不锁定具体版本**；若后续需在 Python/Go 等之间选择且现有事实不足，按停止条件上报 Codex。

### 9.2 数据库最低模型（ARCH_DECISION）

`users`、`children`、`devices`、`parent_child`、`tasks`、`study_sessions`、`events`（幂等去重表）、`rewards`、`device_configs`。（后期：`habits`、`plans`、`reminders`、`ai_conversations`、`media`。）

### 9.3 家长 PWA MVP（ARCH_DECISION）

只覆盖 4 页：**Dashboard**（今日计划/完成/完成率/专注分钟/是否正在学习）、**今日任务**（创建任务：科目/任务/预计分钟/优先级）、**学习记录**（时间/任务/用时/暂停/完成状态）、**设备**（在线/电量/固件版本/最后同步）。推荐 React + TypeScript + Vite + PWA（`ARCH_DECISION`）。

### 9.4 AI Gateway 与语音边界（ARCH_DECISION）

- 定义统一 Provider 接口：`LLMProvider`/`ASRProvider`/`TTSProvider`/`VisionProvider`（`PRODUCT_REQUIRED`）；实现可插拔（ollama/openai/deepseek/custom）。
- **MVP：Gateway 只保留接口与配置边界**；`/assistant/query` 可返回"未启用"占位；语音（唤醒词/VAD/AEC/ASR/TTS）不在本任务扩展（`PRODUCT_REQUIRED`）。
- 设备侧 `assistant` 模块只定义 intent 枚举与 router 边界（第 3.6 节）。

---

## 10. 隐私、安全、可观测性与失败矩阵

### 10.1 儿童数据最小化（PRODUCT_REQUIRED）

- 默认**不持续录像、不持续上传麦克风**、不做人脸/情绪识别。
- 语音数据仅在用户主动触发 AI 对话时上传（MVP 无语音 → 零上传）。
- 图片仅在用户主动拍摄时上传（MVP 摄像头关闭 → 零上传）。
- 语音原文件默认不永久保存；图片设 TTL（V1 落实）；家长可查看/删除/关闭摄像头与 AI 功能。
- 设备只保存最小儿童子集（child_id 等），不保存家庭关系/家长凭据。

### 10.2 日志脱敏与禁止记录（PRODUCT_REQUIRED + ARCH_DECISION）

**结构化日志字段**（设备）：`DEVICE`/`NETWORK`/`SYNC`/`TASK`/`POWER`/`ERROR` 等 tag；`request_id`、`device_id`、`child_id`、`event_id`、`sequence`、HTTP 状态码。

**禁止记录**：API Key、token、密码、SSID/密码、完整语音、儿童姓名/照片/位置、其他设备凭据。

### 10.3 失败矩阵（ARCH_DECISION，覆盖任务包要求）

| 场景 | 预期行为 |
| --- | --- |
| 无任务 | Home 显示"今日无任务"；不发事件；正常心跳 |
| 断网 | 进入离线模式；缓存任务可开始；事件入队；`device.offline` 事件（若可发则发） |
| DNS 错误 | 归为网络类可重试；退避；不产生业务错误 |
| token 失效 | 401/403 → 设备凭据重新 auth → 重试一次；仍失败 → **暂停同步**（业务事件保持 pending，不进死信；见 6.7/7.3.3） |
| API 500 | 可重试（退避）；事件不删除 |
| 重复事件 | 后端幂等忽略（逐事件返回重复）；设备端重复点击被领域层拒绝（不变量 I7 下无重复 sequence） |
| 队列满 | 触发同步；压力下先合并/丢弃非关键 telemetry；**关键业务事件永不丢弃**；仍无法持久化 → 不提交完成状态并提示可恢复错误（outbox，7.3.1/7.3.2） |
| 时间同步失败 | 事件标 `timestamp_source=local`；功能继续；后端按到达时间处理 |
| 设备重启 | 事件队列持久化恢复（outbox）；快照完整 → 恢复同一 `session_id` 由用户选择继续/结束（`auto_saved`），快照损坏 → `aborted`；**Task 不自动 completed**；启动事件入队 |
| 后端重启 | 设备侧重试退避；`sync.failed`/`sync.recovered` 为本地可合并诊断（7.3.3），不入业务队列 |
| 配对码无效/过期 | 已认证家长会话调用；返回通用错误（**不泄露儿童是否存在**）；家长重试新配对码；限速防暴力 |
| child 不属于当前家长授权范围 | `403` 拒绝（通用外部错误，不泄露儿童存在性）；服务端保留脱敏审计原因（6.4.2） |
| claim 请求体自报 parent_id | 忽略自报字段，一律以服务端认证上下文派生 parent_id（6.4.2） |
| challenge 过期/重复使用/重放 | `401` 拒绝；设备重新发起 `POST /devices/challenge` 获取新 challenge（6.4.3） |
| 事件 child_id 与设备绑定不符 | `403`/`rejected` 并记录；该事件保持 pending，不落库、不推进 ACK（6.4.1） |

### 10.4 可观测性（ARCH_DECISION）

MVP：设备 `telemetry` 只写本地结构化日志（`PRODUCT_REQUIRED`）；后端记录 `request_id` 链路日志；不采集儿童内容数据。

---

## 11. 分阶段实施图（HOLD 状态，等待 G2/G3）

> 以下任务全部 `HOLD`（`AGENTS.md` 门禁：Bring-up Stage 1 未验收前，学习业务代码保持 HOLD）。**本文档不自动授权编码**；每项需 Codex 单独任务包 + READY。

| # | 任务 | 输入 | 允许路径建议 | 测试类型 | 依赖 | 停止条件 |
| --- | --- | --- | --- | --- | --- | --- |
| T1 | 接口骨架 | 本文 §3/§4/§5 | `main/learning_domain`、`main/sync`、`main/ui`、`main/assistant`、`main/telemetry` 头文件 | 编译期接口契约检查 | G2/G3 门禁 | 任一接口承诺"已实现" |
| T2 | 领域模型 | 本文 §4/§5 | `learning_domain` 实现 | 单元测试（状态机/不变量/异常路径） | T1 | 业务层出现 LVGL/Wi-Fi/GPIO 依赖 |
| T3 | Mock Backend | 本文 §5/§6 | `backend/`（FastAPI 桩）+ 内存/文件存储 | API 契约测试 | T2（或并行） | 引入真实数据库迁移 |
| T4 | Home 页面 | Mock 今日任务 | `ui/home` | 组件测试 + LVGL 人工检查 | T3 | UI 直接写服务器 |
| T5 | Focus 页面 | 领域 Timer | `ui/focus` | 状态驱动测试 | T2 | UI 绕过状态机 |
| T6 | 事件链路 | 本文 §5.3 | `sync` + `ui` intent | 事件/幂等/顺序测试 | T2 | 事件可丢 |
| T7 | 离线队列 | 本文 §7 | `sync/offline_store` | 掉电恢复/容量/死信测试 | T2+T3 | 队列违背 at-least-once |
| T8 | 真机接入（G2 后） | WB-BRINGUP-S1 证据 | 官方 BSP 集成点 | 实机验证 | G2/G3 | 未经用户授权的刷写 |

每项输入均需 Codex 任务包明确 `READY` 才可开始；完成顺序可并行化部分（T3 与 T2 可并行，`ARCH_DECISION`）。

---

## 12. 决策与未决项

### 12.1 决策表（ADR 精简版）

| # | 决策 | 理由 | 证据 | 影响 | 可逆性 |
| --- | --- | --- | --- | --- | --- |
| D1 | 事件驱动同步为主，服务器 JSON 不整包覆盖设备状态 | at-least-once + 幂等 + 离线可续传；符合 `项目总规划` §十 | `PRODUCT_REQUIRED` | sync 复杂度前置 | 可逆（先事件后补拉全量） |
| D2 | 家长端不直连设备、设备不直连 AI Provider | 安全边界；避免设备暴露端口与凭据泄漏面 | `PRODUCT_REQUIRED` | 后端必须在线才能跨端 | 不可逆（安全原则） |
| D3 | 业务层禁止 GPIO/LVGL/网络直接依赖；`learning_domain` 纯领域 | 可单测、可移植、防耦合 | `PRODUCT_REQUIRED` + `ARCH_DECISION` | 目录边界约束 | 不可逆（架构原则） |
| D4 | MVP 默认摄像头关闭、语音不实现，仅保留 intent 接口 | 隐私 + 范围收敛 | `PRODUCT_REQUIRED` | 功能后置 | 可逆 |
| D5 | 事件批量同步默认走 HTTPS REST；WSS 可选后置 | MVP 闭环不依赖实时通道 | `ARCH_DECISION` | 家长端延迟可见（秒级） | 可逆 |
| D6 | 任务/会话/事件契约以本文 §5 为基线 | 指导 Mock 与单测 | `ARCH_DECISION` | 后续实现受契约约束 | 可逆（版本化） |
| D7 | 设备持久化用 NVS + 既有 FAT 分区，不新增分区、不强依赖 microSD | 不改分区表硬门禁；SD 热插拔 UNKNOWN | `SOURCE_CONFIRMED` + 门禁 | 队列容量受限 | 可逆（V1 评估） |
| D8 | 官方 DeviceState 枚举保留，学习状态机在其上扩展 | 不破坏官方 BSP 边界 | `SOURCE_CONFIRMED` + `ARCH_DECISION` | 双状态机桥接 | 可逆 |
| D9 | **`/events/batch` 为设备业务写入唯一入口**；`/study-sessions/{id}/finish` 不属于设备 MVP 接口 | 单一写入路径，避免双重落账；配合 outbox/ACK 可验证（CR-WB002-02/04） | `ARCH_DECISION` | 后端落账收敛 | 可逆 |
| D10 | **设备—儿童授权绑定**：服务端签发 device_id + 配对/claim + token 声明绑定 + 逐请求/逐事件校验 | 拒绝冒充设备/儿童；注册/配对失败不泄露儿童存在（CR-WB002-04） | `ARCH_DECISION` | 身份链路复杂度前置 | 可逆 |
| D11 | **幂等查重先于 sequence 连续性检查**：认证/归属校验后先查 `(device_id, event_id)`，新事件才做连续性校验；摘要一致重发返回 `duplicate`+当前 ACK，摘要不一致返回 `conflict` | 消除"响应丢失后重发被误拒"的 at-least-once 反例（CR-WB002-06） | `ARCH_DECISION` | 服务端处理顺序唯一化 | 可逆 |
| D12 | **设备认证采用两阶段 challenge**：`POST /devices/challenge` 获取一次性 challenge_id+nonce+expires_at，`/devices/auth` 提交绑定 device_id/challenge_id/nonce 的签名 | 提供可实现的 nonce 获取调用与防重放闭环（CR-WB002-07） | `ARCH_DECISION` | 认证链路多一步交互 | 可逆 |

### 12.2 `HARDWARE_VERIFY_REQUIRED` / `UNKNOWN` 清单（链接现有证据，不复制过期结论）

| # | 项 | 状态 | 证据文档 |
| --- | --- | --- | --- |
| H1 | 屏驱 SKU（NV3051F/FL7707N） | `UNKNOWN` | [`CLAW4_PLATFORM_MAP.md`](CLAW4_PLATFORM_MAP.md) §4.4、[`BOARD_REVISION.md`](BOARD_REVISION.md) §4 |
| H2 | 触摸控制器与功能 | `UNKNOWN` | [`CLAW4_PLATFORM_MAP.md`](CLAW4_PLATFORM_MAP.md) §4.5 |
| H3 | Flash 容量与 mode 一致性 | `HARDWARE_VERIFY_REQUIRED`（芯片/模式已 `DEVICE_LOG_CONFIRMED`，容量未确认） | [`BOARD_REVISION.md`](BOARD_REVISION.md) §1 |
| H4 | PSRAM 实际分配量与 LVGL 缓冲运行稳定性 | `HARDWARE_VERIFY_REQUIRED` | [`CLAW4_PLATFORM_MAP.md`](CLAW4_PLATFORM_MAP.md) §4.2/§4.4 |
| H5 | C5 固件/连接状态 | `UNKNOWN` | [`BOARD_REVISION.md`](BOARD_REVISION.md) §4 |
| H6 | 实际网络通道（Wi-Fi/4G）与 4G 模组 | `UNKNOWN`（P4 日志执行到 `Initialize WiFi board` 行） | [`DEVICE_LOG_REFERENCE.md`](DEVICE_LOG_REFERENCE.md) §7 |
| H7 | partition/OTA 实际布局 | `UNKNOWN`（源码为 32m_dual.csv 非对称槽） | [`CLAW4_PLATFORM_MAP.md`](CLAW4_PLATFORM_MAP.md) §4.3 |
| H8 | 电池容量/充电 IC/SOC 读数 | `HARDWARE_VERIFY_REQUIRED` | [`HARDWARE_ASSUMPTIONS.md`](HARDWARE_ASSUMPTIONS.md) |
| H9 | 音频硬件（Mic/SPK/Codec 型号） | `UNKNOWN` | [`HARDWARE_ASSUMPTIONS.md`](HARDWARE_ASSUMPTIONS.md) |
| H10 | 摄像头传感器型号 | `UNKNOWN` | [`HARDWARE_ASSUMPTIONS.md`](HARDWARE_ASSUMPTIONS.md) |
| H11 | SD 热插拔能力 | `UNKNOWN` | [`CLAW4_PLATFORM_MAP.md`](CLAW4_PLATFORM_MAP.md) §4.9 |
| H12 | 外观 SKU/丝印/序列号 | `USER_EVIDENCE_REQUIRED` | [`PHYSICAL_INSPECTION.md`](PHYSICAL_INSPECTION.md) §5 |

### 12.3 产品待确认清单（PRODUCT_REQUIRED 决策点，需用户/Codex 明确）

| # | 待确认项 | 现状 | 影响 |
| --- | --- | --- | --- |
| P1 | 后端技术栈与部署形态（局域网 NAS/Docker）最终确认 | `ARCH_DECISION`（FastAPI/PostgreSQL 推荐） | T3 Mock Backend 形态 |
| P2 | 激励体系 MVP 范围（仅 +XP？Streak？） | `PRODUCT_REQUIRED`：MVP 仅 +XP 提示 | Reward 模块范围 |
| P3 | WSS 实时通道是否进 MVP | `ARCH_DECISION`：可选 | 家长端可见性延迟 |
| P4 | OTA/固件更新策略（A/B 对称改造需用户授权） | 门禁：不修改分区表 | 未来固件更新 |
| P5 | 家长端设备管理权限模型（单家长/多家长） | `PRODUCT_REQUIRED` 未细化 | 权限表设计 |

---

## 附录 A：与本任务相关的已验收事实索引（不复制内容，仅链接）

| 文档 | 内容 |
| --- | --- |
| [`CLAW4_PLATFORM_MAP.md`](CLAW4_PLATFORM_MAP.md) | WB-001 平台源码映射（12 子系统、证据分级） |
| [`CLAW4_AUDIT.md`](CLAW4_AUDIT.md) | 官方仓库审计 |
| [`HARDWARE_ASSUMPTIONS.md`](HARDWARE_ASSUMPTIONS.md) | 硬件事实与假设台账 |
| [`BUILD.md`](BUILD.md) | 构建基线与环境 |
| [`BOARD_REVISION.md`](BOARD_REVISION.md) | 板卡身份与固件基线（WB-HW-001/002） |
| [`DEVICE_LOG_REFERENCE.md`](DEVICE_LOG_REFERENCE.md) | 设备日志参考与采集边界 |
| [`PHYSICAL_INSPECTION.md`](PHYSICAL_INSPECTION.md) | 物理外观照片证据 |
| [`项目总规划/AGENTS.md`](../项目总规划/AGENTS.md) | 产品总规划（权威产品事实） |
