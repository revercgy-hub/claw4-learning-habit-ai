# Claw4 学习伙伴任务规划 V3

> 日期：2026-09-03
> 状态：评估版
> 状态定义：`DONE` / `DONE_NEEDS_FIX` / `READY` / `PLANNED` / `HOLD` / `BLOCKED_USER_AUTH` / `RESEARCH_ONLY`

---

## 0. 当前快照
- `main`：`03383dbda702e95f69e5e59eab0332526a2915bd`
- `workbuddy/domain-offline-stream`：`bcb2d5defad6433aa85517244400133da8a18f33`
- ahead main 12 / behind 0。
- CP0～CP8 Host MVP 已完成；尚未发现 final-fixes 分支或 PR。
- TASK_BOARD 仍滞后。
- Metalio：2.0.51 / IDF 5.5.4。
- XiaoZhi Upstream：2.4.2 / IDF 6.0.2 preferred。

---

## 1. 总体阶段

| 阶段 | 内容 | 状态 |
|---|---|---|
| P0 | Repo / Platform Audit | DONE |
| P1 | Official Build Baseline | DONE |
| P2 | Hardware Read-only Intake | DONE |
| P3 | Host Architecture V1/V2 | DONE_NEEDS_FIX |
| P4 | Host Learning Domain | DONE_NEEDS_FIX |
| P5 | Host Outbox | DONE_NEEDS_FIX |
| P6 | Host Coordinator | DONE_NEEDS_FIX |
| P7 | Host Presenter | DONE |
| P8 | Backend MVP | DONE_NEEDS_FIX |
| P9 | Parent PWA | DONE_NEEDS_FIX |
| P10 | Host E2E | DONE_NEEDS_FIX |
| P11 | XiaoZhi Upstream Tracking | READY |
| P12 | Metalio Integration Recheck | PLANNED |
| P13 | Interaction Router Host | PLANNED |
| P14 | Learning MCP Host Bridge | PLANNED |
| P15 | Platform Ports | PLANNED |
| P16 | Metalio Adapter Scaffold | PLANNED |
| P17 | Custom Firmware Build | PLANNED |
| P18 | Flash Plan | PLANNED |
| P19 | First Custom Flash | BLOCKED_USER_AUTH |
| P20 | Real Learning Touch UI | PLANNED |
| P21 | Real NVS / Offline | PLANNED |
| P22 | Real Network / Auth | PLANNED |
| P23 | Voice UI Lifecycle | PLANNED |
| P24 | STT Deterministic Commands | PLANNED |
| P25 | Learning MCP Tools | PLANNED |
| P26 | Complete Confirmation Safety | PLANNED |
| P27 | Real-device E2E Stability | PLANNED |
| P28 | AI Coach | HOLD |
| P29 | XiaoZhi-compatible NAS Voice | RESEARCH_ONLY |
| P30 | IDF 6 / XiaoZhi Main Migration | HOLD |

---

## 2. 已完成直接保留

### DONE-001 Platform Audit
保留 `CLAW4_AUDIT.md`、`HARDWARE_ASSUMPTIONS.md`、`CLAW4_PLATFORM_MAP.md`。

### DONE-002 Build Baseline
继续使用 Metalio 5.5.4 当前可复现基线，不因 XiaoZhi 已到 IDF 6 就升级。

### DONE-003 Hardware Evidence
已确认 P4 dual-core、360MHz、32MB PSRAM、正常启动链。Display/Touch/C5/Audio/Flash 等仍按证据等级继续验证。

### DONE-004 Host Presenter
Home/Focus/Done/Offline 保留。后续只新增 Paused、Recovery、Voice/Confirmation Overlay。

---

## 3. 强门槛：WB-HOST-FINAL-FIX-V3

状态：READY。进入任何真机 Integration 前必须完成。

### FIX-01 Task lifecycle
- 今日可执行任务默认 ready。
- Start 仅允许 Ready；Paused 只能 Resume。
- Backend/C++/fixture 状态字符串统一。

### FIX-02 Today authoritative snapshot
- server=[] 清非 active 旧任务。
- active task/session 保留。
- version 不回退；date rollover 明确。

### FIX-03 Dashboard daily metric
- 家庭本地日边界。
- StudySession 按 started_at 归属日。

### FIX-04 Reboot monotonic
- monotonic anchor 不跨 boot。
- Running reboot → Recovery/Paused。
- Resume 重新 anchor。

### FIX-05 Auth Pause
- PausedAuth 后 transport 不再 send。
- 显式新凭据后恢复。

### FIX-06 ACK prefix
- 只删除连续 Accepted/Duplicate。
- Rejected/Conflict/Gap/missing 立即停止。

### FIX-07 DeadLetter persistence
- markDeadLetter fail → StorageError；不推进 ACK。

### FIX-08 Backend uniqueness
- UNIQUE(device_id, sequence)。
- 保留 UNIQUE(device_id, event_id)。

### FIX-09 PWA token restore
- reload 后 ApiClient 与 session auth state 一致。

### FIX-10 completed event task_id
- manual/auto_saved/aborted 全部带 task_id。

### FIX-11 Cross-layer Contract
`Backend Task JSON → shared fixture → C++ mapping → Reducer events → Backend ingest`。

### FIX-12 Stability
C++×5、backend×5、PWA×3、Host E2E×5、pip check、npm audit。

完成条件：`HOST_MVP_FINAL_FIX=PASS`。

---

## 4. WB-XIAOZHI-UPSTREAM-001

状态：Host Final Fix 后 READY。

新增 `docs/XIAOZHI_UPSTREAM_TRACKING.md`，分类：
- KEEP_METALIO：Claw4 board、720 UI、C5 Hosted、蓝牙音频、Power/4G、SetVoiceUiDesired。
- TRACK_UPSTREAM：MCP、AudioService、Protocol、P4/ESP-SR、security/input validation、DeviceStateMachine。
- BACKPORT_CANDIDATE：经过差异评估值得单项引入的修复。
- HOLD_MIGRATION：IDF 6、XiaoZhi 整仓 merge、替换 Metalio BSP。

每个候选 backport 输出风险/收益/冲突/测试要求。

---

## 5. WB-METALIO-INTEGRATION-003

重新核对真实插入点，不凭架构猜：
1. Home app registry。
2. screen load/unload 生命周期。
3. LVGL thread ownership。
4. Application::Schedule。
5. STT message handling。
6. MCP → McpServer::ParseMessage。
7. McpServer tool registration。
8. SetVoiceUiDesired。
9. AudioService。
10. HTTP/Network。
11. NVS/Storage。
12. firmware size/partition headroom。

输出 `docs/METALIO_LEARNING_INTEGRATION_MAP_V3.md`。

---

## 6. WB-INTERACTION-001

新增设备无关 interaction 模块，Command 至少包含：StartTask、PauseTask、ResumeTask、RequestCompleteTask、ConfirmCompleteTask、QueryTodayTasks、QueryCurrentTask、QueryRemainingTime、QueryTodayProgress。

Host Test 验证 Touch/STT/MCP 入口生成同一业务语义。

---

## 7. WB-LEARNING-MCP-001

Host/mock 阶段先实现：
- learning.get_today_tasks
- learning.get_current_task
- learning.get_remaining_time
- learning.get_today_progress
- learning.start_task
- learning.pause_task
- learning.resume_task
- learning.request_complete_task

规则：MCP 只生成 Command，不直接改 state/outbox/backend，不提供无确认的 complete。

---

## 8. WB-PLATFORM-PORTS-001

建议 Ports：Clock、Storage、Network、Ui、VoiceSession、Stt、McpRegistration、Power。

Learning Core 不 include Metalio header。

---

## 9. WB-METALIO-ADAPTER-001

落点：`integration/metalio_claw4/`。

适配 App entry、Presenter→LVGL、Touch→Interaction、Clock、Storage、Network、STT hook、SetVoiceUiDesired、MCP registration、Student Mode 最小入口。

当前只要求 compile/link，不要求 Flash。

---

## 10. WB-FW-BUILD-V3-001

基于 Metalio 5.5.4 编译。

禁止修改 sdkconfig、partition、IDF 主版本，禁止刷机。

输出 firmware size、baseline delta、partition fit、warnings、RAM/PSRAM 风险、touched Metalio files。

---

## 11. USER-FLASH-AUTH-V3

任何写入前必须给用户：目标 partition/地址、binary/size、覆盖范围、原厂恢复、失败恢复、ota_0/ota_1 风险、ESPClaw 影响、串口观察项和验收步骤。

用户明确授权前状态始终 `BLOCKED_USER_AUTH`。

---

## 12. WB-DEVICE-TOUCH-MVP-001

先 Touch，不接 Voice：
`Boot → Learning Home → Today Tasks → Start → Focus → Pause → Resume → Complete → Done`。

采集 heap、PSRAM、largest block、FPS、touch latency、10/30/60 分钟稳定性。

---

## 13. Real NVS / Network / Offline

### WB-NVS-OUTBOX-001
Transactional persistence、power-cycle recovery、真实 reboot monotonic 恢复。

### WB-NETWORK-001
C5 Wi-Fi、HTTPS、device auth、task fetch、event batch sync、auth pause。

### WB-OFFLINE-E2E-001
`在线拉任务 → 断网 → 学习并完成 → 本地保存 → 重启恢复 → 网络恢复 → 自动同步 → PWA 可见`。

这是 Voice 前的强门槛。

---

## 14. Voice Phase

### WB-VOICE-LIFECYCLE-001
复用 `SetVoiceUiDesired()`，压力测试 Learning/Home/Chat 页面切换、heap、CPU、wakeword。

### WB-STT-INTENT-001
第一批：暂停、继续、还有多久、今天还有几个任务、当前任务。

### WB-MCP-DEVICE-001
把 Host 验证过的 Learning MCP tools 注册进 Metalio McpServer；不要重写 MCP protocol。

### WB-COMPLETE-CONFIRM-001
完成任务必须用户二次确认，模型不能静默完成。

---

## 15. AI Coach

状态 HOLD。只有 Touch real MVP、NVS/offline、network/auth、Voice lifecycle、STT、MCP safe tools 全稳定后开始。

AI 只做拆解、建议、复盘、摘要，不拥有 Task/StudySession 权威。

---

## 16. 当前明确 HOLD
- XiaoZhi 2.4.x 整体迁移。
- IDF 6 升级。
- Camera AI / 情绪识别。
- 本地大模型。
- 手机学生端。
- 排行榜/社交。
- 双系统重构。

---

## 17. 推荐执行顺序

`HOST FINAL FIX → 文档/TASK_BOARD → Upstream Tracking → Metalio Recheck → Interaction → Learning MCP Host → Platform Ports → Adapter → Build Only → Flash Plan → USER AUTH → Touch MVP → NVS/Network/Offline → Voice Lifecycle → STT → MCP → Complete Confirmation → Real E2E → AI Coach`