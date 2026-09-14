# Claw4 项目协作总则

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
