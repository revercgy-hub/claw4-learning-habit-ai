# 项目任务看板

- 更新时间：2026-09-04（L1c 持久化复测通过；切换 App-first 批量开发）
- 维护者：Codex；2026-09-04 起 WorkBuddy 暂停调度，Codex 直接实施与复检，验收决策归用户
- 当前阶段：L1 ready 基线 `codex/wb-learning-v4-l1-ready` @ `4db2283` 已完成 COM7 application-only 复测，限定结论 `DEVICE_L1C_PERSISTENCE=PASS` / `CHECKPOINT_READY`；App-first 活动分支为 `codex/app-first-mvp-loop` @ `ea3afb2`。`CODEX-APP-FIRST-001` 的 AF0、AF1a 已通过，正在进入 AF1b；先在 App/Host 模式完成 L2/L3 MVP 主链路，再安排一次用户主导的阶段末真机验收。
- 调度规则：当前无 WorkBuddy 活动流；Codex 按 App-first checkpoint 连续实施，单功能不刷机，阶段 gate 通过后只冻结一个真机候选
- 项目远端：[`revercgy-hub/claw4-learning-habit-ai`](https://github.com/revercgy-hub/claw4-learning-habit-ai)（私有）

## 当前活动工作流

**2026-09-04 当前项：** `WB-LEARNING-V4-L1c` 已在整合头 `4db2283` 完成限定持久化复测：镜像 SHA-256 `6d27653a…5722aa1f` 写入 `0x200000` 后 Hash verified；用户完成屏侧测试；只读 NVS 证据显示 sequence 1..20 连续、20 个有效 event ID 全唯一，Start/Pause/Resume/Complete 与 StudySession 事件均已落盘并在复位后读回。物理操作期间未保持 monitor，因此不声明逐步串口 SELFTEST PASS。当前不再调度 WorkBuddy；下一活动工作流是 Codex 的 `CODEX-APP-FIRST-001`，先完成 App/Host L2/L3 全链，阶段末再做一次真机。禁止项（erase/partition/bootloader/ota_1/C5/eFuse）不变。

`CODEX-APP-FIRST-001` 的批次范围与门禁见 `docs/project_management/tasks/CODEX-APP-FIRST-001_MVP_FULL_LOOP.md`：家长 PWA 建任务 → Backend → 真实 C++ LearningApp 拉取 → 离线学习/重启恢复 → 恢复联网同步/ACK → PWA 恰好一次可见。L4 Voice、L5 MCP 真机注册与 L6 AI Coach 不混入本批。

`WB-LEARNING-V4-HOST`（分支 `workbuddy/learning-v4-host-sync`，基线 planning-v4 `1310ca3d`）曾作为唯一活动工作流，已依据 `项目总规划/WORKBUDDY_CLAW4_学习伙伴_完整开发提示词_V4.md` §3~§9 连续完成：

1. §3 Host Final Fix 正式标志：`HOST_MVP_FINAL_FIX=PASS`（12 项 checklist 落 `HOST_MVP_ACCEPTANCE.md` §0）；
2. §4 Project Fact Sync：同步 TASK_BOARD / ARCHITECTURE / AGENTS / acceptance（Host MVP 与 Device MVP 分开）；
3. §5 XiaoZhi Upstream Tracking：新增 `docs/XIAOZHI_UPSTREAM_TRACKING.md`（只 selective backport，禁止整仓 merge）；
4. §6 Metalio Integration Recheck：输出 `docs/METALIO_LEARNING_INTEGRATION_MAP_V4.md`；
5. §7 Interaction Router（P14）：`firmware/main/interaction/` CommandDispatcher 统一漏斗 + STT mapper（`35e3d45`）；
6. §8 Learning MCP Host（P15）：8 个 `learning.*` 工具，AI 禁止直接 Complete（`1467527`）；
7. §9 Platform Ports（P16）：8 抽象 Port + 确定性 fakes，Learning Domain 零 Metalio/IDF 头（`e10a7e1`）。

截至 2026-09-03：**阶段 1（§3~§6）+ 阶段 2（§7~§9/P14–P16）+ Host 契约修正（WB-V4-HOST-CORRECTION，FIX-V4-01~03）均已完成并推送**（阶段 1 `f8513f1`→`3e72cd8`；阶段 2 `a862cb0`→`6b787d1`；修正 C19 `bba9356` FIX-V4-01/02/03 + C20 `9f62c12` 验收重声明 `HOST_MVP_FINAL_FIX_V4=PASS`；远端 `workbuddy/learning-v4-host-sync`，`current_remote_head=9f62c12`）。host gate 8/8、**152 case 0 失败**；backend 70/70；PWA 30/30；E2E PASS；cold clone PASS（见 `HOST_MVP_ACCEPTANCE.md` §0.1 与 `WB-LEARNING-V4_REPORT.md` §12）。§6 已源码级核实 `openclaw_screen` 非学习宿主。**下一步（已由 `WB-LEARNING-V4-NEXT` 授权到代码+编译）：P17 Metalio Adapter（`integration/metalio_claw4/`，P17a→P17d）+ P18 Learning App Shell BUILD ONLY；真机 app-flash 仍需单独批次授权（`FLASH_AUTH_REQUIRED`；决策/边界材料：`docs/METALIO_ADAPTER_BRIDGE_PLAN_P17.md`）。**

本工作流不触碰真机、LVGL、真实 NVS/网络适配、Flash/分区或发布固件；Device MVP（Bring-up、App Shell、L0~L6）保持 `HOLD` 直至用户另行授权。

`WB-STREAM-002`（`workbuddy/domain-offline-stream`）已收口：CP0~CP8 全部 `CHECKPOINT_READY` 并推送，Review 修复（FIX-03/08/10 + TaskNotReady）推送至分支（看板记录头 `2787fb9`；planning-v4 `1310ca3d` 承载其收口代码全量）；`CR-WB-STREAM-002-FINAL` 因用户取消 Codex 复检 `CANCELLED`。流收口证据以 `WB-STREAM-002_REPORT.md`（§10/§11）与 `HOST_MVP_ACCEPTANCE.md`（§0 `HOST_MVP_FINAL_FIX=PASS`）为准，验收决策归用户。

## 看板

> WB-STREAM-002 流内条目（6.7~6.16）状态保持收口时点 `CHECKPOINT_READY` 记录；流级已收口（见上节），验收决策归用户。
> 序号 10/12/15/16/17 为 WB-STREAM-002 拆解前的旧规划条目，工作已被对应 CP 吸收，标记 `SUPERSEDED`（不删除历史行）。

| 顺序 | 任务 ID | 负责人 | 状态 | 依赖 | 交付物/证据 |
| ---: | --- | --- | --- | --- | --- |
| 0 | CTRL-001 | Codex | `ACCEPTED` | 无 | 根协作规范、看板、模板、Git 边界 |
| 1 | AUD-001 | 历史产出 / Codex 复核 | `ACCEPTED` | 无 | `docs/CLAW4_AUDIT.md`、`docs/HARDWARE_ASSUMPTIONS.md` |
| 2 | BLD-001 | 历史产出 / Codex 复核 | `ACCEPTED` | AUD-001 | `docs/BUILD.md`、`docs/CLAW4_主机准备情况报告_2026-09-01.md`、`E:\b` 构建产物 |
| 2.1 | WB-ENV-REVIEW | WorkBuddy / Codex 复核 | `ACCEPTED` | BLD-001 | `docs/CLAW4_报告复核_2026-09-01.md`，独立复跑主机检查 |
| 3 | WB-001 | WorkBuddy | `ACCEPTED` | AUD-001、BLD-001 | 最终实施提交 `45b2c73`；`docs/CLAW4_PLATFORM_MAP.md` 与 `WB-001_REPORT.md` 已验收并纳入 main |
| 4 | CR-001 | Codex | `ACCEPTED` | WB-001 `REVIEW_READY` | `CODEX_REVIEW_WB-001_2026-09-01.md`；本轮结论为 `CHANGES_REQUIRED` |
| 4.1 | CR-001-R2 | Codex | `ACCEPTED` | WB-001 修订提交 `a829765` | `CODEX_REVIEW_WB-001_ROUND2_2026-09-01.md`；技术内容通过，证据行号仍需窄范围修订 |
| 4.2 | CR-001-FINAL | Codex | `ACCEPTED` | WB-001 最终提交 `45b2c73` | `CODEX_REVIEW_WB-001_FINAL_2026-09-01.md`；G1 `PASSED` |
| 4.5 | WB-HW-001 | WorkBuddy | `ACCEPTED` | WB-001、设备 USB 枚举 | 实施提交 `9867a56`；COM/USB 映射、官方固件日志、`BOARD_REVISION.md`、`DEVICE_LOG_REFERENCE.md` |
| 4.6 | CR-HW-001 | Codex | `ACCEPTED` | WB-HW-001 `REVIEW_READY` | `CODEX_REVIEW_WB-HW-001_2026-09-01.md`；原始证据哈希与日志结论通过 |
| 4.7 | USER-HW-EVIDENCE-001 | 用户 / Codex 记录 | `ACCEPTED` | WB-HW-001 | `CODEX_USER_HW_EVIDENCE_2026-09-01.md`；4 张仓库外照片及一次 COM3 打开授权 |
| 4.8 | WB-HW-002 | WorkBuddy | `ACCEPTED` | USER-HW-EVIDENCE-001 | 初次提交 `63281b2`、修订提交 `9fe38c3`；一次受控启动采集与照片证据已验收 |
| 4.9 | CR-HW-002 | Codex | `ACCEPTED` | WB-HW-002 初次 `REVIEW_READY` | `CODEX_REVIEW_WB-HW-002_2026-09-01.md`；结论 `CHANGES_REQUIRED`，禁止新增硬件操作 |
| 4.10 | CR-HW-002-R2 | Codex | `ACCEPTED` | WB-HW-002 修订提交 `9fe38c3` | `CODEX_REVIEW_WB-HW-002_ROUND2_2026-09-01.md`；技术内容通过，保留原采集脚本未留档的过程限制 |
| 5 | WB-002 / STREAM-001 CP0 | WorkBuddy | `ACCEPTED` | WB-HW-002 `ACCEPTED` | 提交 `72b77ee`；契约收口并经 Codex 复检/修复 |
| 6 | CR-002 | Codex | `ACCEPTED` | WB-002 初次 `REVIEW_READY` | `CODEX_REVIEW_WB-002_2026-09-02.md`；结论 `CHANGES_REQUIRED` |
| 6.1 | CR-002-R2 | Codex | `ACCEPTED` | WB-002 修订提交 `6251486` | `CODEX_REVIEW_WB-002_ROUND2_2026-09-02.md`；结论 `CHANGES_REQUIRED` |
| 6.2 | CR-002-R3 / CR-WB-STREAM-001 | Codex | `ACCEPTED` | WB-STREAM-001 checkpoint `bc41ff3` | `CODEX_REVIEW_WB-STREAM-001_2026-09-02.md`；`ACCEPTED_WITH_CODEX_FIXES` |
| 6.5 | WB-STREAM-001 | WorkBuddy | `ACCEPTED` | 用户连续开发授权；远端工作流分支 | CP0 `72b77ee`、CP1 `a38dfad`、CP2 `bc41ff3` |
| 6.6 | CR-WB-STREAM-001-FIX | Codex | `ACCEPTED` | WB-STREAM-001 checkpoint 冻结 | `f021233`；身份/绑定、设备级并发、challenge 原子消费、event envelope、outbox 接口修复，pytest 28/28×5 |
| 6.65 | CR-WB-STREAM-002-PREFLIGHT | Codex | `ACCEPTED` | WB-STREAM-002 未开始 | sequence 单一所有者、LLVM 来源/SHA 与空分支状态预检；`CODEX_PREFLIGHT_WB-STREAM-002_2026-09-02.md` |
| 6.7 | WB-STREAM-002 / CP0 | WorkBuddy | `CHECKPOINT_READY` | 最新调度基线（历史包含 `f021233`） | 提交 `690d848`；本机 C++17 compile/link/run 门槛 PASS；`WB-STREAM-002_REPORT.md` |
| 6.8 | WB-STREAM-002 / CP1 | WorkBuddy | `CHECKPOINT_READY` | CP0 checkpoint | 提交 `0998447`；纯领域 reducer、状态机与主机单测 27/27 PASS |
| 6.9 | WB-STREAM-002 / CP2 | WorkBuddy | `CHECKPOINT_READY` | CP1 checkpoint | 提交 `b16b739`；平台无关 transactional outbox，outbox 21/21 + domain 27/27 PASS |
| 6.10 | WB-STREAM-002 / CP3 | WorkBuddy | `CHECKPOINT_READY` | CP2 checkpoint | 提交 `179fd6c`；设备应用协调器，coordinator 20/20 PASS |
| 6.11 | WB-STREAM-002 / CP4 | WorkBuddy | `CHECKPOINT_READY` | CP3 checkpoint | 提交 `9220f38`；Home/Focus/Done/Offline 纯 presenter 28/28 PASS；不含 LVGL |
| 6.12 | WB-STREAM-002 / CP5 | WorkBuddy | `CHECKPOINT_READY` | CP4 checkpoint；Mock Backend | 提交 `d238e1d`；SQLAlchemy 持久化家庭后端，backend 58/58 PASS |
| 6.13 | WB-STREAM-002 / CP6 | WorkBuddy | `CHECKPOINT_READY` | CP5 checkpoint | 提交 `07d4ab9`；家长 PWA 四页，typecheck/vitest 30/30/build PASS |
| 6.14 | WB-STREAM-002 / CP7 | WorkBuddy | `CHECKPOINT_READY` | CP6 checkpoint | 提交 `79900f4`；主机 MVP E2E 闭环 PASS（backend 62/62） |
| 6.15 | WB-STREAM-002 / CP8 | WorkBuddy | `CHECKPOINT_READY` | CP7 checkpoint | 提交 `36d2e13`+`028a544`+`52481dd`；C++/Backend 5 轮、PWA 3 轮、E2E 5 轮 0 失败；`HOST_MVP_ACCEPTANCE.md` 交付 |
| 6.16 | WB-STREAM-002 / Review 修复 | WorkBuddy | `CHECKPOINT_READY` | CP8 checkpoint | 提交 `7e7fa07`+`c4a3bed`+`2787fb9`；FIX-03/08/10 + TaskNotReady；C++ 28/28、backend 70/70、E2E PASS；报告 §11 |
| 6.17 | WB-LEARNING-V4-HOST / §3~§6 | WorkBuddy | `CHECKPOINT_READY`（流级；验收决策归用户） | planning-v4 `1310ca3d` | 提交 `f8513f1`（§3 PASS 标志 + §4 Fact Sync）+`ef5d22d`（§5 tracking）+`cbdd5ba`（§6 integration map）推送 `workbuddy/learning-v4-host-sync`；`WB-LEARNING-V4_REPORT.md`；openclaw 源码级核实 |
| 6.18 | WB-LEARNING-V4-HOST / §7~§9（P14–P16） | WorkBuddy | `CHECKPOINT_READY`（流级；验收决策归用户） | `3e72cd8`（阶段 2 任务包基线） | 任务包 `WB-LEARNING-V4_INTERACTION_MCP_PORTS.md`（`a862cb0`）；P14 interaction `35e3d45`（含 mapFocusTap task_id 修正 P14.1）；P15 learning mcp host `1467527`；P16 platform ports `e10a7e1`；host funnel 集成测试 `a4d35a`（learning.* → 真实 coordinator 全链 6 case）；host gate 8/8、**147 case 0 失败**（C15 边缘用例：18/16/8）、4b3 include 扫描 PASS、target-ISA impl 语法 3/3；跨语言 E2E 复跑 `E2E RESULT: PASS`（backend 70 + PWA 30 + build，`a04900c` 后补记）；报告 §11 |
| 6.19 | WB-V4-HOST-CORRECTION（FIX-V4-01~03，阶段 A/B） | WorkBuddy | `CHECKPOINT_READY`（流级；验收决策归用户） | `6b787d1`（fetch 核实基线） | C19 `bba9356`：FIX-V4-01 auth_pause 短路+`resetAuthPause`、FIX-V4-02 权威快照 `applyTodaySnapshot`、FIX-V4-03 deadletter 持久化失败中止 ACK cleanup；C20 `9f62c12`：`HOST_MVP_FINAL_FIX_V4=PASS`（§0.1 三项证据）；host gate 8/8、152 case 0 失败、backend 70、PWA 30、E2E PASS、cold clone PASS；报告 §12 |
| 6.20 | P17a LearningApp glue（阶段 E cp1） | WorkBuddy | `CHECKPOINT_READY`（流级；验收决策归用户） | `911e5c2` | C23 `31cb6b8`：`integration/metalio_claw4/host_glue/`（ContextBuilder/CommandSinkGlue/BackendGlue/LearningApp）+ glue 测试 4 case + `integration_manifest.md`（0 官方改动）+ harness 纳入 host_glue；host gate 9/9、**156 case 0 失败**；报告 §13 |
| 6.21 | P18 Learning L0 App Shell BUILD（阶段 F） | WorkBuddy | `LEARNING_V4_L0_BUILD_READY` → 批次授权已获 → **首刷 PASS + Start 交互 PASS（C26/C27）** | `911e5c2` | C25：`integration/metalio_claw4/device/learning_screen/`（权威副本）+ manifest 两条 APPLIED；`E:/b/xiaozhi.bin` = **9,025,520 B（delta +2,096 B）**、sdkconfig diff=0（调优基线未动）、idf.py build exit=0、learning 源入固件；ota_0 余量 ≈0.39 MiB；报告 §14/§14.2/§14.3 |
| 6.22 | WB-LEARNING-V4-NEXT 全任务收口（阶段 A–F） | WorkBuddy | `CHECKPOINT_READY`（任务书授权范围全部完成；验收决策归用户） | `6b787d1`（fetch 核实基线） | C19–C27 链：FIX-V4-01~03 → `HOST_MVP_FINAL_FIX_V4=PASS` → Fact Sync → P17 集成策略 → P17a glue（host gate 9/9、156 case）→ P18 L0 BUILD（+2,096 B、sdkconfig diff=0）→ 首刷 PASS → Start 交互修复复测 PASS（9,026,000 B）；报告 §12–§15 |
| 6.23 | WB-LEARNING-V4-L1 Learning App 设备基本功能 | WorkBuddy 历史实现 + Codex 修复/真机复测 | `CHECKPOINT_READY`（`DEVICE_L1C_PERSISTENCE=PASS`；验收决策归用户） | WorkBuddy `7ee686d`；Codex ready `4db2283` | 根因/修复仍见 HANDOFF §7/§9。COM7 application-only 写入 9,175,856 B / SHA-256 `6d27653a…5722aa1f`，Hash verified；用户完成屏侧测试；只读 NVS：learning blob/CRC OK、`S|0`、`E|20`、seq 1..20、20 个 `ev-[0-9a-f]{16}` 全唯一，六类 transition 事件均存在且复位后可读。无逐步 monitor 证据的限制、I2C 独立风险见 HANDOFF §10 与 `CODEX_WB_LEARNING_V4_L1_DEVICE_TEST_2026-09-04.md`。 |
| 6.24 | CODEX-APP-FIRST-001 / L2-L3 MVP 全链批次 | Codex | `IN_PROGRESS`（AF0、AF1a `CHECKPOINT_READY`；AF1b 下一步；WorkBuddy 暂停） | 6.23 `CHECKPOINT_READY` | AF0：统一 Quick gate + Virtual Device runner PASS；AF1a：target-portable challenge/auth/today/events/ACK codec，host 12/12、interface 34/34+3/3+1/1 PASS，提交 `ea3afb2`，报告 `CODEX_APP_FIRST_AF1A_2026-09-05.md`。完成 AF0~AF4 后才允许 AF5 真机。 |
| 7 | WB-BRINGUP-S1 | WorkBuddy | `BACKLOG` | WB-HW-001；恢复路径；涉及刷写时需用户明确授权 | B001/B002/B003/B004/B005/B009/B013 + `BRINGUP_STAGE1_REPORT.md` |
| 8 | CR-BRINGUP-GATE | Codex | `BACKLOG` | WB-BRINGUP-S1 `REVIEW_READY` | Stage 1 复检和 GO/NO-GO |
| 9 | WB-MVP-INTERFACES / STREAM-001 CP1 | WorkBuddy | `ACCEPTED` | CP0 checkpoint | 提交 `a38dfad`，Codex 接口修复并入 `f021233` |
| 10 | WB-MVP-DOMAIN / STREAM-002 CP1 | WorkBuddy | `SUPERSEDED` | WB-STREAM-002 CP1（行 6.8） | 已被 WB-STREAM-002 CP1 吸收：Task/StudySession/DeviceEvent/DomainState reducer 与主机单测（`0998447`） |
| 11 | WB-MVP-MOCK / STREAM-001 CP2 | WorkBuddy | `ACCEPTED` | CP1 checkpoint；CP0 契约 | 提交 `bc41ff3`，Codex 回归修复 `f021233`；pytest 28/28×5 |
| 12 | WB-MVP-UI-PRESENTER / STREAM-002 CP4 | WorkBuddy | `SUPERSEDED` | WB-STREAM-002 CP4（行 6.11） | 已被 WB-STREAM-002 CP4 吸收：Home/Focus/Done/Offline 纯主机 presenter 28/28（`9220f38`）；LVGL 页面仍 `HOLD` |
| 13 | WB-MVP-DEVICE-UI | WorkBuddy | `HOLD` | Bring-up G2/G3 + host presenter 验收 | LVGL Home/Focus/Done/Offline 与真机交互 |
| 14 | WB-MVP-EVENTS | WorkBuddy | `HOLD` | Focus 验收 | task.start、task.complete |
| 15 | WB-MVP-QUEUE / STREAM-002 CP2 | WorkBuddy | `SUPERSEDED` | WB-STREAM-002 CP2（行 6.9） | 已被 WB-STREAM-002 CP2 吸收：平台无关 outbox 21/21（`b16b739`）；真实 NVS/掉电/真机适配仍 `HOLD` |
| 16 | WB-MVP-BACKEND / STREAM-002 CP5 | WorkBuddy | `SUPERSEDED` | WB-STREAM-002 CP5（行 6.12） | 已被 WB-STREAM-002 CP5 吸收：持久化家庭后端 SQLite 测试 + PostgreSQL 部署配置（`d238e1d`） |
| 17 | WB-MVP-PWA / STREAM-002 CP6 | WorkBuddy | `SUPERSEDED` | WB-STREAM-002 CP6（行 6.13） | 已被 WB-STREAM-002 CP6 吸收：家长 Dashboard/今日任务/学习记录/设备四页 PWA（`07d4ab9`） |
| 18 | CR-WB-STREAM-002-FINAL | Codex | `CANCELLED` | ~~CP8 `STREAM_REVIEW_READY`~~ | 用户 2026-09-03 决定不再安排 Codex 复检；流收口证据改由 `WB-STREAM-002_REPORT.md` §11 + `HOST_MVP_ACCEPTANCE.md` 支撑，验收决策归用户 |

## 当前阻塞

| 阻塞 ID | 影响任务 | 证据 | 解除条件 |
| --- | --- | --- | --- |
| BLK-FLASH-AUTH-001 | 固件刷写/Flash 擦除/分区/OTA 操作 | **既有批次仅覆盖已完成的 ota_0 application-only L0/L1 测试**；App-first AF0~AF4 不写真机。erase_flash、bootloader、partition、ota_1、C5、eFuse、Secure Boot、Flash Encryption 仍禁 | AF4 通过后再向用户提交唯一候选与 AF5 屏侧验收单；任何新范围需重新说明风险并取得明确授权 |

## 已解除阻塞

| 阻塞 ID | 状态 | 解除证据 | 结果 |
| --- | --- | --- | --- |
| BLK-GIT-REMOTE-001 | `RESOLVED` | [`GIT_PROJECT_SETUP_2026-09-01.md`](reports/GIT_PROJECT_SETUP_2026-09-01.md) | 已创建独立私有仓库并推送 `main` 与 `workbuddy/wb-001-platform-map`；未使用 Metalio 官方 origin |
| BLK-HW-001 | `RESOLVED` | [`CODEX_DEVICE_INTAKE_2026-09-01.md`](reports/CODEX_DEVICE_INTAKE_2026-09-01.md) | 已检测 COM3/4/5/6；COM3 明确枚举为 Espressif USB JTAG/serial debug unit，可进入只读接收检查 |
| BLK-USER-EVIDENCE-001 | `RESOLVED` | [`CODEX_USER_HW_EVIDENCE_2026-09-01.md`](reports/CODEX_USER_HW_EVIDENCE_2026-09-01.md) | 用户已提供 4 张设备照片并授权下一任务打开 COM3 一次；照片未显示可辨识 SKU 标签，相关字段继续标记待确认 |

## 已知风险

- 官方分区 `ota_0` 约 9 MiB、`ota_1` 约 4 MiB，当前应用约 8.62 MiB，不能假定双槽对称 OTA 可用。
- 官方 `sdkconfig` 中 Flash mode 选择项与字符串值存在不一致迹象，WorkBuddy 只能记录，不能擅自修正。
- 真机 SKU、屏驱、PSRAM、Flash、C5、触摸等均需实机确认。
- 历史 `docs/系统检查报告_2026-09-01.md` 已被后续主机准备报告取代，不得继续据其重新安装工具链。
- 历史 WorkBuddy 环境曾出现共享对象库 refs 竞争；后续以独立 `codex/` 工作树、普通 checkpoint push 和远端 SHA 为证据，不复用过时的 `current_remote_head` 表述。
- 本地测试环境为易失 scratch：backend/.venv 与 frontend/node_modules 随时可能被清理，复跑 E2E 需先重建（pip ~2m / `npm ci` ~4m，经代理 51846；`.gitignore` 已含 `**/node_modules/`、`**/dist/`）。
- 真机 COM7 已完成 L0 与 L1c 本地持久化验证；L1c 的 NVS/重启读回限定为 `DEVICE_L1C_PERSISTENCE=PASS`。后续曾观察到 GT911/TCA95xx/BQ27220 短暂 I2C timeout，单列 `HARDWARE_VERIFY_REQUIRED`；Wi-Fi/TLS、音频（mic/唤醒词互斥）和真实 Backend 真机链仍待 App-first 阶段末一次性验证。
