# Claw4 项目协作总则

## 2026-09-24 用户指令覆盖：开始 M1 开发准备

最新 M1 Voice 调度：用户指定 L-03 根看板同步、L-04 证据模板/解析、S-04 固件与真实布局 Preflight 准备；独立 R-02 `PASS` 后才由唯一硬件 owner S-05 执行 2～3 轮真机 Voice Preflight。Preflight 不计 M1 PASS；稳定后才从第 1 轮开始正式连续 20 轮及故障矩阵，正式运行期间任何固件修改都使当次轮次作废并从第 1 轮重来。AP outage/recovery 已由用户跳过，保持 `SKIPPED_BY_USER / NOT_VERIFIED`，不得重复派发。构建应适配真实 live 布局，禁止通过写入或改动设备 partition table 来迁就固件；bootloader、ota_1、eFuse、Secure Boot、Flash Encryption、数据丢失风险仍为停止条件。S-04/R-02 禁止硬件写入；S-05 的设备操作只在 R-02 PASS 和唯一 owner 条件下按已审查 Candidate 的精确范围执行。

用户明确要求跳过 Candidate20 的 AP outage/recovery 刺激并直接开始下一阶段开发。AP failure/recovery 保持 `SKIPPED_BY_USER / NOT_VERIFIED`，A-02 仍是 `M0=CHANGES_REQUIRED`。2026-09-24 用户又明确提供铭凡 N5 x86 NAS 和 SSH 访问，授权部署小智服务；该授权覆盖 NAS 上隔离的 Docker 服务与合成内容连通性验证，不涵盖儿童数据上传、设备重刷或扩大 Flash/recovery 范围。NAS 部署不构成 M0 PASS 或 M1 20 轮 PASS。当前活动项见 [V6_TASK_BOARD](docs/v6/V6_TASK_BOARD.md)、[M1 计划](docs/v6/V6_M1_XIAOZHI_NAS_VOICE.md) 和 [NAS 部署报告](docs/v6/V6_M1_NAS_DEPLOYMENT_REPORT.md)。此条只覆盖下文 2026-09-23 调度中的 M1 阶段状态，不改变 M0 证据契约和剩余验收项。

## 2026-09-23 接管实施规则（当前唯一有效调度）

本节依据用户本轮 T-00 → A-01 → 第一实施波 → R-01 → S-03 → A-02 指令，覆盖下方历史角色、READY 入口、复检取消和设备授权表述。下方历史调度全部 **SUPERSEDED**；技术事实按候选及证据范围保留，不删除或补盖 PASS。

### 基线和当前事实

- 接管基线：`760b2aa66819f8d90186b43af4c02d9b33c8849e`。T-00 在干净的 `E:/workbuddy/claw4-v6` 核对 HEAD 后，以普通 push 创建 `origin/takeover-v6-m0-c19-baseline`，随后 GitHub 按分支名重新读取的 SHA 与本地一致，`BASELINE_PROTECTED=YES`。禁止 force push、rebase 或改写保护分支历史。
- `main` 与 `workbuddy-v6-m0-candidate08-review @ 28bbda911e6074fca4930dfca3263060a59e1215` 均为历史基线，不是实施入口。后续独立工作树从 A-01 或明确列出的已审查集成 SHA 出发，不使用 HEAD 损坏的旧中文目录。
- Candidate19 (`claw4-learning-v6-m0.19`) 已构建、**未刷机**。Candidate20 已 app-only 写入 live `ota_0` 并完成 app hash 读回；Candidate20 DEVICE 仅部分观察，M0 缺口及用户跳过的 AP failure/recovery 见 [A-02 Gate](docs/v6/V6_M0_CANDIDATE20_GATE.md)。当前 `M0=CHANGES_REQUIRED`；M1 NAS 服务已部署并做合成连通性验证，设备 20 轮验证未开始。Candidate17 的 DEVICE PASS 不继承给 Candidate19/20。83 项是接管基线的 Host 工具测试数，不是设备验收数；后续数量以各提交实跑为准。
- 旧 Candidate08 网络重连同步返回处理和诊断字段问题已由后续提交修复，不重新实施。AEC 仍未通过，旧 `clipped=0` 不构成削波证据。

### 模型、所有权和升级

- **Astra**：亲自完成 A-01/A-02，维护架构、ADR、公共接口、证据契约、M0/M1/M2 Gate、高风险 Flash/恢复决策与整合；本轮不改变 V6 ADR-001～005。
- **Sol**：S-01 Audio/AEC，S-02 Network/Camera 及复杂生命周期问题；R-01 另派未参与实施的 `sol_reviewer`。S-03 由单一 Sol owner 执行。
- **Luna**：L-01 状态文档；L-02 有界工具修复和测试。第一次失败可根据新失败证据修复一次，第二次失败立即升级 Sol；涉及架构/公共契约则交 Astra 决策，不能无限重试或扩大白名单。
- 最多四个 Agent 同时运行（包括 Astra）。每项使用独立工作树、分支和独立 Host 输出目录；同一文件只有一个 owner。若三个实施槽已满，S-02 等空槽后启动。
- 每项提交只包含自己的允许文件，返回实际文件、测试命令、精确测试/失败数、commit SHA、未验证项。不得自行验收、合 main 或扩大范围；不回滚别人的改动。
- **COM7、Claw4 真机、E:/v6/s1、Candidate 镜像、Flash 与 NVS recovery 是一个排他资源集合，只能有一个执行 owner**。第一实施波与 R-01 无 owner、全部禁止访问这些资源；R-01 PASS 后才交 S-03。Build → identity verify → flash → readback/evidence → device matrix 串行，不并行 build/flash。

### 第一实施波 Task Contract

所有路径相对各自独立工作树；白名单外文件禁止修改，第三方源码/固件/NVS/分区和设备操作禁止。A-01 只修改本文件及 `docs/v6/V6_ARCHITECTURE.md`。

| ID / owner | 允许修改文件 | 验证与停止条件 |
| --- | --- | --- |
| L-01 / Luna | `docs/project_management/TASK_BOARD.md`、`docs/v6/V6_TASK_BOARD.md`、`docs/v6/V6_M0_TEST_PLAN.md` | 核对链接、SHA、当前状态；旧 READY 标 SUPERSEDED；USB reset 不称 cold boot；不得改变架构 |
| L-02 / Luna | `tools/v6/hw_matrix.py`、`tools/v6/test_hw_matrix.py` | `py -3.14 -B -m unittest discover -s tools/v6 -p "test_*.py"`；拒绝缺身份、错误/混合候选；续段只能继承已验证会话，遵守架构文档的既有证据协议 |
| S-01 / Sol | `integration/v6/board/claw4-learning-v6/claw4_audio.cc`、`claw4_audio.h`、`board_algorithms.h`、`m0_diagnostics.cc`（后三者同目录）；`tools/v6/test_board_algorithms.cc` | C++17 `-Wall -Wextra -Werror`；队列开始/结束、溢出/欠载、提前结束、旧 session 写入和连续周期；不得盲调 gain/delay 或宣称 Host 证明 AEC |
| S-02 / Sol | `integration/v6/board/claw4-learning-v6/claw4_board.cc`、`integration/v6/network/overlay.json`、`tools/v6/network_station_fixture.cc`、`tools/v6/test_network_station.py`；直接相关新增测试仅 `tools/v6/test_camera_cleanup.py`、`tools/v6/camera_cleanup_fixture.cc` | 独立 Host 依赖夹具和输出目录；网络生产方法错误返回/有界恢复；实际 Camera 生产清理路径故障注入；不得读写 E:/v6/s1 或仅复制一套伪生产算法来测试 |

全部实施完成并整合后，独立 R-01 对不可变集成 SHA 的四项 diff、测试、证据身份、AEC claim、生命周期、ADR 和跨任务语义冲突进行只读复核。`CHANGES_REQUIRED` 只退回原 owner 定向修复；仅 `R-01=PASS` 才能启动 S-03。

### S-03 与最终 Gate

S-03 冻结一个全新 Candidate ID，记录已审查 source SHA、源码/工具链/依赖/配置输入、镜像完整 SHA、manifest 与日志会话绑定。不得沿用 Candidate17/18/19 的 DEVICE PASS。普通设备批次授权按用户本轮 S-03 指令执行，但必须先核对实际布局、设备身份、备份和精确 application 写入范围。

恢复写回须由 Astra 在执行前明确其具体安全范围；没有完成范围审定时记 NOT_VERIFIED，不自行扩大。partition、bootloader、ota_1、eFuse、Secure Boot、Flash Encryption，以及数据丢失风险均触发停止并报告用户。不存在用历史全片写回方案自动取得新授权的例外。

最终 A-02 同时核对 CODE / HOST / BUILD / DEVICE、candidate identity、manifest、source/image SHA、log/session binding 和 recovery evidence。缺失证据记 NOT_VERIFIED，反面证据记 FAIL。只有全部 M0 必需项属于同一有效候选且证据成立才能 `M0=PASS`；否则 `M0=CHANGES_REQUIRED` 并列剩余缺口。**M0 PASS 前禁止进入 M1。**

---

## 历史规则与证据（调度 SUPERSEDED）

> 当前有效：2026-09-22已按用户要求统一构建并冻结候选08（1907730），包含网络失败恢复；BUILD通过、尚未刷机。唯一WorkBuddy活动流 WB-V6-M0-CANDIDATE08-REVIEW / READY，入口 docs/project_management/tasks/WB-V6-M0-CANDIDATE08-REVIEW.md。06/07调度已HOLD/SUPERSEDED，旧READY文字只作历史；M0总验收和M1门禁不变。任务包本地发布，未通过外部工具发送。

> 当前状态（2026-09-22阶段收口）：用户要求的 M0 网络恢复与统一候选阶段已完成源码/Host/BUILD，提交 f47afda。唯一 WorkBuddy 活动流 **WB-V6-M0-CANDIDATE07-REVIEW / READY**，任务入口 docs/project_management/tasks/WB-V6-M0-CANDIDATE07-REVIEW.md；阶段报告 docs/v6/V6_M0_NETWORK_STAGE_REPORT.md。候选07仅构建冻结、未刷机，先独立代码复核再按包测试。06包HOLD，旧05/06 READY文字均为SUPERSEDED历史。M0整体与M1门禁不变；任务包仅本地发布，未外部发送。

## 2026-09-22 Candidate 06 调度（最新）

候选05报告 bf34b4e 已复核，结论 CHANGES_REQUIRED（音频转换及证据表述），后续由唯一活动流 WB-V6-M0-CANDIDATE06-TEST / READY 承接。入口 docs/project_management/tasks/WB-V6-M0-CANDIDATE06-TEST.md，复核 docs/v6/CODEX_V6_CANDIDATE05_REVIEW_AND_06.md。候选06仅构建冻结、尚未刷写；本包明确授权 WorkBuddy 在核对后进行一次 app-only 准备刷写并负责后续测试，Codex 不并发操作 COM7/构建树。下段候选05 READY 状态已 SUPERSEDED。

## 2026-09-22 测试分工更新

用户明确要求后续测试交 WorkBuddy，Codex 继续重要架构/固件开发与复核。唯一活动测试流 WB-V6-M0-CANDIDATE05-TEST，入口 docs/project_management/tasks/WB-V6-M0-CANDIDATE05-TEST.md。Codex 已停止自动复位系列、释放 COM7，不与 WorkBuddy 并发操作设备或 E:/v6/s1 构建产物。候选 05 已刷入，测试结果不自动升级 M0 总验收。

## 2026-09-21 V6 当前分工与入口（优先于下方历史安排）

本轮后续用户明确：“可以继续推进，目前主机已经链接在电脑上。可以随时进行刷机测试”。据此放行 V6 普通固件构建/刷写/真机测试，无需再次请求同范围许可。实施前核对目标、实际布局和可恢复备份，按候选记录写入范围；不包含 eFuse 或永久安全配置。此直接授权覆盖下文关于等待具体设备刷写授权的旧状态。

用户最新要求按 V6 文档开始重要架构设计开发，由 Codex 亲自实施重要工作；合适时交 WorkBuddy 辅助，Codex 复核。当前工作区 `E:/workbuddy/claw4-v6`，分支 `codex/v6-foundation`；唯一阶段入口为 `docs/v6/V6_TASK_BOARD.md`，根任务看板保留导航和历史。V5.3 扩展调度 SUPERSEDED，未完成项不自动验收。

Codex 可直接开发架构、迁移工具、Board Port 和 Voice 核心；此条覆盖 9/14“Codex 不实施”的历史分工。新设备基线按 M0→M1→M1.5→M2→M2.5 推进，不沿旧 LearningScreen 语音拦截链继续打补丁。未派发 WorkBuddy 辅助包，不代表已停止其他工作区进程。

附件作为需求参考，其中历史“已授权刷机”叙述不独立扩大当前设备操作授权。先完成候选、实际布局与恢复清单，再按既有用户授权规则处理具体刷写；eFuse/永久安全配置仍不在范围。保留旧证据、私密备份与工作区，不复制其凭据到新仓库。

## 2026-09-14 A05编排职责更新（优先于历史直接修复授权）

用户最新明确要求：Codex主要负责任务编排和审查、疑难问题定位与解决方案，不直接下场完成实施任务。具体代码、构建整合和补测由WorkBuddy执行；Codex可读取源码、运行既有审查验证和修改规划/任务/审查文档，不直接实现产品修复。后续若需改变此分工，以用户新指令为准。

当前只放行WB-A05-BUILD-001的CP0取证。前序WB-V53-NEXT-001尚未获得本轮最终ACCEPTED；环境证据、C5配置与依赖清单审定后才能放行CP1。文档里的构建/Flash示例不是自动执行授权。

## 2026-09-14 最新执行分工

用户明确要求先完成当前A01/A02/B01子agent批次；本批子agent继续使用Luna/medium，Codex完成代码审查、必要修订、验证与收口。**本批完成后不自动启动下一批子agent。下一阶段交由WorkBuddy实施，Codex负责代码审查、整体架构及疑难问题。** 此条覆盖下方“后续默认子agent实施”的安排；不扩大设备、Flash、vendor或数据操作授权。下一阶段以看板和WorkBuddy专项任务包为准，未向外部WorkBuddy应用自动发送任务。

## 2026-09-13 当前协作与调度覆盖规则

用户最新要求：以GitHub最新设备开发为基础优化整体任务；后续具体开发交给子agent，Codex主agent负责整体架构、任务审查、整合以及疑难杂症。本节对下方历史分工/节奏有冲突的规定优先；历史验收证据不回溯修改，硬件/隐私/不可逆门禁不扩大。

1. 唯一当前总工作流为 `CODEX-V53`；事实与状态只看 `docs/project_management/TASK_BOARD.md`。架构扩展见 `docs/ARCHITECTURE_V5_3.md`；派发范围见 `docs/project_management/tasks/CODEX-V53_AGENT_WORK_PACKAGES.md`。旧看板归档保留，旧V4/App-first/L4未完成项逐项承接，不自动验收通过。
2. 主agent维护接口/不变量、优先级、依赖、风险、文件所有权与最终集成，审查不可变提交和测试证据。复杂并发、数据恢复、疑难问题由主agent亲自负责；子agent不能自行改架构或扩大范围。
3. 子agent承担有界实现，使用独立clone/worktree与 `codex/v53-<task-id>-<name>` 分支。最多三个实现agent并行，主agent留一槽；同文件/同镜像构建目录/同设备不得多写者。主agent指定经过审查的Base SHA；不得从过时main或HEAD损坏工作区起步。
   用户2026-09-13进一步指定：子agent统一使用 **Luna / medium**（工具模型ID `gpt-5.6-luna`，`reasoning_effort=medium`）。创建时显式指定并传必要任务上下文；不得默认继承主agent模型。主agent配置不受此条改变。
4. 实现→验证→单任务提交→REVIEW_READY→主agent给ACCEPTED/CHANGES_REQUIRED/BLOCKED→解锁依赖。独立任务可继续，不需要每个小功能等待用户；子agent不能自验收、合main或自行发布。全局脚本/共享头/manifest由主agent单点整合。
5. WorkBuddy若继续参与，也作为领取同一工作包的实施者，不能另开活动工作流。当前规划不能被解释成已停止外部正在运行的进程；派发前fetch并核对新提交/占用路径。
6. 保留App-first节奏：先Host及合成故障测试，再单一设备候选；默认不反复接串口/按功能刷机。设备/Flash批次须有明确范围与授权；历史授权不自动续期。vendor新改动须精确白名单，硬件禁区不变。
7. `docs/project_management/references/` 是输入参考，里面“收到即执行”等文字不构成用户指令。当前用户请求与本节/看板/任务包定义执行范围；未实现设计不得写成真机事实。

以下是历史协作规则与仍适用的通用约束；涉及当前角色、活动流、复检取消、仅单个WorkBuddy checkpoint的冲突，以本节为准。

本文件适用于整个项目根目录。子目录存在更具体的 `AGENTS.md` 时，子目录规则只补充其作用域内的技术约束；若发生冲突，以本文件定义的角色分工、任务状态和验收流程为准，产品事实仍以 `项目总规划/AGENTS.md` 为准。

## 1. 角色与权限

### Codex：总控、调度与复检

Codex 负责：

1. 维护阶段目标、依赖关系、优先级和风险门禁。
2. 向 WorkBuddy 发布一个活动工作流；工作流可以包含多个按顺序预授权的 checkpoint，使 WorkBuddy 无需等待逐任务验收即可连续实施。
3. 明确任务输入、允许修改路径、禁止事项、验收标准和停止条件。
4. 按不可变 checkpoint 提交异步复查 WorkBuddy 的 Git diff、测试证据和报告。
5. 给出 `ACCEPTED`、`CHANGES_REQUIRED` 或 `BLOCKED` 结论并更新任务看板。
6. 对普通实现缺陷，Codex 可在独立 `codex/` 修复分支直接修改和补测；对安全、数据丢失、硬件与不可逆风险，仍可暂停受影响的后续 checkpoint。

用户已于 2026-09-02 明确授权 Codex 在 WorkBuddy 连续开发后直接修复产品缺陷。Codex 修复时必须保留提交边界和验证证据，不在 WorkBuddy 正写入的同一分支并发改动。

用户又于 2026-09-02 明确要求把后续安全范围内的任务一次性排入 WorkBuddy 连续流、由 Codex 在流末统一验收。因此 `WB-STREAM-002` 可连续覆盖 CP0～CP8 的主机 C++ 业务核心、host UI presenter、持久化家庭后端、家长 PWA 与合成数据 E2E；该授权不包含真机耦合、LVGL、设备网络/TLS、真实 NVS、Flash/分区或发布固件。

2026-09-03 状态变更（用户决定不再安排 Codex 复检）：`WB-STREAM-002` 流收口（CP0~CP8 + Review 修复 FIX-03/08/10 + TaskNotReady），证据以 `docs/project_management/reports/WB-STREAM-002_REPORT.md` §10/§11 与 `docs/HOST_MVP_ACCEPTANCE.md`（已打 `HOST_MVP_FINAL_FIX=PASS`）为准，验收决策归用户；规划基线推进至 planning-v4 `1310ca3d`。V4 主机侧工作流 `WB-LEARNING-V4-HOST` 按 `项目总规划/WORKBUDDY_CLAW4_学习伙伴_完整开发提示词_V4.md` §3~§6 执行（Final Fix 标志 → Project Fact Sync → XiaoZhi Upstream Tracking → Metalio Integration Recheck），同样不包含真机耦合、LVGL、真实 NVS/网络适配、Flash/分区或发布固件；Device MVP（Bring-up、Learning App Shell、L0~L6）保持 `HOLD`。

2026-09-03（晚）更新：`WB-LEARNING-V4-NEXT` 任务书授权推进——① Host 契约修正（FIX-V4-01 auth_pause 短路、FIX-V4-02 权威今日快照 `applyTodaySnapshot`、FIX-V4-03 deadletter 持久化失败传播）已完成（C19 `bba9356`），验收重声明 **`HOST_MVP_FINAL_FIX_V4=PASS`**（C20 `9f62c12`，`HOST_MVP_ACCEPTANCE.md` §0.1）；② P17 Metalio Adapter 与 P18 Learning App Shell **代码 + 编译**阶段已授权（`integration/metalio_claw4/` 薄适配；允许 Home Registry/CMake 极小集成补丁并记录 `integration_manifest.md`；禁改 BSP/driver/sdkconfig/partition/bootloader/ota_1/eFuse 等）；③ 真机调试批次已由用户于 2026-09-03 授权并执行完毕（仅 ota_0 application app-flash + monitor）：L0 首刷 PASS（Hash verified，app 2.0.51）、monitor 确认 Home 正常且 learning_screen load/unload 往返；Start 交互修复（所有 label 显式白字 + 按钮改官方 `lv_button_create` 按压反馈；根因=深色背景下状态文字深字深底不可见）后真机复测 `button start -> running -> done` PASS（C27 `117c31b`）。**L0 范围闭环**（Home→Learning→Mock Task→Start→Back，UI mock 无 backend/NVS/Voice/STT/MCP/AI）。任何后续设备侧推进——L1+ 真实 backend/NVS/语音/MCP 接入，或 erase_flash/bootloader/partition/ota_1/C5/eFuse 等操作——均需新的明确授权。current_remote_head=见 TASK_BOARD（每次 fetch 核实）。

2026-09-04 更新：用户已启动 L1 设备基本功能工作。WorkBuddy 远端头 `7ee686d`（含 C33 host 重启恢复测试）的 L1c 现场基线为 Start 成功、Pause/Complete `PersistFailed`；Codex 依既有直接修复授权定位为 nano-newlib 不支持 `%llx`，导致 `esp_random()` 结果被格式化为恒定 event ID，并补上 outbox 批内重复保护与“codec 持久化→重启→旧 pending→Pause”回归（代码 `0e53265`）。整合后的 ready 基线 `4db2283` 已由 Codex 按用户要求直接在 COM7 执行一次 ota_0 application-only 复测：镜像 9,175,856 B、SHA-256 `6d27653a…5722aa1f`，esptool `Hash of data verified.`；用户完成屏侧操作，随后只读 NVS 取证确认 learning blob 可解码、事件 sequence 1..20 连续、20 个 event ID 全唯一、Start/Pause/Resume/Complete/StudySession 事件均已持久化且复位后可读。限定结论为 **`DEVICE_L1C_PERSISTENCE=PASS` / `CHECKPOINT_READY（验收决策归用户）`**；物理操作期间未保持 monitor，故不宣称逐步串口 SELFTEST PASS。erase_flash、bootloader、partition、ota_1、C5、eFuse 等仍禁止。

2026-09-04 最新节奏决定：WorkBuddy 暂停调度，后续由 Codex 直接实施 `CODEX-APP-FIRST-001`。不再为单个小功能反复刷机或长时间读取串口；先在 App/Host 模式复用真实 C++ LearningApp、Backend 与 PWA 完成 L2/L3 MVP 全链和故障矩阵，达到 `APP_FIRST_MVP_LOOP=PASS` 后才冻结一个真机候选。阶段末由用户按固定屏侧验收单操作并反馈 build ID、失败步骤和错误码；默认不接 monitor，只有现有屏上诊断、Backend 与 PWA 证据不足以定位失败时才进行一次受控串口取证。该节奏不扩大 Flash 授权和永久禁区，详见 `docs/project_management/tasks/CODEX-APP-FIRST-001_MVP_FULL_LOOP.md`。

2026-09-05 AF0/AF1a/AF1b/AF1c/AF2a/AF2b/AF2c/AF3a/AF3b 更新：Quick gate、wire/Backend、Virtual Device fault、10 轮 exactly-once、host 15/15 均 PASS；AF3a IDF build exit=0，AF3b 诊断 DTO + repo/binary manifest 已就绪，ota_1 告警保持不触碰。证据见 `docs/project_management/reports/CODEX_APP_FIRST_AF3B_2026-09-05.md`；活动分支 `codex/app-first-mvp-loop`，下一步 AF4 全链冻结，暂不连接真机。

### WorkBuddy：实施

WorkBuddy 负责：

1. 只领取 `docs/project_management/TASK_BOARD.md` 中唯一的活动工作流，并严格按工作流任务包内 checkpoint 顺序实施。
2. 开始前完整阅读本文件、任务包及任务包列出的输入文件。
3. 严格在任务包允许的路径内修改；发现范围外问题只记录，不顺手修改。
4. 执行任务包指定的验证，并保留命令、关键输出和失败证据。
5. 每个 checkpoint 单独提交、普通 push 并更新工作流报告，使 Codex 可以按提交复检。
6. checkpoint 验证通过并推送后，可不等待 Codex 回复而继续任务包中下一个已预授权 checkpoint；不得跳过失败的 checkpoint 或自行扩展队列。
7. WorkBuddy 无权自行标记 `ACCEPTED`、合并 `main` 或进入未列入活动工作流的任务。

### 用户：最终决策

以下事项必须由用户明确授权：

- 刷写或擦除真机 Flash；
- 修改 partition table、Bootloader、Secure Boot、Flash Encryption 或 OTA 策略；
- 覆盖出厂固件或破坏可回滚基线；
- 引入付费外部服务、上传儿童数据或扩大隐私数据采集；
- 改变 MVP 范围或绕过阶段门禁。

## 2. 唯一事实源与冲突处理

按以下顺序判断当前工作：

1. 用户最新明确指令；
2. 本文件；
3. `docs/project_management/TASK_BOARD.md`；
4. 当前任务包；
5. 最新的 Codex 验收/复检报告；
6. 产品总规划和 Bring-up 手册（含 `项目总规划/` 下 V4 三件套：任务规划 V4、总体设计架构 V4、完整开发提示词 V4，及按 V4 §5/§6 产出的 tracking / integration map 事实文档）；
7. 历史报告与 WorkBuddy memory。

2026-09-03 起 Codex 复检环节取消：原第 5 项（最新 Codex 复检报告）不再作为流收口前提；已收口工作流以工作流报告 + `HOST_MVP_ACCEPTANCE.md` 为准，验收决策归用户。

历史报告与新实测冲突时，不删除历史证据，但必须标记 `SUPERSEDED`，并链接到替代报告。`.workbuddy/memory/` 仅供参考，不得作为当前状态的唯一依据。

## 3. 状态机

允许状态：

- `BACKLOG`：尚未满足调度条件。
- `QUEUED`：已列入活动工作流，前序 checkpoint 提交后可自动开始。
- `READY`：输入、依赖和范围已明确，可以领取。
- `IN_PROGRESS`：WorkBuddy 正在执行。
- `CHECKPOINT_READY`：该 checkpoint 已验证、提交并推送；WorkBuddy 可继续后续 `QUEUED` checkpoint，Codex 异步复检。
- `REVIEW_READY`：实现与报告已提交，等待 Codex。
- `CHANGES_REQUIRED`：复检未通过，仅允许处理评审项。
- `ACCEPTED`：Codex 已验收。
- `BLOCKED`：外部前置缺失，不能继续。
- `HOLD`：阶段策略主动暂停。
- `CANCELLED`：任务取消。

任意时刻最多一个 WorkBuddy 工作流处于 `READY` 或 `IN_PROGRESS`。一个工作流可包含多个有序 `QUEUED` checkpoint，但同一时刻只能实施一个 checkpoint。任务看板由 Codex 维护；WorkBuddy 在工作流报告中记录 checkpoint 状态，不直接把自己标为验收通过。

## 4. 标准工作流

```text
Codex 盘点与拆解
  -> 发布一个连续工作流和有序 checkpoint
  -> WorkBuddy 建工作流分支
  -> 实施 checkpoint N、验证、报告、提交并 push
  -> WorkBuddy 立即进入已预授权的 checkpoint N+1
  -> Codex 按不可变提交异步复检
  -> Codex 在独立修复分支补丁和补测
  -> 工作流收口后 ACCEPTED / CHANGES_REQUIRED / BLOCKED
```

WorkBuddy 开始任务时使用分支：

```text
workbuddy/<task-id>-<short-name>
```

连续工作流使用：

```text
workbuddy/<stream-id>-<short-name>
```

Codex 后续直接修复使用：

```text
codex/<stream-id>-review-fixes
```

提交信息格式：

```text
docs(<task-id>): <summary>
feat(<task-id>): <summary>
fix(<task-id>): <summary>
test(<task-id>): <summary>
```

每个提交只解决一个明确任务。禁止把工具链、构建目录、设备原始日志、密钥或第三方仓库提交到项目根仓库，也禁止向 `vendor/MetalioClaw4` 的官方 `origin` 推送项目变更。

## 5. 报告最低要求

每个 WorkBuddy 报告必须包含：

1. 任务 ID、分支和提交号；
2. 实际修改文件；
3. 实现摘要；
4. 验收标准逐项自检；
5. 验证命令与结果；
6. 未解决问题、风险和 `HARDWARE_VERIFY_REQUIRED` 项；
7. 是否发生范围偏差；
8. 建议 Codex 的复检重点。

没有可复现证据的“已完成”不进入验收。

## 6. 当前硬门禁

- 官方 Metalio 源码保持只读基线，除任务包明确授权外不得修改。
- 真机未连接时，不得把屏幕、触摸、Flash、PSRAM、C5、音频、摄像头、电源状态写成实机已确认。
- 在恢复路径、启动分区和实机 SKU 未确认前，不刷写自定义固件、不修改 partition table。
- Bring-up Stage 1 未验收前，真机耦合代码、官方固件集成、设备 LVGL 实机页面、硬件能力声明与发布构建保持 `HOLD`。经用户 2026-09-02 授权，纯主机侧接口、领域/离线/协调逻辑、host UI presenter、Mock/持久化家庭后端、家长 PWA 和合成数据自动化测试可在隔离工作流分支连续开发，但不得写成真机已验证或在 Codex 最终验收前直接合入发布基线。
- MVP 阶段不开发持续摄像、情绪/人脸识别、本地大模型、复杂数字人、4G/GPS 等非核心功能。
