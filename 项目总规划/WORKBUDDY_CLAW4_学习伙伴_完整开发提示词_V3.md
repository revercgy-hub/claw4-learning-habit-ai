# WorkBuddy Master Prompt — Claw4 学习伙伴 V3

> 用途：用户确认 V3 路线后交给 WorkBuddy 连续执行。
> Flash/partition/boot/OTA 等高风险操作始终受用户授权门禁约束。

---

## 0. 你的角色

你是 `revercgy-hub/claw4-learning-habit-ai` 的主要实现者、测试者和常规代码审查者。

最终产品不是普通 Task App，也不是通用聊天机器人，而是：
> **放在孩子书桌上的专注型 AI 学习伙伴。**

最高原则：
1. Learning Core 是业务权威。
2. Touch 必须始终能完成核心流程。
3. Voice 降低操作摩擦。
4. AI 只能建议和调用受控 Tool。
5. AI 不得静默完成 Task。
6. 断网/ASR/LLM 故障时仍能学习。
7. 不为追新版本破坏 Claw4 稳定性。
8. Flash/partition/OTA 必须等待用户明确授权。

---

## 1. V3 技术路线

必须采用：
`78/xiaozhi-esp32 Upstream → selective backport → CloudZao/MetalioClaw4 Platform → thin adapters → Learning Product`。

### XiaoZhi Upstream
只读跟踪 Voice、MCP、Protocol、Audio、P4/ESP-SR、Security fixes。禁止整体 merge。

### Metalio Platform
是 Claw4 真机唯一基线。保护 P4+C5、720 MIPI、GT911、Audio、Power、4G、LVGL framework、SetVoiceUiDesired 和当前 5.5.4 build baseline。

### Learning Product
我们控制 Domain、Coordinator、Outbox、Sync、Recovery、Presenter/UI、Interaction、Learning MCP、Backend、PWA。

---

## 2. 开工前重新读取远端事实

执行：
```bash
git fetch --all --prune
git status
git branch -a
git log --oneline --decorate --all -40
```

当前已知但必须复核：
- main `03383dbda702e95f69e5e59eab0332526a2915bd`
- domain-offline-stream `bcb2d5defad6433aa85517244400133da8a18f33`
- ahead main 12。
- CP0～CP8 已完成。
- TASK_BOARD 滞后。
- 尚未看到 final-fixes 分支。

若远端已变化，以远端最新事实为准并在报告中说明。

---

## 3. 分支策略

用户正式授权执行后，从最新 Host 成果创建/沿用 `workbuddy/learning-firmware-v3`。

禁止直接在 main 开发；禁止 force push。

---

## 4. 第一阶段：Host MVP Final Fix

真机 Integration 前必须全部完成。

### FIX-01 Task lifecycle
today executable Task 默认 ready；Start 只允许 Ready；Paused 只能 Resume；Pending 不可 Start；跨 Backend/C++/fixture 一致。

### FIX-02 Today cache
server=[] 清非 active 旧任务；active session 保留；version 不回退；date rollover 明确。

### FIX-03 Dashboard
按家庭本地日边界；Session 按 started_at 归属。

### FIX-04 Reboot monotonic
旧 anchor 不跨 boot；Running reboot → Recovery/Paused；Resume 新 anchor。

### FIX-05 Auth Pause
PausedAuth 后 transport 不再 send；显式 reset 后恢复。

### FIX-06 ACK
只删除连续 Accepted/Duplicate；Rejected/Conflict/Gap/missing 停止。

### FIX-07 DeadLetter
markDeadLetter fail → StorageError；不推进 ACK。

### FIX-08 Backend uniqueness
增加 UNIQUE(device_id, sequence)，保留 event_id 去重。

### FIX-09 PWA auth
reload/sessionStorage/ApiClient 同步并测试 login/logout/reload。

### FIX-10 completed task_id
manual/auto_saved/aborted 始终带 task_id；backend 不创建空 task_id terminal row。

### FIX-11 Cross-layer Contract
`Backend Task JSON → C++ mapping → Reducer canonical events → Backend ingest`。

### FIX-12 Stability
C++×5、backend×5、PWA×3、Host E2E×5、pip check、npm audit。

未全部通过，不得进入 Metalio 集成。

---

## 5. 第二阶段：同步项目事实源

更新 README、TASK_BOARD、HOST_MVP_ACCEPTANCE 和相关报告。

不得继续写 CP0 READY / CP1-CP8 QUEUED；Host 完成不等于 Real Device MVP 完成。

---

## 6. 第三阶段：XiaoZhi Upstream Tracking

新增 `docs/XIAOZHI_UPSTREAM_TRACKING.md`。

初始事实：Metalio 2.0.51 / IDF 5.5.4；XiaoZhi 2.4.2 / IDF 6.0.2 preferred。

分类 KEEP_METALIO / TRACK_UPSTREAM / BACKPORT_CANDIDATE / HOLD_MIGRATION。

重点比较 MCP、Application、AudioService、Protocol、WebSocket/MQTT、DeviceStateMachine、P4/ESP-SR、security/input validation。

禁止 `git merge xiaozhi/main`；禁止当前阶段升级 IDF 6。

---

## 7. 第四阶段：Metalio Integration Recheck

重新读取 `vendor/MetalioClaw4` 当前真实代码，输出 `docs/METALIO_LEARNING_INTEGRATION_MAP_V3.md`。

必须确认 Home app registry、screen lifecycle、LVGL thread、Application::Schedule、STT handling、MCP handling、McpServer registration、SetVoiceUiDesired、AudioService、Network/HTTP、NVS/storage、firmware size/partition headroom。

证据标记 SOURCE_CONFIRMED / DEVICE_LOG_CONFIRMED / ARCH_DECISION / HARDWARE_VERIFY_REQUIRED。

---

## 8. 第五阶段：Interaction Router Host

新增 `firmware/main/interaction/`，包含 intent、command、router、dispatcher、stt mapper、context。

最少 Command：StartTask、PauseTask、ResumeTask、RequestCompleteTask、ConfirmCompleteTask、QueryTodayTasks、QueryCurrentTask、QueryRemainingTime、QueryTodayProgress。

Touch、STT、MCP 必须共享 CommandDispatcher。

---

## 9. 第六阶段：Learning MCP Host Bridge

新增 `firmware/main/learning_mcp/`，先 Host/mock，不依赖真实 XiaoZhi Server。

建议工具：
- learning.get_today_tasks
- learning.get_current_task
- learning.get_remaining_time
- learning.get_today_progress
- learning.start_task
- learning.pause_task
- learning.resume_task
- learning.request_complete_task

MCP callback 只允许 validate → Command → dispatcher → Domain。

禁止直接 mutate state、直接写 outbox/backend、暴露无确认 complete。

完成流程必须 request_complete → AwaitingConfirmation → 用户确认 → ConfirmCompleteTask。

---

## 10. 第七阶段：Platform Ports

新增 ClockPort、StoragePort、NetworkPort、UiPort、VoiceSessionPort、SttPort、McpRegistrationPort、PowerPort。

Learning Core 不得 include Metalio/ESP-IDF header。

---

## 11. 第八阶段：Metalio Adapter Scaffold

落点 `integration/metalio_claw4/`。

实现 Learning App entry、Presenter→LVGL、Touch→Interaction、Clock、Storage、Network、STT bridge、SetVoiceUiDesired bridge、MCP registration bridge、Student Mode 最小入口。

原则：BSP 不知道 Learning 业务；Learning Core 不知道 Metalio。

---

## 12. 第九阶段：Custom Firmware Build Only

在现有 Metalio 5.5.4 基线编译。

禁止修改 sdkconfig、partition、IDF 6 migration、Flash。

输出 compile/link、binary size、baseline delta、partition fit、warnings、RAM/PSRAM 风险、touched Metalio files。

---

## 13. 第十阶段：Flash Plan 后停止

生成 `docs/FLASH_PLAN_LEARNING_V3.md`，必须写 running partition、目标地址、binary/size、覆盖范围、原厂恢复、失败恢复、ota_0/ota_1 风险、ESPClaw 影响、串口观察和验收。

然后输出：
`LEARNING_FIRMWARE_V3_HOST_READY`
`FLASH_AUTH_REQUIRED`

并停止。没有用户明确授权不得刷机。

---

## 14. 用户授权后：Touch Real MVP

先只做：`Boot → Learning Home → Today Tasks → Start → Focus → Pause → Resume → Complete → Done`。

暂不接 Voice。

采集 internal heap、PSRAM、largest free block、FPS、touch latency、页面切换、10/30/60 分钟稳定性。

---

## 15. 然后接 NVS / Network / Offline

必须先证明：
`在线拿任务 → 断网 → 学习 → 完成 → 本地持久化 → 重启恢复 → 网络恢复 → event sync → Parent PWA 可见`。

这是 Voice 前强门槛。

---

## 16. Voice Integration 顺序

### 16.1 Voice UI Lifecycle
复用 Metalio `SetVoiceUiDesired()`。Learning Home/Focus 前台启用，离开时关闭；不要自己重建 ESP-SR AFE。

压力测试 Learning↔Home、Learning↔Chat、连续进入退出、heap leak、CPU、wakeword、crash。

### 16.2 STT Deterministic Commands
先做暂停、继续、还有多久、今天还有几个任务、当前任务。

`XiaoZhi ASR → stt text → SttIntentMapper → CommandDispatcher`。

### 16.3 Device MCP Tools
把 Host 验证过的 Learning Tools 注册进 Metalio `McpServer`。不要重写 MCP protocol。

### 16.4 Complete Confirmation
模型不能直接完成任务，必须用户确认。

---

## 17. AI Coach 最后做

只有 Real Device + Offline + Voice 稳定后开始。

AI Coach 可使用 Family Backend / AI Gateway / Ollama / DeepSeek / OpenAI-compatible，用于任务拆解、建议、复盘、家长摘要。

不得改写历史、自动完成 Task、绕过 Domain。

---

## 18. Upstream Backport 规则

任何 XiaoZhi 上游改进必须单项处理：
`upstream commit → 问题 → Metalio 对应实现 → 冲突分析 → narrow backport → host/build test → 必要时 HARDWARE_VERIFY_REQUIRED`。

重点保护 Metalio 特有：SetVoiceUiDesired、Claw4 board、720 UI、C5 Hosted、Bluetooth audio、Power/4G。

---

## 19. 当前明确禁止

- 整仓 XiaoZhi fork 替换 Metalio。
- IDF 6 migration。
- partition/ota_1 重构。
- Camera AI / emotion detection。
- local LLM。
- 手机学生端。
- 娱乐功能。
- 复杂 Plan/Habit 自动化。

---

## 20. 最终开发哲学

`XiaoZhi = 成熟语音与 MCP 上游`
`Metalio = 成熟 Claw4 硬件平台`
`Learning Core = 我们的产品真相`
`Interaction = Touch / STT / MCP 统一入口`
`AI = 受控 Coach`

你的任务不是把三个项目揉成一团，而是通过清晰边界让三者协同。

> **最终目标：一台像小智同学一样自然可对话，但所有交互都围绕孩子真实学习执行闭环服务，并且断网也能可靠工作的 Claw4 专注学习伙伴。**