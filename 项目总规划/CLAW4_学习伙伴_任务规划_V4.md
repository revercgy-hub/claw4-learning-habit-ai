# Claw4 学习伙伴任务规划 V4

> 日期：2026-09-03  
> 核心调整：删除过重的 Simulation / Recovery 强制门槛，采用“Host Final Fix → Learning App Shell → 真机 app-flash 高频迭代”的开发路线。
> **2026-09-04 执行节奏覆盖说明：** 用户已改为 App-first 批量开发。后续先完成一个可验收纵向阶段并跑自动 gate，再冻结唯一固件由用户一次性上机；不再按小功能反复刷写或持续读取串口。当前批次为 L2/L3 MVP 主链，详见 `docs/project_management/tasks/CODEX-APP-FIRST-001_MVP_FULL_LOOP.md`。

---

## 0. 当前技术决策

- MetalioClaw4：唯一真机硬件基线。
- XiaoZhi：仅上游，不作为主仓。
- ESP-IDF 5.5.4：当前稳定真机基线。
- ESP-IDF 6.1：并行兼容性验证，不进入首次 Learning 真机基线。
- Learning 初期：原厂 Home 内原生 App。
- 日常写入：优先 app-flash。
- Simulator：辅助，不是 Gate。
- eFuse / Secure Boot / Flash Encryption / partition / bootloader：普通开发禁止。

---

## 1. 总体路线

```text
HOST FINAL FIX
→ PROJECT FACT SYNC
→ XIAOZHI UPSTREAM TRACKING
→ METALIO INTEGRATION RECHECK
→ INTERACTION
→ LEARNING MCP HOST
→ PLATFORM PORTS
→ METALIO ADAPTER
→ LEARNING APP SHELL
→ BUILD
→ USER FLASH AUTH
→ REAL DEVICE L0
→ L1 CORE
→ L2 NVS/OFFLINE
→ L3 NETWORK
→ L4 STT
→ L5 MCP
→ L6 AI COACH
```

---

## 2. P0～P10：Host MVP 现状

P0～P10 继续沿用现有成果。

但以下 Final Fix 必须先完成：

1. ready / pending / paused 状态契约
2. authoritative today cache
3. dashboard local-day filtering
4. reboot monotonic recovery
5. auth_paused transport short-circuit
6. ACK continuous prefix
7. deadletter persistence
8. UNIQUE(device_id, sequence)
9. PWA token restore
10. completed event task_id
11. cross-layer contract fixture
12. regression / E2E 多轮稳定性

完成标志：

```text
HOST_MVP_FINAL_FIX=PASS
```

---

## 3. P11 — Project Fact Sync

更新：

- TASK_BOARD.md
- ARCHITECTURE.md
- HOST_MVP_ACCEPTANCE.md
- AGENTS.md
- 相关状态报告

要求：

- 不再把已完成 CP0～CP8 标成 queued；
- Host MVP 与 Device MVP 分开；
- 当前真实 HEAD 和测试结果成为唯一事实源。

---

## 4. P12 — XiaoZhi Upstream Tracking

新增：

`docs/XIAOZHI_UPSTREAM_TRACKING.md`

分类：

- KEEP_METALIO
- TRACK_UPSTREAM
- BACKPORT_CANDIDATE
- HOLD_MIGRATION

重点：

- MCP
- AudioService
- Protocol
- WebSocket / MQTT
- ESP-SR
- P4 / C5 fixes
- security / input validation
- DeviceStateMachine

禁止整仓 merge。

---

## 5. P13 — Metalio Integration Recheck

输出：

`docs/METALIO_LEARNING_INTEGRATION_MAP_V4.md`

必须确认：

- Home app registry
- screen load/unload
- LVGL ownership
- app entry
- Application::Schedule
- STT handling
- MCP ParseMessage
- MCP tool registration
- SetVoiceUiDesired
- Storage / NVS
- Network / HTTP
- app partition size
- ota_0 size margin
- build/app-flash commands

---

## 6. P14 — Interaction Router

Host 实现：

- Command
- Intent
- Router
- Dispatcher
- Context
- STT mapper

Touch / STT / MCP 全部汇入同一 Dispatcher。

---

## 7. P15 — Learning MCP Host

先用 Mock Transport 验证：

- get_today_tasks
- get_current_task
- get_remaining_time
- get_today_progress
- start_task
- pause_task
- resume_task
- request_complete_task

AI 不直接 Complete。

---

## 8. P16 — Platform Ports

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

## 9. P17 — Metalio Adapter

实现：

- Learning App registration
- Presenter → LVGL
- Touch → Interaction
- Clock
- Storage
- Network
- STT bridge
- SetVoiceUiDesired bridge
- MCP registration bridge

只做薄适配，不复制 Metalio service。

---

## 10. P18 — Learning App Shell

这是第一次真机目标，不做完整 Learning。

功能仅包括：

```text
Metalio Home
→ Learning
→ Mock Task
→ Start
→ Back
```

要求：

- 原厂 Home 保持正常；
- 原厂 Chat / Settings / Test 保持可用；
- Learning 可重复进入退出；
- Learning 不在 early boot 初始化；
- 不接 Voice；
- 不接 Backend；
- 不改 partition / bootloader / eFuse。

---

## 11. P19 — Build & Static Gate

执行：

- idf.py build
- size
- warnings
- partition fit
- diff scan

必须输出：

- app binary size
- ota_0 headroom
- touched Metalio files
- 是否修改 sdkconfig
- 是否涉及危险基础设施

只有在“仅应用层变化”的情况下进入真机写入申请。

---

## 12. P20 — 真机写入授权

实际写 Flash 前必须获得用户明确授权。

推荐授权范围：

> “允许本次/本批次仅对 ota_0 应用分区执行 app-flash + monitor 调试；禁止修改 bootloader、partition、ota_1、eFuse、Secure Boot、Flash Encryption。”

如果用户授权的是“一个明确批次”，则批次内可以反复：

```text
build → app-flash → monitor → fix → app-flash
```

一旦写入范围变化，必须重新授权。

---

## 13. P21 — L0 真机 App Shell

验证：

- boot
- Home
- Learning icon
- enter / exit
- touch
- no crash
- heap / PSRAM
- 100 次切页
- 原厂功能回归

通过后进入 L1。

---

## 14. P22 — L1 Learning Core 真机闭环

加入：

- real Task
- Start
- Pause
- Resume
- Complete
- Presenter
- Domain
- Coordinator

仍然可以先不联网。

验收：

```text
Home
→ Learning
→ Task
→ Start
→ Focus
→ Pause
→ Resume
→ Complete
→ Done
```

---

## 15. P23 — L2 NVS / Offline / Recovery

加入：

- persistent task cache
- StudySession
- Outbox
- reboot recovery
- power-cycle scenarios

重点验证 reboot monotonic 逻辑。

---

## 16. P24 — L3 Network / Backend

加入：

- Wi-Fi
- auth
- today task fetch
- event batch sync
- ACK handling
- Parent PWA

验收：

```text
在线拉任务
→ 断网
→ 学习
→ 完成
→ 本地保存
→ 恢复网络
→ 自动同步
→ PWA 可见
```

---

## 17. P25 — L4 Voice STT

先接确定性语音：

- 暂停
- 继续
- 还有多久
- 当前任务
- 今天还有几个任务

复用 Metalio STT message + SetVoiceUiDesired。

---

## 18. P26 — L5 Learning MCP

接入 XiaoZhi McpServer：

- Tool registration
- command validation
- state validation
- complete confirmation

不重写 MCP protocol。

---

## 19. P27 — L6 AI Coach

最后增加：

- 任务拆解
- 学习建议
- 讲解
- 日复盘
- 周复盘
- 家长摘要

AI 不拥有 Task / StudySession 状态权威。

---

## 20. Simulator — Optional Track

状态：

`OPTIONAL / RECOMMENDED`

用于：

- 720×720 UI 快速设计
- fault injection
- offline scenarios
- automated E2E

但不阻塞真机。

---

## 21. ESP-IDF 6.1 Compatibility Track

状态：

`RESEARCH / BUILD_ONLY`

独立分支建议：

`experimental/idf61-compat`

验证：

1. dependency resolution
2. XiaoZhi upstream compatibility patches
3. compile
4. link
5. binary size
6. warnings
7. component migration list

禁止直接作为首次真机 Learning 基线。

---

## 22. 普通开发永久禁区

没有独立基础设施任务和用户明确授权，不允许：

- burn eFuse
- Secure Boot
- Flash Encryption
- disable ROM download
- disable JTAG
- change partition table
- change bootloader
- rewrite ota_1
- erase_flash
- mass erase
- replace C5 firmware

---

## 23. V4 开发节奏

推荐日常节奏：

```text
小改动
→ Host Test
→ idf.py build
→ app-flash
→ monitor
→ 真机验证
→ 修复
→ 再 app-flash
```

不要每个阶段都重新设计恢复方案。

安全来自：

> **原厂基线稳定 + 写入范围小 + ROM Download 可恢复 + 官方完整固件存在 + 不碰永久性安全配置。**
