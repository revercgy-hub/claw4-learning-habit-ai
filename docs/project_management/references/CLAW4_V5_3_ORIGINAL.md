# CLAW4 学习伙伴后续开发任务 V5.3（正式执行版）

> 版本：V5.3  
> 日期：2026-09-13  
> 适用仓库：`revercgy-hub/claw4-learning-habit-ai`  
> 适用执行者：Codex / WorkBuddy  
> 核心产品定位：**Claw4 是学生学习伙伴主机，不是 NAS 的语音遥控器。它必须本地掌握每日任务、本地按时主动提醒、本地完成任务执行闭环，并在此基础上提供 AI 对话、解释、鼓励和复盘能力。**

---

# 0. V5.3 的目标

V5.3 不再增加新的产品方向，而是把 V5.2 已确定的产品路线工程化、事实化。

V5.3 的核心产品结构：

```text
                 CLAW4 学习伙伴
                        │
       ┌────────────────┼────────────────┐
       │                │                │
       ▼                ▼                ▼
  每日任务执行       主动提醒         AI 对话
  Task Engine     Reminder Engine   AI Conversation
       │                │                │
       └───────────┬────┴────┬──────────┘
                   ▼         ▼
               Habit Engine  MCP
                   │
                   ▼
                学习习惯
```

产品原则：

> **AI 对话是能力，主动提醒是职责，学习习惯养成是目标。**

Claw4 必须做到：

```text
知道今天要做什么
知道什么时候该开始
到点主动提醒
孩子不响应时合理再次提醒
开始后进入专注
到点提醒休息/继续
完成时确认
晚上回顾
第二天继续形成规律
```

---

# 1. 四个基础可靠性 Gate

V5.3 将基础能力拆成四个独立 Gate：

```text
                 PRODUCT FOUNDATION
                        │
      ┌─────────────────┼─────────────────┬────────────────┐
      ▼                 ▼                 ▼                ▼
P0-A Learning      P0-B Time        P0-C Reminder      P0-D Voice
Reliability        Reliability      Reliability        Reliability
```

只有四个 Gate 逐步稳定，产品能力才能可靠叠加。

---

# 2. P0-A — Learning Reliability Gate

## 2.1 当前问题

真机当前已出现：

```text
Start → Accepted
Pause → PersistFailed
Complete → PersistFailed
```

Host 同源路径：

```text
Start
→ Pause
→ Resume
→ Complete
→ Restart
→ Recovery
```

已通过，因此：

> 当前缺陷优先视为 Device-only integration defect，不允许优先重写 Domain / Outbox / Coordinator。

## 2.2 必须完成

将 `PersistFailed` 拆成结构化原因：

```text
LOAD_FAILED
DECODE_FAILED
EMPTY_DRAFT
EMPTY_EVENT_ID
DUPLICATE_EVENT_ID
CAPACITY_EXCEEDED
ENCODE_FAILED
STORAGE_SAVE_FAILED
STORAGE_COMMIT_FAILED
CONTEXT_INVALID
REENTRY_DETECTED
UNKNOWN
```

主攻：

```text
NvsOutboxStorage
LearningRuntime
ReducerContext / ContextBuilder
LearningApp lifetime
LVGL callback / timer re-entry
NVS handle lifecycle
state_ vs persisted snapshot
```

## 2.3 验收

真机：

```text
Seed
→ Start
→ Pause
→ Resume
→ Complete
→ Restart
→ Recovery
```

全部通过。

完成标记：

```text
LEARNING_RELIABILITY_GATE=PASS
```

---

# 3. P0-B — Time Reliability Gate

> V5.3 将“时间”从 Reminder 内部实现细节提升为独立平台基础设施。

当前硬件/固件事实：

```text
RTC 使用内部 RC
未启用外部 32.768 kHz 晶振
现有代码未形成持续 SNTP 客户端机制
现有 ClockPort 只有：
  epochSeconds()
  monotonicMs()
  isTimeSynced()
```

因此不能假设：

```text
设备开机对过一次时间
=
以后一直准确
```

---

# 4. Time Authority

新增平台无关概念：

```text
TimeAuthority
```

建议接口：

```cpp
enum class TimeQuality {
    Unsynced,
    Synced,
    Stale,
};

struct TimeStatus {
    int64_t epoch_seconds;
    int64_t monotonic_ms;
    TimeQuality quality;
    int64_t last_sync_epoch;
    int64_t last_sync_monotonic_ms;
};
```

ClockPort 可保持现有兼容接口，同时新增扩展能力。

---

# 5. SNTP / 时间同步

## 5.1 必须新增

设备端实现 SNTP 客户端：

```text
Boot
→ Network ready
→ SNTP sync
→ settimeofday()
→ 记录 last_sync
```

并支持：

```text
Wi-Fi reconnect
→ 再同步

长时间运行
→ 周期校时

休眠唤醒
→ 检查时间可信度
```

具体同步周期不在任务书中硬编码。

初期建议：

```text
30~60 分钟范围试验
```

最终由实测功耗、网络稳定性和漂移数据确定。

---

# 6. 时间质量判定

不得再把：

```text
isTimeSynced = 曾经同步过
```

作为唯一标准。

建议：

```text
Unsynced
= 从未获得可信网络时间

Synced
= 最近同步仍在可信窗口内

Stale
= 曾同步，但距上次同步已过长
```

Reminder Engine 根据质量做决策。

---

# 7. 时间漂移实测

新增真机测试：

```text
1 小时
4 小时
8 小时
24 小时
```

场景至少覆盖：

```text
正常在线
Wi-Fi 断开
屏幕关闭
未来 light sleep
```

记录：

```text
真实参考时间
设备时间
误差
温度/状态（可记录则记录）
```

---

# 8. Reminder 时间目标

当前产品目标：

```text
正常联网环境：
提醒目标误差 ≤ 30 秒

连续离线 4 小时：
提醒目标误差 ≤ 60 秒
```

这是产品目标，不是硬件先验结论。

若实测内部 RC 无法满足：

```text
再评估外部 32.768 kHz 晶振
```

硬件修改不属于 V5.3 MVP 默认范围。

---

# 9. Time Gate 验收

必须验证：

```text
Boot → 自动同步
Wi-Fi 重连 → 自动恢复同步
网络断开 → 本地时间继续走
恢复网络 → 时间纠偏
时间突然跳变 → Reminder reconcile
```

不得出现：

```text
恢复联网后所有历史过期提醒同时触发
```

完成标记：

```text
TIME_RELIABILITY_GATE=PASS
```

---

# 10. P0-C — Reminder Reliability Gate

Reminder 是 V5.3 第一核心产品能力。

底层原则：

> **基础提醒必须本地触发。**

不能依赖：

```text
NAS
LLM
WebSocket
MCP
云服务
```

---

# 11. Reminder Core

新增：

```text
firmware/main/reminder/
  reminder_types.h
  reminder_engine.h
  reminder_engine.cpp
  reminder_policy.h
  reminder_store.h
```

建议：

```cpp
ReminderInstance {
    reminder_id
    task_id
    kind
    due_epoch
    next_fire_epoch
    state
    repeat_count
    last_fired_epoch
    acknowledged_action
}
```

类型：

```text
TaskDue
TaskOverdue
FocusEnd
BreakEnd
DailyReview
```

MVP：

```text
TaskDue
Snooze
FocusEnd
DailyReview
```

---

# 12. Reminder 与 Learning Domain 分工

Reminder：

```text
负责什么时候提醒
```

Learning：

```text
负责任务当前状态
```

例：

```text
Reminder 到点
→ UI 提示 Start

孩子点 Start
→ CommandDispatcher
→ Learning Domain
```

Snooze：

```text
ReminderEngine::snooze()
```

不等于：

```text
Task Pause
```

---

# 13. Reminder 第一阶段：前台本地提醒

在任何 Power Save 集成之前，先做：

```text
设备保持正常运行
→ 设置 2 分钟后提醒
→ 到点
→ 本地 PlaySound
→ Reminder Overlay
→ Start / Snooze / Dismiss
```

必须完成：

```text
20 次连续提醒
```

无：

```text
漏响
重复触发
UI 卡死
NVS 错乱
```

完成标记：

```text
LOCAL_REMINDER_FOREGROUND=PASS
```

---

# 14. Reminder 本地音频

Level 1：

```text
本地固定 OGG 提示音
```

必须：

```text
无需 Wi-Fi
无需 NAS
无需 LLM
无需 TTS
```

使用 Metalio 现有：

```text
Application::PlaySound()
AudioService::PlaySound()
```

不得为提醒重写 AudioService。

---

# 15. Reminder Overlay

Reminder Overlay 优先级：

```text
高于当前普通页面
低于系统级 Critical Alert
```

应可覆盖：

```text
Home
Learning Screen
普通 Chat 页面
```

但不销毁当前 App 状态。

MVP：

```text
┌────────────────────────┐
│       学习提醒          │
│                        │
│ 数学作业               │
│ 计划 19:00 开始        │
│ 预计 30 分钟           │
│                        │
│ [开始] [10分钟后]      │
│      [稍后处理]        │
└────────────────────────┘
```

---

# 16. Reminder Persistence

持久化：

```text
reminder_id
task_id
due_epoch
next_fire_epoch
state
repeat_count
last_fired_epoch
```

Boot：

```text
load
→ current time
→ reconcile
→ rebuild next reminder
```

---

# 17. Missed Reminder

例：

```text
计划 19:00
设备 19:08 重启
```

允许补提醒。

如果：

```text
计划昨天 19:00
今天上午才开机
```

不得突然响昨天的提醒。

实现：

```text
grace_window
```

超出：

```text
mark missed
→ habit event
→ Daily Review
```

具体时长走配置。

---

# 18. Offline Reminder

必须验证：

```text
Wi-Fi OFF
NAS OFF
xiaozhi-server OFF
```

仍然：

```text
到点响铃
亮屏
显示任务
Start / Snooze
状态保存
```

完成标记：

```text
OFFLINE_REMINDER=PASS
```

---

# 19. Claw4 Power Save 的真实现状

Metalio common 层有：

```text
power_save_timer
sleep_timer
```

但当前 Claw4 板级：

```text
power_save_timer include / 调用未正式启用
CONFIG_PM_ENABLE 未启用
```

因此不能把：

```text
Light Sleep Reminder
```

视作现成能力。

V5.3 的定位：

> **Power Save / Light Sleep 是首次正式板级集成任务。**

---

# 20. Power Save 开发顺序

严格按顺序：

```text
1. 前台 Reminder
2. Reminder Persistence
3. Offline Reminder
4. Screen Off
5. Light Sleep
6. Wake → Reminder
```

禁止一开始就同时调：

```text
Scheduler
NVS
LVGL
Audio
Sleep
Wake
```

---

# 21. ReminderWakePort

新增：

```text
ReminderWakePort
```

平台无关接口：

```text
armNextWake(epoch)
cancelWake()
wakeForReminder()
```

Host：

```text
FakeReminderWakePort
```

Device：

```text
MetalioReminderWakePort
```

Reminder Core 不直接引用：

```text
esp_sleep
```

---

# 22. Light Sleep 验收

测试：

```text
设 2 分钟提醒
→ 屏幕休眠
→ light sleep
→ reminder due
→ CPU wake
→ 真正退出 sleep 流程
→ LVGL 恢复
→ 背光恢复
→ PlaySound
→ Overlay
```

仅验证：

```text
CPU 被 timer 唤醒
```

不算通过。

完成标记：

```text
LIGHT_SLEEP_REMINDER=PASS
```

---

# 23. Deep Sleep

V5.3 MVP：

```text
不做
```

因为 Deep Sleep 涉及：

```text
RTC wake
→ boot
→ restore TodayPlan
→ restore Reminder
→ identify wake cause
→ decide due reminder
→ UI / Audio
```

后续单独评估。

---

# 24. P0-D — Voice Reliability Gate

AI Voice 必须稳定，但不抢占 Reminder 的开发优先级。

继续验证：

```text
AutoStop
Speaking
VoiceProcessing
WakeNet
AEC
VAD
PSRAM
```

原则：

> **不能先假设 AEC 是自问自答根因。**

MVP 优先：

```text
稳定半双工
```

而不是：

```text
Realtime 全双工
```

---

# 25. Voice 验收

连续 20 轮：

```text
Wake
→ 用户说话
→ ASR
→ LLM
→ TTS
→ Idle
```

要求：

```text
无自问自答
无循环
无 crash
无 watchdog
无持续 heap/PSRAM 下降
```

完成：

```text
VOICE_RELIABILITY_GATE=PASS
```

---

# 26. Parent PWA → TodayPlan

在 Local Reminder 稳定后，再接家长端。

Parent Task 至少增加：

```text
planned_start
estimated_duration
reminder_enabled
pre_reminder_minutes
snooze_options
priority
```

未来再做：

```text
repeat_rule
school_day_rule
quiet_hours
```

---

# 27. TodayPlan 同步

链路：

```text
Parent PWA
→ Backend
→ Device Sync
→ TodayPlan
→ ReminderEngine rebuild
```

断网时：

```text
Claw4 使用最后一次成功同步 TodayPlan
```

---

# 28. Reminder Level 2 — 在线自然语言播报

Level 1 永远先执行：

```text
本地铃声
+
Reminder Overlay
```

之后如果网络可用，再做 Level 2：

```text
Reminder local fire
        ↓
local sound
        ↓
overlay
        ↓
临时 OpenAudioChannel
        ↓
发送 reminder narration intent
        ↓
NAS TTS
        ↓
播放一句自然语言
        ↓
CloseAudioChannel
```

例：

> “现在是数学作业时间，预计需要 30 分钟，要开始了吗？”

---

# 29. Level 2 禁忌

禁止：

```text
为了 Reminder 长期保持 WebSocket
```

禁止：

```text
常驻 Voice session
```

禁止：

```text
TTS 获取失败 → 整个 Reminder 不响
```

Level 2 永远是增强层。

---

# 30. InteractionArbiter

新增：

```text
InteractionArbiter
```

处理：

```text
AI 正在说话
+
Reminder 到点
```

规则：

1. 两路音频不同时播放；
2. 普通 Reminder：
   - AI 当前句很短 → 结束后马上提醒；
   - Reminder 不能永久被对话吞掉；
3. Reminder Overlay 出现后孩子仍可进入 AI；
4. AI 不得自动替孩子 Snooze / Dismiss；
5. 用户明确说：
   “十分钟后提醒”
   才允许修改 Reminder。

---

# 31. Device MCP

在 Reminder 与 Voice 都稳定后接入。

现有：

```text
learning.get_today_tasks
learning.get_current_task
learning.get_remaining_time
learning.get_today_progress
learning.start_task
learning.pause_task
learning.resume_task
learning.request_complete_task
```

新增：

```text
learning.get_next_reminder
learning.snooze_reminder
learning.ack_reminder
```

提醒触发器仍然是：

```text
Local Clock
→ Reminder Engine
```

不是 MCP。

---

# 32. AI 的职责

AI 负责：

```text
解释
鼓励
拆解
建议
复盘
```

不负责：

```text
本地计时
基础提醒触发
状态真相
直接完成任务
```

必须显式禁止：

> AI 不得直接把任务标记为完成。

完成仍：

```text
request_complete
→ physical confirmation
→ Domain Complete
```

---

# 33. Habit Engine

先记录客观行为：

```text
task_scheduled
reminder_fired
reminder_snoozed
reminder_dismissed
task_started
task_paused
task_resumed
task_completed
task_overdue
```

Backend 统计：

```text
按时开始率
平均延迟
Snooze 次数
完成率
连续执行天数
```

---

# 34. 产品里程碑

## M1 — LOCAL_REMINDER_MVP

```text
Claw4 独立
→ 本地任务
→ 到点
→ 主动响
→ Overlay
→ Start / Snooze
```

要求：

```text
不依赖 NAS
```

---

## M2 — DAILY_PLAN_MVP

```text
Parent PWA
→ Backend
→ Claw4 TodayPlan
→ 按时主动提醒
```

---

## M3 — HABIT_LOOP_MVP

```text
Reminder
→ Start
→ Focus
→ FocusEnd
→ Continue / Complete
→ Daily Review
```

---

## M4 — AI_COMPANION_MVP

```text
AI Voice
+
Device MCP
+
Learning State
```

---

## M5 — LEARNING_COMPANION_MVP

```text
主动提醒
+
每日任务
+
Habit Loop
+
AI 对话
+
Parent PWA
+
Offline reliability
```

---

# 35. V5.3 正式 Checkpoint

```text
V5.3-C0       Fact Sync / workflow gate

V5.3-P0A-a    Persist diagnostics
V5.3-P0A-b    Device adapter fix
V5.3-P0A-c    Host regression
V5.3-P0A-d    Device full cycle                [FLASH_AUTH_REQUIRED]

V5.3-P0B-a    RTC / current-time baseline
V5.3-P0B-b    SNTP client implementation
V5.3-P0B-c    TimeQuality / last-sync
V5.3-P0B-d    Drift test 1h/4h/8h/24h          [DEVICE_TEST]
V5.3-P0B-e    reconnect / reconcile
V5.3-P0B-f    timezone handling

V5.3-P0C-a    Reminder host core
V5.3-P0C-b    Reminder persistence
V5.3-P0C-c    Local PlaySound integration       [VENDOR_PATCH_AUTH]
V5.3-P0C-d    Reminder Overlay                  [VENDOR_PATCH_AUTH]
V5.3-P0C-e    20 foreground cycles              [FLASH_AUTH_REQUIRED]
V5.3-P0C-f    Offline / NAS-off test
V5.3-P0C-g    ReminderWakePort
V5.3-P0C-h    Claw4 power-save integration      [VENDOR_PATCH_AUTH]
V5.3-P0C-i    Light Sleep wake                  [FLASH_AUTH_REQUIRED]
V5.3-P0C-j    Level-2 temporary TTS connection

V5.3-P0D-a    Voice instrumentation
V5.3-P0D-b    Half-duplex baseline
V5.3-P0D-c    20-turn stability                 [FLASH_AUTH_REQUIRED]
V5.3-P0D-d    Optional AEC experiment

V5.3-P1a      Parent task schedule fields
V5.3-P1b      Backend TodayPlan
V5.3-P1c      Device schedule sync
V5.3-P1d      Reminder rebuild

V5.3-P2       Focus / Snooze / Habit telemetry

V5.3-P3       NAS XiaoZhi Server

V5.3-P4       Device MCP

V5.3-P5       AI + Reminder

V5.3-P6       Daily Review

V5.3-P7       Personalization
```

---

# 36. vendor / 真机授权门禁

默认：

```text
vendor/MetalioClaw4
=
只读基线
```

需要修改时：

```text
必须任务包显式授权
```

正式集成优先采用：

```text
integration/metalio_claw4/
  device adapters
  thin bridge
  minimal patch
```

所有 Metalio 改动记录：

```text
integration_manifest.md
```

---

# 37. Flash Gate

任何：

```text
app-flash ota_0
```

执行前：

```text
FLASH_AUTH_REQUIRED
```

WorkBuddy 只可：

```text
build
生成 bin
给出命令
说明风险
停止等待授权
```

禁止：

```text
erase flash
erase NVS
partition
bootloader
ota_1
C5
eFuse
```

---

# 38. 工作流唯一性

执行前 Codex 必须检查：

```text
AGENTS.md
TASK_BOARD.md
当前 READY / IN_PROGRESS workflow
```

确保同一时刻：

```text
只有一个正式工作流处于 READY / IN_PROGRESS
```

如果 V4/V5.1/V5.2 仍标记活动：

```text
先关闭/归档
再启用 V5.3
```

---

# 39. 每个 Checkpoint 报告

必须包含：

```text
Task ID
Base SHA
New SHA
Changed Files
Why
Implementation
Tests
Host Tests
Device Build
Binary Size
sdkconfig diff
Partition diff
NVS impact
Time impact
Heap / PSRAM impact
Voice impact
Reminder impact
Regression
Device Evidence
Known Risks
Vendor Patch Required
Flash Required
User Authorization
Scope Deviations
Next Task
```

---

# 40. Codex 第一条执行指令

收到本文后：

1. 读取远端最新状态；
2. 核实当前 workflow；
3. 将旧 V5.2 标记 superseded；
4. 将 V5.3 设为唯一活动工作流；
5. 同时准备：
   - P0-A Learning Reliability
   - P0-B Time Reliability
   - P0-C Reminder host core
6. 不启动 Light Sleep，直到前台 Reminder PASS；
7. 不启动 Level-2 TTS，直到本地铃声 PASS；
8. 不把 NAS/MCP 提前到 Reminder 前；
9. 所有 vendor 修改走显式授权；
10. 所有 app-flash 等用户授权。

---

# 41. WorkBuddy 第一条执行指令

第一阶段目标不是“更智能”。

而是确保：

```text
时间是可信的
↓
到了时间一定知道
↓
一定能响
↓
一定能显示正确任务
↓
孩子操作一定保存
↓
断网仍然工作
```

执行顺序：

```text
Time
→ Foreground Reminder
→ Persistence
→ Offline
→ Parent Sync
→ Power Save
→ Voice
→ NAS
→ MCP
→ AI
```

不得为了 AI 体验破坏基础提醒可靠性。

---

# 42. V5.3 最终原则

> **Claw4 是主机。**

> **时间是地基。**

> **Reminder 是职责。**

> **Learning Domain 是事实。**

> **NAS 是增强。**

> **AI 是助手。**

> **MCP 是边界。**

> **先做到“按时叫、一定响、能开始”，再做到“会聊天、会建议、会总结”。**
