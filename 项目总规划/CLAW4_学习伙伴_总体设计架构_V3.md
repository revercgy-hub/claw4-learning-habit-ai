# Claw4 学习伙伴总体设计架构 V3

> 日期：2026-09-03
> 状态：评估版；不授权 Flash/partition/OTA 等高风险操作
> 核心变化：在 V2 的 Metalio 原生 Learning App 方案上，正式加入 `78/xiaozhi-esp32` 作为上游语音/MCP/协议架构源。

---

## 0. 当前事实基线

### 我们的项目
- 仓库：`revercgy-hub/claw4-learning-habit-ai`
- `main`：`03383dbda702e95f69e5e59eab0332526a2915bd`
- WorkBuddy 当前成果分支：`workbuddy/domain-offline-stream`
- 当前已知 HEAD：`bcb2d5defad6433aa85517244400133da8a18f33`
- 相对 main：ahead 12 / behind 0
- CP0～CP8 Host MVP 已完成，但此前独立审查发现的 Task 状态契约、Today Cache、Dashboard 当日统计、reboot monotonic、Auth Pause、ACK 防御、PWA token restore 等问题仍应在真机集成前修复。
- 当前未发现 final-fixes 分支或 PR；`TASK_BOARD.md` 仍明显滞后。

### Claw4 / Metalio
- 实机已确认 ESP32-P4 双核、360 MHz、32 MB PSRAM / 200 MHz。
- 720×720 MIPI-DSI；ESP32-C5 + ESP-Hosted SDIO 提供 Wi-Fi。
- Metalio 当前项目版本 `2.0.51`，项目基线 ESP-IDF 5.5.4。
- 已具备 LVGL、多 App、AudioService、ESP-SR、WebSocket/MQTT、MCP、ASR/LLM/TTS 协议链和 Chat/OpenClaw。

### XiaoZhi Upstream
- 上游仓库：`78/xiaozhi-esp32`。
- 当前项目版本 `2.4.2`，主线优先 ESP-IDF 6.0.2。
- Metalio README 明确将其标为上游架构。
- 两边 `main/mcp_server.h` 当前 blob SHA 完全一致：`dacdd551534daf2afe92b7449d7d1e6a2749dc07`。
- Protocol、WebSocket、MCP、AudioService、DeviceState 等大量结构明显同源。
- Metalio 已形成 Claw4 专用分叉，例如 `SetVoiceUiDesired()`、720 UI、P4+C5、蓝牙音频、电源/4G 等。
- XiaoZhi 主线当前未发现 Metalio Claw4 专用 board 目录。

### V3 决策
> **MetalioClaw4 作为 Claw4 真机/硬件运行基线；XiaoZhi 作为上游语音、MCP、协议与安全改进来源；Learning Product 作为我们独立可测试的产品层。**

明确不做：
1. 不从零重写 Claw4 固件。
2. 不整体 merge `xiaozhi/main`。
3. 不重新 Fork XiaoZhi 后再重新移植完整 Claw4 硬件栈。
4. 当前阶段不迁移 IDF 6。

---

## 1. 产品定位

本项目不是小智聊天机器人加任务列表，也不是 Claw4 上的普通任务管理 App。

> **最终产品：放在孩子学习桌上的专注型 AI 学习伙伴。**

核心闭环：
`计划 → 提醒/触发 → 开始 → 专注 → 暂停/恢复 → 完成 → 即时反馈 → 复盘 → 调整`

原则：
- 专注优先。
- Touch 始终能完成核心学习流程；Voice 作为低摩擦主要入口。
- AI 是 Coach，不是业务权威。
- 网络、ASR、LLM 故障时基础学习链仍可工作。
- 家长管理任务/规则/趋势，不远程微操。
- Student Mode 默认隐藏娱乐和无关 App。
- 摄像头/视觉不进入第一阶段 MVP。

---

## 2. 三层技术架构

```text
Layer C — Learning Product（我们拥有）
  Learning UI / Interaction / Learning MCP
  Learning Core / Outbox / Sync / Recovery
  Backend / Parent PWA / AI Coach
                 │
                 │ stable ports/adapters
                 ▼
Layer B — Metalio Claw4 Platform
  Claw4 BSP / 720 LVGL / Touch / Audio / C5
  Power / Storage / App Framework
  XiaoZhi-derived Audio/MCP/Protocol runtime
                 │
                 │ selective upstream backport
                 ▼
Layer A — 78/xiaozhi-esp32 Upstream
  Voice / MCP / Protocol / Audio / Security / P4 fixes
```

### Layer A：XiaoZhi Upstream
定位：只读跟踪 + 选择性 backport。

重点跟踪：
- MCP Server / MCP protocol。
- AudioService / Audio Engine。
- Wake Word / VAD / AEC。
- WebSocket / MQTT+UDP。
- P4 / ESP-SR 修复。
- 协议输入校验、安全修复。
- DeviceStateMachine / 网络生命周期。
- IDF 6 迁移经验。

禁止：直接 `git merge xiaozhi/main`，或仅因为版本号更高就整体切换。

### Layer B：Metalio Platform
定位：Claw4 真机唯一硬件基线。

负责：P4/C5、Display/Touch、Audio Codec、Power/Battery、4G、Camera、Storage、LVGL、多 App、Voice UI 生命周期。

原则：**底层少动，上层深做。**

### Layer C：Learning Product
负责：Task、StudySession、Domain、Coordinator、Outbox、Offline/Recovery、Today Task Cache、Learning UI、Interaction、Learning MCP、Backend/PWA、AI Coach。

硬约束：Learning Core 不依赖 XiaoZhi、LVGL 或 ESP-IDF。

---

## 3. 固件内原生开发方式

继续采用 V2 的判断：
> **Learning App 是 Metalio 主固件内原生应用模块，不是 Android 式沙盒 App，也不是另一套独立系统。**

建议结构：
```text
firmware/main/
  learning_domain/      # 纯 C++，已存在
  application/          # Coordinator，已存在
  sync/                 # Outbox/Sync，已存在
  ui/                   # Presenter，已存在
  interaction/          # V3 新增
  learning_mcp/         # V3 新增
  platform/             # Ports

integration/metalio_claw4/
  app_entry/
  adapters/
  voice_bridge/
  mcp_bridge/
  lvgl/
  integration_manifest.md

vendor/MetalioClaw4/    # 固定硬件基线
docs/XIAOZHI_UPSTREAM_TRACKING.md
```

---

## 4. Interaction Layer：Touch / STT / MCP 统一入口

```text
Touch ----------------┐
STT Deterministic ----┼----> Interaction Router
XiaoZhi MCP Tool -----┘              │
                                     ▼
                              Command Dispatcher
                                     │
                                     ▼
                               Learning Domain
```

所有入口最终变成同一套 Command：
- `StartTask(task_id)`
- `PauseTask()`
- `ResumeTask()`
- `RequestCompleteTask()`
- `ConfirmCompleteTask()`
- `GetTodayTasks()`
- `GetCurrentTask()`
- `GetRemainingTime()`
- `GetTodayProgress()`

禁止 Touch、Voice、AI 各自维护一套业务逻辑。

---

## 5. XiaoZhi 语音如何进入 Learning

Metalio 已有：`Wake Word → AudioService → Opus → WebSocket/MQTT → ASR → LLM → TTS → Speaker`。

同时 Metalio `application.cc` 已处理 `type=stt` 和 `type=mcp`，后者进入 `McpServer::ParseMessage()`。

因此 V3 使用双通道：

### 通道 A：本地确定性 STT Intent
适合“暂停”“继续”“还有多久”“当前任务是什么”等。

`Speech → XiaoZhi ASR → STT text → SttIntentMapper → CommandDispatcher → Domain`

优点：不依赖 LLM Tool 决策，延迟和安全性更可控。

### 通道 B：XiaoZhi MCP
适合自然语言理解和参数映射，例如“小智，我先做数学”。

建议工具：
- `learning.get_today_tasks`
- `learning.get_current_task`
- `learning.get_remaining_time`
- `learning.get_today_progress`
- `learning.start_task`
- `learning.pause_task`
- `learning.resume_task`
- `learning.request_complete_task`

### 完成任务安全规则
AI 不得静默完成任务。

推荐：
`learning.request_complete_task → AwaitingConfirmation → 用户明确触摸/语音确认 → ConfirmCompleteTask → Domain Complete`

不向模型暴露无保护的 `learning.complete_task`。

---

## 6. Metalio Voice UI 生命周期

Metalio 的 `SetVoiceUiDesired()` 是 Claw4 特有关键能力，目前用于 Chat/Digital Human 前台持有唤醒词/AFE，离开页面后软停并延迟释放，以降低 CPU 和避免低内存反复重建问题。

Learning 必须复用这一机制：
- Learning Home/Focus 前台按配置调用 `SetVoiceUiDesired(true)`。
- 离开 Learning 主体验调用 `false`。
- Learning 不直接 destroy/recreate ESP-SR 内部对象。
- 真机必须做 Learning/Home/Chat 反复切换压力测试。

---

## 7. Learning MCP 边界

```text
MCP Call
→ LearningMcpBridge
→ 参数/状态校验
→ Interaction Command
→ DomainReducer
→ Outbox
→ Presenter
```

MCP callback 禁止：
- 直接修改 Task.status。
- 直接写 Backend DB。
- 直接操纵 Outbox sequence。
- 绕过 Domain。
- 绕过完成确认。

工具返回只给最小必要状态，避免把大量儿童学习隐私上下文发给通用 LLM。

---

## 8. AI 分阶段

### V1：先复用 XiaoZhi ASR / LLM / TTS / MCP
目的：快速得到像小智一样自然的学习语音入口。

### V2：Family Backend AI Coach
再接 Ollama / DeepSeek / OpenAI-compatible，用于任务拆解、建议、每日/每周复盘、家长摘要。

### V3：可选 XiaoZhi-compatible 自建语音服务
未来如需降低对 `xiaozhi.me` 的依赖，再评估 NAS 上的小智兼容 Server/Gateway。当前不进入真机 MVP。

---

## 9. Upstream Tracking

新增 `docs/XIAOZHI_UPSTREAM_TRACKING.md`。

必须记录：Metalio 当前、XiaoZhi 当前、差异、是否 KEEP/TRACK/BACKPORT/HOLD。

重点初始条目：
- Project：Metalio 2.0.51 vs XiaoZhi 2.4.2 → TRACK。
- IDF：5.5.4 vs 6.0.2 preferred → HOLD。
- MCP：高度同源 → TRACK/BACKPORT。
- AudioService：Claw4 定制 vs 上游持续演进 → REVIEW。
- DeviceStateMachine：Metalio 较旧/定制 vs 新主线 → REVIEW。
- `SetVoiceUiDesired()`：Metalio 特有 → KEEP。

每个 backport 必须单独记录 upstream commit、问题、冲突、测试和真机验证要求。

---

## 10. Student Mode

最终默认入口：`Boot → Student Mode → Learning Home`。

默认隐藏 2048、电台、音乐、开发工具、Camera(MVP)。OpenClaw 通用入口由家长决定是否显示。

系统设置、网络、恢复入口继续保留在家长/维护模式，不能为了专注而删除原厂恢复能力。

---

## 11. 离线稳定性原则

即使 XiaoZhi 云、LLM、ASR、Backend、Wi-Fi 全部不可用，仍必须支持：
`缓存任务 → Touch 开始 → 本地计时 → Pause/Resume → Complete → Outbox → 网络恢复后同步`。

> **XiaoZhi Runtime 是交互能力，不是 Learning Core 的依赖。**

---

## 12. 推荐实施顺序
1. Host Final Fix。
2. 架构/任务板同步。
3. XiaoZhi Upstream Tracking。
4. Metalio Integration Recheck。
5. Platform Ports。
6. Interaction Router Host。
7. Learning MCP Host/mock。
8. Metalio Adapter Scaffold。
9. 定制固件只编译。
10. Flash Plan。
11. 用户明确授权。
12. 真机 Touch Learning Home/Focus。
13. NVS/Network/Offline。
14. Voice UI lifecycle。
15. STT deterministic commands。
16. Learning MCP tools。
17. Complete confirmation safety。
18. Real-device E2E 稳定性。
19. AI Coach。

---

## 13. 高风险门禁

必须用户明确授权：Flash 写入/擦除、partition、boot partition、OTA 策略、Bootloader、Secure Boot/Flash Encryption、覆盖原厂恢复路径。

Host 代码、文档、编译、静态检查不需要逐项授权。

---

## 14. V3 最终架构决策

> **以 MetalioClaw4 为 Claw4 硬件与运行时基线，以 78/xiaozhi-esp32 为语音/MCP/协议/安全上游，以现有 Learning Core 为产品核心；在 Metalio 主固件内原生集成 Learning UI、Interaction Router、Learning MCP、Offline Sync，并通过选择性 backport 吸收 XiaoZhi 上游改进。**

V3 的核心价值：**复用 XiaoZhi 成熟交互能力，复用 Metalio 成熟 Claw4 硬件适配，把有限开发精力集中在真正差异化的 Learning Product。**