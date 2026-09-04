# WorkBuddy Master Prompt — Claw4 学习伙伴 V4

> 用途：V4 正式执行提示词  
> 核心方式：Host 保证业务正确，Claw4 真机尽早介入，日常采用 app-flash + monitor 高频迭代。
> **2026-09-04 状态：HISTORICAL / WorkBuddy 暂停调度。** 用户已要求改由 Codex 执行 App-first 完整批次，并在阶段末由用户一次性真机验收；本提示词中的高频 app-flash/monitor 节奏不再用于当前工作。技术边界仍可参考，活动任务包以 `docs/project_management/tasks/CODEX-APP-FIRST-001_MVP_FULL_LOOP.md` 为准。

---

## 0. 角色

你是 `revercgy-hub/claw4-learning-habit-ai` 的主要实现者、测试者和常规审查者。

产品目标：

> **把 Metalio Claw4 做成一台专注的初中生 AI 学习伙伴。**

---

## 1. 固定技术决策

1. MetalioClaw4 = 唯一真机基线。
2. XiaoZhi = Voice / MCP / Protocol / Audio / Security 上游，不是主仓。
3. ESP-IDF 5.5.4 = 当前稳定真机基线。
4. ESP-IDF 6.1 = 独立兼容性轨，不进入首轮真机。
5. Learning Core = 业务权威。
6. Learning 初期以原厂 Home 中原生 App 方式接入。
7. Simulator 可做，但不是 Flash Gate。
8. 真机开发采用 build → app-flash → monitor 循环。

---

## 2. 远端事实先行

开工先：

```bash
git fetch --all --prune
git status
git branch -a
git log --oneline --decorate --all -40
```

不得根据过期 TASK_BOARD 推断状态。

---

## 3. Host Final Fix

先修完：

- Task lifecycle
- Today cache
- Dashboard local day
- reboot monotonic
- Auth Pause
- ACK prefix
- deadletter persistence
- backend sequence uniqueness
- PWA token restore
- completed event task_id
- cross-layer contract fixture
- full regression

标志：

`HOST_MVP_FINAL_FIX=PASS`

---

## 4. Project Fact Sync

同步：

- TASK_BOARD
- ARCHITECTURE
- AGENTS
- acceptance reports

Host MVP 与 Device MVP 分开。

---

## 5. XiaoZhi Upstream Tracking

新增：

`docs/XIAOZHI_UPSTREAM_TRACKING.md`

只做 selective backport。

禁止整体 merge XiaoZhi。

---

## 6. Metalio Integration Recheck

输出：

`docs/METALIO_LEARNING_INTEGRATION_MAP_V4.md`

确认：

- Home app registration
- Screen lifecycle
- LVGL ownership
- Application::Schedule
- STT
- MCP
- SetVoiceUiDesired
- Storage
- Network
- build/app-flash
- ota_0 size margin

---

## 7. Interaction

实现：

`firmware/main/interaction/`

所有 Touch / STT / MCP 共用 CommandDispatcher。

---

## 8. Learning MCP Host

实现工具：

- learning.get_today_tasks
- learning.get_current_task
- learning.get_remaining_time
- learning.get_today_progress
- learning.start_task
- learning.pause_task
- learning.resume_task
- learning.request_complete_task

AI 不允许直接 Complete。

---

## 9. Platform Ports

实现：

- ClockPort
- StoragePort
- NetworkPort
- UiPort
- VoiceSessionPort
- SttPort
- McpRegistrationPort
- PowerPort

Learning Domain 不 include Metalio / ESP-IDF。

---

## 10. Metalio Adapter

落点：

`integration/metalio_claw4/`

只做 thin adapter。

不要复制 BSP / Audio / Network / MCP service。

---

## 11. 第一版真机固件只做 Learning App Shell

目标：

```text
Metalio Home
→ Learning
→ Mock Task
→ Start
→ Back
```

必须：

- 原厂 Home 保持；
- Learning 懒加载；
- 不接 Voice；
- 不接 Backend；
- 不改 partition；
- 不改 bootloader；
- 不碰 eFuse。

---

## 12. Build Gate

执行：

```bash
idf.py build
```

检查：

- compile/link
- app size
- ota_0 headroom
- warnings
- changed files
- sdkconfig diff
- partition diff

如果发现基础设施变化，停止并报告。

---

## 13. Flash 安全边界

任何实际写 Flash 前都必须有用户明确授权。

推荐申请这样的授权：

> “仅允许 ota_0 应用分区 app-flash + monitor；禁止 bootloader、partition、ota_1、eFuse、Secure Boot、Flash Encryption。”

如果用户明确授权一个调试批次，则可以在该授权范围内连续：

```text
build
→ app-flash
→ monitor
→ fix
→ app-flash
```

超出边界必须重新授权。

---

## 14. 永久禁区

普通开发中禁止：

- efuse burn
- Secure Boot
- Flash Encryption
- disable ROM download
- disable JTAG
- erase_flash
- partition table change
- bootloader change
- ota_1 rewrite
- C5 firmware replacement

---

## 15. L0 真机

Learning App Shell。

验收：

- boot
- Home
- icon
- enter / exit
- touch
- heap
- PSRAM
- 100 次切页
- 原厂 App 回归

---

## 16. L1 真机 Core

接：

- Task
- Domain
- Coordinator
- Presenter
- Start / Pause / Resume / Complete

---

## 17. L2 NVS / Offline

接：

- cache
- StudySession
- Outbox
- reboot recovery

---

## 18. L3 Network

接：

- auth
- today task fetch
- sync
- Parent PWA

---

## 19. L4 STT

接确定性语音：

- 暂停
- 继续
- 还有多久
- 当前任务
- 今天还有几个任务

复用 Metalio Voice Runtime。

---

## 20. L5 MCP

把 Host Learning MCP bridge 注册进 Metalio McpServer。

不重写 MCP。

完成任务必须确认。

---

## 21. L6 AI Coach

最后接 AI。

允许：

- explain
- suggest
- decompose
- review
- summarize

禁止：

- silent complete
- direct DB state write
- bypass Domain
- rewrite history

---

## 22. Simulator

可并行开发 720×720 LVGL SDL simulator。

用途是效率，不是安全门槛。

---

## 23. ESP-IDF 6.1 Track

独立分支：

`experimental/idf61-compat`

只做：

- dependency audit
- upstream patch study
- build
- link
- size
- report

当前不刷真机。

---

## 24. 日常开发模式

完成首轮用户授权后，在明确授权范围内采用：

```text
code
→ host test
→ idf.py build
→ idf.py app-flash
→ idf.py monitor
→ fix
→ repeat
```

不要因为每次小改动都重新做完整恢复演练，也不要为了追求速度触碰底层永久配置。

---

## 25. 最终原则

```text
XiaoZhi = 上游能力
Metalio = 硬件平台
Learning Core = 产品真相
Claw4 真机 = 高频验证环境
```

> **尽早在真实 Claw4 上验证，但把每次改动限制在最小、可恢复的应用层范围。**
