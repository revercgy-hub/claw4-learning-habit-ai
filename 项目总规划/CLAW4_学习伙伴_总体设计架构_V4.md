# Claw4 学习伙伴总体设计架构 V4

> 日期：2026-09-03  
> 状态：评估确认版  
> 适用基线：MetalioClaw4 + ESP-IDF 5.5.4 稳定轨；XiaoZhi 仅作上游；ESP-IDF 6.1 仅作并行兼容性轨  
> V4 核心变化：从“模拟优先、刷机谨慎”调整为“Host 保证业务正确 + 真机早介入 + app-flash 高频迭代”。
> **2026-09-04 执行节奏覆盖说明：** 用户在 L1c 持久化复测后将后续节奏改为“App/Host 完整批次 → 冻结唯一候选 → 用户阶段末一次真机验收”。本文件的分层架构与安全边界继续有效，但“每个小功能高频 app-flash/monitor”不再是当前执行方式；以根 `AGENTS.md` 与 `docs/project_management/tasks/CODEX-APP-FIRST-001_MVP_FULL_LOOP.md` 为准。

---

## 1. V4 最终技术路线

```text
78/xiaozhi-esp32
      │
      │ selective backport / upstream tracking
      ▼
CloudZao/MetalioClaw4
      │
      │ stable platform + thin adapter
      ▼
Claw4 Learning Product
      ├─ Learning Core
      ├─ Interaction Router
      ├─ Learning MCP
      ├─ Offline / Sync / Recovery
      ├─ Learning UI
      ├─ Backend
      └─ Parent PWA
```

固定原则：

1. **MetalioClaw4 是唯一真机硬件基线。**
2. **XiaoZhi 只作为 Voice / MCP / Protocol / Audio / Security 上游。**
3. **Learning Core 是业务真相，不依赖 XiaoZhi、LVGL、ESP-IDF。**
4. **真机尽早介入，但日常只做应用级烧录。**
5. **底层基础设施不随普通功能开发一起改动。**
6. **模拟器是效率工具，不是刷机安全的强制前置。**

---

## 2. 为什么 V4 可以更早真机开发

Metalio 官方开发方式已经明确支持：

```bash
idf.py build
idf.py flash
idf.py monitor
idf.py app-flash
```

Claw4 同时具备：

- ESP32-P4 USB JTAG/Serial 调试与烧录入口；
- P4 ROM Download 能力；
- `BOOT_BUTTON_GPIO = GPIO_NUM_35`；
- 默认未启用 Secure Boot；
- 默认未启用 Flash Encryption；
- `CONFIG_SECURE_ROM_DL_MODE_ENABLED=y`；
- 官方仓库提供 `Metalio_Claw4_Latest.bin` 完整恢复镜像；
- 官方工程支持 `idf.py merge-bin`；
- 蓝屏/崩溃/串口监视本身就是官方调试流程的一部分。

因此项目不需要把“真机刷写”当成极少发生的特殊事件，而应把它视为标准 ESP-IDF 开发过程。

---

## 3. V4 的安全模型

V4 不追求“避免刷写”，而是追求“只改必要范围”。

### 日常允许的开发范围

```text
Learning App
Learning UI
Interaction
Learning MCP
Learning Core integration
Network / NVS adapter
```

默认目标：

> **只更新 ota_0 的应用镜像。**

### 普通开发禁止触碰

- eFuse
- Secure Boot
- Flash Encryption
- Disable ROM Download
- Disable JTAG
- Partition Table
- Bootloader
- ota_1 / ESPClaw 布局
- factory / resources / system / storage 分区
- IDF 主版本切换

除非专门开基础设施迁移任务，否则这些都不进入普通 Learning 开发。

### 授权模型

实际 Flash 仍属于高风险动作。

用户可以：

1. 对一次具体 app-flash 单独授权；
2. 或对一个明确范围的真机调试批次授权，例如：
   - 仅 `ota_0`
   - 仅 `app-flash`
   - 不改 partition / bootloader / eFuse
   - 在该批次内允许重复 build → app-flash → monitor

一旦超出授权边界，必须重新获得用户明确授权。

---

## 4. Learning 初期仍采用原厂 Home 中的原生 App

V4 保留这个策略，但理由从“防刷死”调整为“控制变量、便于定位”。

初期：

```text
Metalio Home
├─ Chat
├─ OpenClaw
├─ Settings
├─ Test
├─ ...
└─ Learning
```

只有进入 Learning 后才创建 Learning UI / Coordinator / Presenter。

优点：

- 原厂屏幕、触摸、Audio、Wi-Fi、Home 可作为对照组；
- Learning 问题更容易定位；
- Learning 可以独立进入和退出；
- 不改变开机主链路；
- 保留原厂测试页和维护入口。

成熟后再改成：

```text
Boot
→ Student Mode
→ Learning Home
```

---

## 5. Learning App 必须懒加载

第一阶段不要在以下位置做大量 Learning 初始化：

- Board constructor
- `app_main()`
- Metalio early boot
- AudioService early init

推荐：

```text
Boot
→ Metalio Home
→ 点击 Learning
→ LearningApp::Load()
→ Coordinator
→ Presenter
→ Learning UI
```

离开 Learning 时释放不必要的页面资源。

---

## 6. Interaction 统一入口

```text
Touch ----------------┐
STT Deterministic ----┼──→ Interaction Router
XiaoZhi MCP ----------┘          │
                                 ▼
                         Command Dispatcher
                                 │
                                 ▼
                          Learning Domain
```

业务逻辑不允许散落到 Touch callback、MCP callback 或 STT handler 中。

首批 Intent / Command：

- START_TASK
- PAUSE_TASK
- RESUME_TASK
- REQUEST_COMPLETE_TASK
- CONFIRM_COMPLETE_TASK
- LIST_TODAY_TASKS
- GET_CURRENT_TASK
- GET_REMAINING_TIME
- GET_TODAY_PROGRESS

---

## 7. Learning MCP

正式工具建议：

- `learning.get_today_tasks`
- `learning.get_current_task`
- `learning.get_remaining_time`
- `learning.get_today_progress`
- `learning.start_task`
- `learning.pause_task`
- `learning.resume_task`
- `learning.request_complete_task`

不提供无保护的 AI 自动完成工具。

完成链：

```text
AI / Voice
→ learning.request_complete_task
→ AwaitingConfirmation
→ 用户明确确认
→ ConfirmCompleteTask
→ Domain Complete
```

---

## 8. Voice 策略

继续复用 Metalio：

- AudioService
- ESP-SR
- Wake Word / VAD / AEC
- STT message
- WebSocket / MQTT
- MCP Server
- TTS lifecycle
- `SetVoiceUiDesired()`

优先级：

1. Touch MVP
2. STT deterministic commands
3. Learning MCP
4. AI Coach

不重写底层 Audio / I2S / Wake Word。

---

## 9. 离线能力仍是基础能力

即使 XiaoZhi、ASR、LLM、Backend、Wi-Fi 都不可用，仍必须：

```text
缓存今日任务
→ Touch Start
→ Focus
→ Pause / Resume
→ Complete
→ 本地 StudySession
→ Outbox
→ 后续同步
```

Voice 和 AI 是增强层，不是 Learning Core 的依赖。

---

## 10. V4 真机迭代层级

### L0 — Learning App Shell

- 原厂 Home 增加 Learning 图标
- Learning 页面能进入/退出
- 一个 Mock Task
- 一个 Start 按钮
- 无 Backend / NVS / Voice

### L1 — Core Loop

- Start
- Pause
- Resume
- Complete
- Presenter / Domain 真正连接

### L2 — Persistent Offline

- NVS / Storage adapter
- Outbox
- Reboot recovery

### L3 — Network

- Backend
- Auth
- Today Tasks
- Event sync
- Parent PWA

### L4 — Voice Command

- STT
- deterministic mapping
- Voice overlay

### L5 — Learning MCP

- XiaoZhi MCP tools
- completion confirmation

### L6 — AI Coach

- Plan / decomposition
- explanation
- review
- parent summary

---

## 11. Simulator 的新定位

Simulator 不再是强制 Gate。

建议保留：

```text
simulator/
  720x720 LVGL SDL
  fake_clock
  fake_storage
  fake_network
  fake_stt
  fake_mcp
```

用于：

- UI 快速设计；
- 故障注入；
- Offline 场景；
- 自动化 E2E；
- 无设备时继续开发。

但不能阻塞真机开发。

---

## 12. ESP-IDF 6.1 新定位

ESP-IDF 6.1 已支持 P4 / C5，因此从 HOLD 改为：

> **Compatibility Track — BUILD / RESEARCH ONLY**

稳定轨：

```text
Metalio + IDF 5.5.4
→ Learning 真机开发
```

旁路：

```text
experimental/idf61-compat
→ dependency audit
→ XiaoZhi upstream diff
→ compile
→ link
→ binary size
→ compatibility report
```

在 Learning L0/L1 真机稳定前，不把 6.1 作为主基线。

---

## 13. 推荐仓库结构

```text
firmware/main/
  learning_domain/
  application/
  interaction/
  learning_mcp/
  sync/
  ui/
  platform/

integration/metalio_claw4/
  learning_app_entry.*
  metalio_lvgl_adapter.*
  metalio_storage_adapter.*
  metalio_network_adapter.*
  metalio_voice_adapter.*
  metalio_mcp_adapter.*
  integration_manifest.md

simulator/                  # optional but recommended
backend/
frontend/
docs/
  XIAOZHI_UPSTREAM_TRACKING.md
  METALIO_LEARNING_INTEGRATION_MAP_V4.md
```

---

## 14. V4 最终开发哲学

> **Host 测试保证业务正确，真机高频迭代保证平台集成正确。**

> **不要为了“安全”把真机拖到最后；也不要为了“快”去动与 Learning 无关的 bootloader、partition、eFuse 和硬件底层。**

最终产品路线保持不变：

> **Metalio 提供可靠硬件平台，XiaoZhi 提供成熟语音与 MCP 上游，Learning Core 提供真正的学习产品价值。**
