# Claw4 当前任务看板

> 当前有效（2026-09-23）：候选08复检修复已提交 `26f60ec`；候选10完整构建、app-only 刷写/回读及启动和隐藏网络路径验证通过。当前固件清单 `docs/v6/V6_M0_CANDIDATE10_REPORT.md`；串口私密证据留在 `.gitignore` 覆盖的 `out/v6-device-private/`。播放可闻性获用户确认；正常音量余量与参考/AEC仍未验证。M0总验收未通过，M1保持 BACKLOG。用户已要求 Codex 直接负责后续测试与开发；当前没有 WorkBuddy 活动流。

> 当前状态（2026-09-22阶段收口）：用户要求的 M0 网络恢复与统一候选阶段已完成源码/Host/BUILD，提交 f47afda。唯一 WorkBuddy 活动流 **WB-V6-M0-CANDIDATE07-REVIEW / READY**，任务入口 docs/project_management/tasks/WB-V6-M0-CANDIDATE07-REVIEW.md；阶段报告 docs/v6/V6_M0_NETWORK_STAGE_REPORT.md。候选07仅构建冻结、未刷机，先独立代码复核再按包测试。06包HOLD，旧05/06 READY文字均为SUPERSEDED历史。M0整体与M1门禁不变；任务包仅本地发布，未外部发送。

> 2026-09-22 最新覆盖：candidate05 @ bf34b4e 已复核，CHANGES_REQUIRED，修订由 candidate06 承接。唯一 WorkBuddy 活动流 **WB-V6-M0-CANDIDATE06-TEST / READY**；任务包 `docs/project_management/tasks/WB-V6-M0-CANDIDATE06-TEST.md`，复核 `docs/v6/CODEX_V6_CANDIDATE05_REVIEW_AND_06.md`。候选06已构建、56工具测试及C++测试通过，尚未刷机；设备测试由WorkBuddy独占执行。本地任务包已发布，未通过外部消息工具送达。下文05调度状态 SUPERSEDED，M0/M1门禁不变。

## 2026-09-21 V6 覆盖状态（当前有效）

用户要求 Codex 亲自进行重要架构设计开发，WorkBuddy 可承担有界辅助任务并由 Codex 复核。当前活动 `CODEX-V6-FOUNDATION / IN_PROGRESS`；分支 `codex/v6-foundation`，工作区 `E:/workbuddy/claw4-v6`，来源 `5657ebed64ad5962c889fcf90f0979f5891ab862`。

唯一当前队列：[V6_TASK_BOARD](../v6/V6_TASK_BOARD.md)；[架构](../v6/V6_ARCHITECTURE.md)、[基线](../v6/V6_BASELINE.md)、[首批报告](../v6/V6_FOUNDATION_REPORT.md)。V5.3 后续扩展 HOLD；下方旧调度入口 SUPERSEDED，仅留历史，不构成当前 READY 授权。2026-09-22 已发布唯一 WorkBuddy 流 WB-V6-M0-CANDIDATE05-TEST（READY），任务包见 tasks/WB-V6-M0-CANDIDATE05-TEST.md。候选 05 已由 Codex 构建刷入，后续测试交 WorkBuddy；未宣称 M0 总 PASS。

## 2026-09-14 A05方案审查覆盖状态（当前有效）

用户最新要求：WorkBuddy实施和补测；Codex只负责规划、代码/证据审查、疑难定位，不直接代替实施。以下状态覆盖后文首批收口时的历史队列。

| 工作 | 当前状态 | 依据/下一步 |
| --- | --- | --- |
| WB-V53-NEXT-001 | REVIEW_READY，等待验收证据收口 | source `19fd979d4222093ff4ce7464e5b58407586594a2`；WorkBuddy29/29日志，本轮24/29实际运行，5项应用控制阻断；cleanup6 cases通过，非整流ACCEPTED |
| WB-A05-BUILD-001 CP0 | READY（当前唯一可领取checkpoint） | 只做Host证据与C5配置/依赖取证报告 |
| WB-A05-BUILD-001 CP1～CP4 | QUEUED，不得提前开始 | Codex记录前序ACCEPTED并认可CP0清单后放行 |
| A05-DEVICE | HOLD | 唯一候选BUILD审查+用户批次授权 |
| C01/C03及其后 | BACKLOG（本包不执行） | 另行编排；Host任务可不依赖设备连接，设备门禁不变 |

任务入口：[修订版A05任务书](tasks/WB-A05-BUILD-001.md) · [审查依据与实测](reports/CODEX_A05_PLAN_REVIEW_2026-09-14.md)。不能使用E:/c当前H2 sdkconfig生成C5候选；历史C5配置hash及cache来源见报告。未进行本轮IDF构建/Flash。

## 以下为首批收口时的历史快照（调度已由上节覆盖）

更新时间：2026-09-14。维护者：Codex 主 agent。当前唯一可领取工作流：**WB-V53-NEXT-001 / READY**。CODEX-V53-WAVE1（A01/A02/B01，Luna/medium）已完成源码/Host范围验收。

用户最新分工：当前子agent批次已收口；后续由WorkBuddy实施，Codex负责整体架构、代码审查、整合与疑难问题。历史验收不回溯修改。

> **9/14覆盖决定**：仅收口当前三个子agent任务；本批完成后，下一阶段由WorkBuddy实施、Codex审查，不再自动派发下一批子agent。下方角色列是原工作包专业分工，后续统一由WorkBuddy承担实施。

## 1. 基线与入口

- 最新设备开发来源：`origin/workbuddy-app-first-l3-acceptance` @ `7dd6511ab0125962d37f039955298819bdbb77be`（2026-09-13 11:43 +08:00；本轮远端复核一致）。
- 规划分支：`codex/v53-architecture-task-plan`；独立目录 `E:/workbuddy/claw4-v53-control-20260913`。后续agent从主agent给出的已审查SHA建立独立分支，不从旧工作区或main起步。
- 当前整合分支：`codex/v53-foundation-wave1`，从已推送规划提交 `1ae2a0c546615666a57abdf601b343bc88a043c4` 起步；三个实现工作区分别为同级 `claw4-v53-a01`、`claw4-v53-a02`、`claw4-v53-b01`，模型统一 `gpt-5.6-luna / medium`。
- `main` 仍为9/2旧基线，不代表当前设备开发。旧目录HEAD损坏，未修复/覆盖。
- [现状核验](reports/CODEX_V53_FACT_SYNC_2026-09-13.md) · [架构与路线](../ARCHITECTURE_V5_3.md) · [子agent工作包](tasks/CODEX-V53_AGENT_WORK_PACKAGES.md)。
- [旧看板](TASK_BOARD_PRE_V53_20260913.md)保留历史证据，SUPERSEDED仅指调度入口。原V5.3是参考输入，不是自动执行命令。

## 2. 当前事实

| 项目 | 状态 |
| --- | --- |
| L1 PersistFailed | 历史修复+限定设备持久化PASS；不重做排障 |
| L2/L3 | 历史报告6/6链路通过；开发HTTP relay，不等于生产TLS/整机稳定 |
| pause_count | 陈旧构建修正后，9/13报告追加真机验证通过 |
| D2 ResetToSeed | A02源码/Host修复已验收；本批真机验证未执行 |
| D8网络阻塞 | 未修：主循环同步I/O + runtime共享锁跨网络等待，A03优先 |
| 快照覆盖离线终态 | 源码高风险待定向复现，A04 |
| D5 double-free | 最新源码已修，修复候选真机验证未完成，A05 |
| Time/Reminder | B01 TimeAuthority Host已验收；设备校时与Reminder待后续 |
| L4 Voice | 实验功能链通，Voice Gate未过；NAS方案未验证 |
| 本轮Host | 20/20 suites PASS；42/42头、4/4源、1/1契约语法检查PASS；非设备build |

## 3. 调度队列

READY为可下发，QUEUED必须等待列出的依赖与主agent放行；同一共享文件任务串行。首批三个实现包已验收；下一包按WorkBuddy任务书顺序领取。审查包C0不计为产品Gate。

| ID | 任务 | 执行角色 | 状态 | 依赖/门禁 |
| --- | --- | --- | --- | --- |
| C0 | 远端事实、架构、看板、子agent包 | 主agent + 只读审计agents | ACCEPTED（规划范围） | 7dd6511；无硬件声明 |
| A01 | 可复现镜像/补丁/产物证据 | Luna实施+Codex修订 | ACCEPTED（工具/取证） | Base 1ae2a0c；实际镜像只读取证 |
| A02 | 正式任务Reset数据保护 | Luna实施+Codex修订 | ACCEPTED（源码/Host） | Base 1ae2a0c；Host+源码最小修复，无实际擦除 |
| B01 | TimeAuthority Host | Luna实施+Codex修订 | ACCEPTED（Host） | Base 1ae2a0c；独立目录，与A01/A02并行 |
| A03 | 网络I/O与状态所有权 | WorkBuddy + Codex | QUEUED | A02 + 主agent线程ADR审查 |
| A04 | 离线终态与快照合并 | WorkBuddy | QUEUED | A03；共享session/coordinator串行 |
| B02 | InteractionArbiter Host | WorkBuddy | QUEUED | WB CP3；CP2完成且接口冻结 |
| B03 | Reminder Host与恢复 | WorkBuddy | QUEUED | B01/B02接口 + A04合并契约 |
| A05-BUILD | M0候选冻结/已有D5回归 | WorkBuddy + Codex | QUEUED | A01–A04；精确patch与工具链可用 |
| A05-DEVICE | M0同一候选上机 | 主agent协调 | HOLD | A05-BUILD + 候选批次授权 |
| C01 | Backend TodayPlan/时间元数据 | WorkBuddy | QUEUED | A04/B01 schema审查 |
| C02 | PWA计划编辑 | WorkBuddy | QUEUED | C01契约冻结；最终对真实API集成 |
| C03-HOST/BUILD | Device Time/Reminder薄适配 | WorkBuddy | QUEUED | A01–A04/B01–B03/C01；限定文件与vendor边界 |
| C03-DEVICE | 本地提醒/离线/误差验收 | 主agent协调 | HOLD | M0设备Gate + C03构建 + 批次授权 |
| C04 | Focus/DailyReview/习惯闭环 | WorkBuddy | QUEUED | M1/M2；共享文件分提交串行 |
| D01 | 语音服务路线合成验证 | WorkBuddy | QUEUED | B02；先研究/fixture，不自动部署 |
| D02 | Voice/MCP/Level-2联合 | WorkBuddy | QUEUED | 本地提醒Gate + D01；真机部分需候选授权 |
| D03 | 家庭试用安全/全链/长稳 | WorkBuddy | BACKLOG | M1–M4；真实数据/部署另定范围 |
| E01 | ScreenOff/LightSleep/Wake | WorkBuddy | HOLD | M1稳定 + 新板级授权；不阻塞M1–M4 |
| E02 | 个性化/复杂重复规则 | 后续 | BACKLOG | 客观数据可信、核心MVP稳定 |

当前派发入口：[WB-V53-NEXT-001](tasks/WB-V53-NEXT-001.md)。CP0 READY；CP1～CP5 QUEUED，顺序推进，Codex按提交审查；不再派发新子agent。冻结代码SHA `e70c2830d1f7ea43629a61ce01a33f78b6f11128`；完整[首批收口报告](reports/CODEX_V53_WAVE1_CLOSEOUT_2026-09-14.md)。其它队列行仅表示后续路线，不能越过本任务书自动领取。

## 4. 旧任务承接

| 旧项 | 本轮处置 |
| --- | --- |
| WB-LEARNING-V4-HOST / L1c | 已有完成证据保留，旧PersistFailed由修复报告取代 |
| CODEX-APP-FIRST-001/002 | 历史Host/L3证据保留；新调度入口停用，缺口承接A01–A05/C01/C03/D03，不整体补盖ACCEPTED |
| WB-APP-FIRST-L3-ACCEPTANCE | 报告6/6保留；D2/D8及构建缺口承接A01–A05 |
| WB-LEARNING-L4 | 新功能扩张暂停；已有修复源码保留，稳定性与方案承接A05/B02/D01/D02 |
| 原V5.3 | 产品方向采纳；具体顺序/契约由优化架构与工作包覆盖，原文只读参考 |

这是本规划分支的调度约定，不表示已远程停止另一个工具中正在运行的进程。后续开始实现前主agent重新fetch；若源分支推进，先核对新提交与占用路径。

## 5. 硬门禁与验收分层

- 不改partition/bootloader/ota_1/C5/eFuse/SecureBoot/FlashEncryption，不擦NVS，不以清Backend事件表修复D2；srmodels刷写也不是ota_0授权的一部分。
- vendor保持只读；新的修改必须精确任务白名单与适用授权，完整patch登记，禁止推官方origin。
- 每个候选分别记录CODE/ HOST/ BUILD/ DEVICE证据；旧报告PASS不自动适用于新固件。
- 首次实际家庭试用前需D03（身份、TLS、数据保留等）；开发HTTP relay和单家庭stub不能直接发布。
- 子agent只交REVIEW_READY，主agent负责技术审查；用户保有硬件/发布/范围最终决策。
