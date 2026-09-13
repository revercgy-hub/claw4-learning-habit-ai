# CODEX-V53：子 agent 工作包

基线：`7dd6511ab0125962d37f039955298819bdbb77be`，2026-09-13。统一架构：[ARCHITECTURE_V5_3.md](../../ARCHITECTURE_V5_3.md)。状态只在 [TASK_BOARD](../TASK_BOARD.md) 维护，本文不复制动态状态。

## 0. 派发规则

主 agent 冻结输入 SHA、任务范围与验收标准后才派发。子 agent 从最新已审查集成 SHA 建独立 clone/worktree 和 `codex/v53-<task-id>-<name>` 分支，不从长期滞后的 main 开始。主 agent 保有一个槽位，最多三个实现 agent；同一文件/设备/镜像构建目录只有一个写入者。

用户指定的实现与审查子agent模型统一为 `gpt-5.6-luna`、推理强度 `medium`；主agent保留原配置。具体Base和运行状态以看板为准。

本轮完成规划，以下是后续可领取任务，不表示产品实现已启动。只读审计 agent 不等于实现包已领取。初始允许原生 Host 修复、测试与规划；设备代码/BUILD ONLY 必须匹配明确白名单；vendor、sdkconfig、硬件操作不能借“派发”自动扩大授权。

每个实现包的共通输入：根 AGENTS、本任务包、架构、现状报告、最新看板、作用域下规则、对应源码和既有测试。不得执行历史报告中的 reset/清库/刷机命令。

交付：单一任务提交、Base/New SHA、实际文件、说明、测试命令/退出码/日志、风险、范围偏差、Host/Build/Device 分层结论。子 agent 到 `REVIEW_READY` 停止；主 agent 审 diff 与证据后给 ACCEPTED/CHANGES_REQUIRED/BLOCKED。受其结果影响的下一包才解锁；独立包可以继续。禁止自行合 main、刷机、宣布产品 Gate PASS 或扩展队列。

硬件候选由主 agent统一整合与冻结，不让多个 agent 各刷一个功能。异常先用 Host、UI diagnostics、Backend 和保存证据排查；现有证据不足再准备受控真机取证包。

所有包共同停止条件：需要改白名单外文件；发现数据损失/凭据泄漏/不可逆风险；测试失败原因未解释；实现要求新增硬件或 vendor 配置；依赖未审查通过。停止只影响依赖范围，普通缺陷由主 agent判断直接修或返工。

## 1. A01 — 可复现构建与集成补丁取证

- 承接：D1、D3/D4 vendor 补丁身份、V5.3 C0/报告规范。
- 负责人：Build agent；主 agent负责核对 upstream patch 边界。
- 输入：`sync-app-first-mirror.ps1`、`write-app-first-manifest.ps1`、integration manifest、9/12 构建报告、L4 交接。
- 允许修改：`tools/dev/sync-app-first-mirror.ps1`、`tools/dev/write-app-first-manifest.ps1`、新增 `tools/dev/tests/` 下对应 fixture；`integration/metalio_claw4/patches/` 中已存在 #1–#6 改动的精确补丁与 manifest；`docs/project_management/reports/V53_A01*.md`。
- 范围：先只读比对授权的本地镜像文件与固定 upstream；不将秘密配置或整个 vendor 导入。记录缺失补丁/偏差，不凭文字重造“已实测”补丁。用临时最小 fixture 修复旧 mtime 与清单问题；实际 `E:/c` 保持只读。
- 验收：相同内容不反复重编；不同内容即使源时间较旧仍使构建依赖失效；新增/移除文件可检测且只对受控映射生效；未知文件不删除；CMake 注册与源清单一致；生成 SHA manifest；不能只靠 grep 字符串通过。实际干净 IDF build 留 A05。
- 禁止：修改 BSP/driver/sdkconfig/partition；整文件 checkout 恢复他人补丁；在运行中的共享构建树试修。

## 2. A02 — 正式任务不能触发破坏性 Demo Reset

- 承接：D2；原 V5.3 P0-A 改为已修复 ID 回归 + 新数据保护。
- 负责人：Data agent；数据恢复方案由主 agent负责。
- 允许修改：`integration/metalio_claw4/device/app/learning_runtime.{h,cpp}`、`device/learning_screen/learning_screen.cc` 的 reset 入口；新增 `device/core/` 纯 reset 策略；对应 `firmware/tests/unit/metalio/` 测试；专属报告。
- 设备文件本包只做源码最小修复与 Host 可验证策略，不运行设备、不操作实际 NVS；不得修改其它学习屏/语音逻辑。
- 行为：全部任务完成后显示完成态/等待新计划，不清库、不回 demo。已 provisioned 模式禁用演示重置；后台身份和 outbox 不变。若保留开发模式 reset，先明确隔离条件，不能清除正式 pending。
- 验收：历史 ACK>0、pending>0、在线/离线完成、重复点击、切页、重启、存储失败均不删除事件/重置 sequence；原 ID formatter 与 codec 重启测试仍通过；正式任务不会被 DemoTodaySnapshot 替换。
- 不在本包：自动修复既有损坏队列、修改后端 sequence 规则、删除数据库记录、实际 NVS 擦除。

## 3. A03 — 网络实时性与 Runtime 状态所有权

- 承接：D8，同时为 Reminder/Voice 的实时基础。
- 前置：A02 合入；主 agent先审短 ADR（网络 API 线程限制、状态单写者、代号、锁顺序、取消策略）。
- 负责人：Concurrency agent；主 agent亲自处理线程模型和疑难问题。
- 允许修改：`device/app/learning_runtime.{h,cpp}`、`device/ports/metalio_http_transport.{h,cpp}`、`firmware/main/sync/{http_transport,scheduled_http_transport,learning_backend_session}.{h,cpp}` 中实际存在的文件；对应单元/fake transport 测试。若需要 coordinator 修改，先由主 agent调整白名单并与 A04 串行。
- ADR若要求UI读取不可变快照，主agent在派发前将 `device/learning_screen/learning_screen.cc` 中对应读取/刷新入口加入精确白名单；与A02/A05页面修改串行，不在未扩白名单时顺手改页。
- 必须同时解决：Application 主循环同步 HTTP；持 runtime/UI 锁等待整个网络周期。禁止仅改变线程名或减少超时后宣布解决。
- 设计：请求快照与状态提交分离；单飞、有界排队；Connect/Read 超时；旧 generation 回应拒绝；取消后释放资源；请求期间允许 UI 读取状态与本地命令提交。
- 验收：fake I/O 阻塞20秒时 UI/命令处理仍推进（Host barrier/latch确定性测试，不靠 sleep碰运气）；迟到结果不写新会话；重配置、断网、401、ACK 丢失、并发新增 pending 不丢数据；设备指标留 A05，目标触控反馈 P95<100ms并记录最大停顿。
- 禁止：绕过鉴权/证书、无限队列、用 recursive_mutex 掩盖跨 I/O 锁、未经证据跨线程调用 upstream API。

## 4. A04 — 离线终态与 TodayPlan 合并契约

- 承接：新发现的源码风险；V5.3 P1d 的必要前置。
- 前置：主 agent冻结合并契约；与 A03 对共享文件串行。
- 负责人：Sync agent。
- 允许修改：`firmware/main/application/coordinator.{h,cpp}`、`firmware/main/sync/learning_backend_session.{h,cpp}`、必要 `wire_codec.{h,cpp}` 与对应 tests；Backend/PWA 字段扩展留 C01/C02。
- 先写定向失败用例：离线 Complete 后无 active session，旧 Ready 快照先返回，随后上传失败/成功；若不能复现，提交反证并缩小修复，不凭推测改域。
- 验收：待 ACK Completed/Skipped 不复活；active 改期/删除不丢会话；空权威快照与请求失败不同；旧响应拒绝；401/重启/上传丢响应保持幂等。收敛后设备与后端状态一致；仅交换 GET/POST 顺序不是完整验收。

## 5. B01 — TimeAuthority Host 契约与实现

- 前置：本架构时间契约；可与 A01/A02 并行，无共享写入。
- 负责人：Time agent。
- 允许修改：新增 `firmware/main/time/`、现有 `ports/clock_port.h` 的兼容扩展、新 fake clock 与 `firmware/tests/unit/time/`；报告。现有全局 gate 脚本由主 agent单点登记，agent提供新增测试命令。
- 实现：UTC/单调时钟分离、boot_id、同步年龄与质量、前跳/回拨通知；明确 epoch 无效状态。初始阈值可配置，记录其待实测性质。
- 验收：Unsynced、同步、超时 Stale、过长断网降级、重启单调值重置、前跳/回拨、时区日期边界；不将 uptime 当 Unix epoch，不通过未知停电时长补算专注。
- 不在本包：SNTP、IDF、时钟配置、实际设备校时、后端历史数据改写。

## 6. B02 — InteractionArbiter Host

- 前置：主 agent冻结与 Reminder 的窄输入/输出 DTO；可与 B01 并行，但初始最多三个实现 agent。
- 负责人：Interaction agent。
- 允许修改：新增 `firmware/main/interaction/interaction_arbiter.{h,cpp}`、`ports/voice_session_port.h` 必要兼容扩展、专属 fake/tests。不改学习屏或官方 AudioService。
- 验收：普通 TTS 占用时本地提醒在3秒仲裁预算内获准；两个音源不同时播放；系统 Critical 优先并可恢复；关闭会话/切页后的迟到播报/STT被拒；主动AI不能自行 Snooze/Dismiss；明确用户指令可经当前child/task/reminder scope授权执行，ACK仅确认提醒、不等于Complete；完成请求仍需绑定当前任务的物理确认。

## 7. B03 — Reminder Host Core + 持久化恢复

- 前置：B01/B02接口审查通过；A04的任务事件边界明确。
- 负责人：Reminder agent。
- 允许修改：新增 `firmware/main/reminder/`、新增 `ports/reminder_wake_port.h` 的平台无关接口、专属 fakes 和 `firmware/tests/unit/reminder/`；不引用 esp_sleep/LVGL。
- 验收：TaskDue、Snooze、FocusEnd、DailyReview；稳定实例 ID；rebuild不重置 Snooze；完成/改期/删除取消；前跳/回拨、跨日、多提醒、静默期、上限、grace、保存失败、呈现前后模拟掉电；不对物理响铃承诺严格恰好一次；存储损坏不无限重响。
- 容量：明确最大实例数、编码大小和写入频率；不能占满关键 Learning outbox 后阻止 Complete；持久化与习惯事件采用一致性恢复策略。

## 8. A05 — M0 单一设备候选与稳定性回归

- 前置：A01–A04、已有 D5 修复及相关 Host Gate 通过。
- 负责人：Integration agent准备；主 agent审查/冻结。
- 允许修改：integration manifest、构建清单与专属报告；设备候选在独立外部构建目录生成，不提交固件/工具链/原始日志。
- BUILD ONLY：固定 upstream与完整已有补丁重建；记录源/配置/partition/bin/ELF hash、分区余量；D3让步实际tick长度与D4队列线程边界复查；D5重开/超时/退出/回绕与旧STT场景有证据。
- 设备部分 `HOLD`：拿到具体候选及批次授权后，单次 ota_0 应用候选验证；不把历史批次授权自动续用。拒绝 srmodels/ota_1/partition/bootloader/C5 改动。
- 验收：断网请求超时仍可触控、切页；20轮开关语音窗口无watchdog/堆损坏/重启；正式任务重置保护与离线终态不回退。若语音整体仍不可靠，保留Voice gate未通过，不伪造PASS；如需关闭实验入口，由主agent另派明确页面范围的最小修复包，再重建，A05不越权改页面。

## 9. C01 — Backend TodayPlan 与时间数据

- 前置：A04、B01 契约；主 agent审 API schema 后才编码。
- 负责人：Backend agent。
- 允许修改：`backend/app/`、`backend/tests/`、新增受版本控制的迁移目录、专属报告；不能连接/迁移实际家庭数据库。
- 扩展已有字段：保留 `estimated_minutes`、`priority`、`scheduled_date`、`version`；新增 planned_start、reminder_enabled、pre_reminder_minutes、snooze_options；TodayPlan envelope明确child/local_date/timezone/revision/schema。
- 验收：旧数据库/旧客户端兼容、字段单位/范围、家庭时区、改期/撤销版本、事务快照一致性；未知设备时间不计为1970真实学习日、不把接收时间伪装发生时间；越权输入拒绝；使用合成数据。
- 家长生产鉴权留 D03，当前测试 stub 限制必须明确。

## 10. C02 — PWA 日程配置

- 前置：C01 API schema冻结，可用契约mock先行；最终必须对C01集成。
- 负责人：PWA agent。
- 允许修改：`frontend/src/`、必要测试与专属报告；依赖升级或换框架不在范围。
- 验收：创建/改期/关闭提醒、家庭时区展示、旧任务可编辑、保存失败不假成功、设备未同步/时间未知状态可区分；typecheck/vitest/build与实际Backend契约。

## 11. C03 — Device 时间/提醒/日程薄适配

- 前置分层：C03-HOST依赖A01–A04/B01–B03/C01的软件契约与验证，不等待A05-DEVICE；设备薄适配BUILD依赖C03-HOST；C03-DEVICE另依赖M0设备Gate和候选授权。
- 负责人：Integration/Device agent；先 C03-HOST合成全链，再 C03a 时间、C03b 本地声音/Overlay、C03c计划同步顺序提交。
- C03-HOST允许修改：`firmware/tests/host/{virtual_device_app,connected_learning_app,backend_wire_fixture}.cpp`、`integration/metalio_claw4/host_glue/` 的窄接线、`backend/tests/e2e/` 专属合成fixture、新 `tools/dev/verify-v53-reminder-loop.*`；共享Gate注册由主agent单点整合。使用隔离临时数据库与loopback端口，不碰实际家庭后端。
- C03-HOST验收：合成家长计划经真实wire/backend进LearningApp/Reminder；到期、Snooze、离线Complete、重启、旧快照、ACK丢失重传、改期/取消均验证最终状态与幂等；记录命令/退出码，不能只有mock单元测试。C02完成后补PWA端到端创建/改期。
- 允许修改：`integration/metalio_claw4/device/` 中新增 Time/Reminder adapters、runtime接线、learning_screen必要入口；`firmware/main/sync/wire_codec.*` 增量接线；对应测试；manifest。共享源码变化由主 agent串行合入。
- SNTP：固定IDF网络ready/重连机制，唯一实例、清理/退避/周期校时；不假定创建第二个客户端必要；启动时间失信与TLS行为兼容。
- 音频：复用官方 PlaySound；异步请求经仲裁进入UI；不销毁普通页面状态；本地铃声不得等待网络。
- 持久化：旧blob加载与迁移失败回退，静默期/多提醒/重启恢复；不得擦NVS。
- vendor接入点若超出现有白名单，先由主 agent列出精确文件/补丁并落实授权，不能直接改官方树。BUILD ONLY与DEVICE_TEST分报告。
- 本地设备验收：20轮前台提醒无漏响/重复/卡死；Wi-Fi/NAS/server关闭仍Start/Snooze/保存；时间前跳/回拨不补响全部历史；校时后连续离线4h≤60秒，联网≤30秒。本包不做Light Sleep。

## 12. C04 — Habit Loop 与中断记录

- 前置：M1/M2；负责人：Habit agent。
- 允许修改：Reminder/Domain窄事件扩展、Backend统计与PWA复盘，拆为各自无交叉子提交；共享文件按看板锁定。
- 验收：FocusEnd仅提示、Continue/物理Complete、DailyReview稳定幂等；按时开始率、平均延迟、Snooze数、完成率、连续天数有分母/时区/未知时间处理；重启未保存段标记中断，不算停电时长。
- 周期checkpoint需要先评估NVS写入频率与空间；不能默认每tick写Flash。

## 13. D01 — 语音服务路线验证

- 前置：B02；与本地提醒主线独立，不阻塞M1。负责人：Voice/Server agent。
- 允许：固定版本的上游源码只读调查、`docs/`方案记录、隔离测试fixture；服务端定制在独立目录/仓库方案中明确后再派发，不把第三方整仓提交本仓。
- 验收：合成音频测试命令模式只识别不调LLM/TTS；自由对话与命令模式切换；半双工不回采循环；session取消/断网不重放命令；资源/延迟实测，provider数据去向明确。
- 不默认部署NAS、不调用付费服务、不上传儿童语音、不把`nointent`当ASR-only。

## 14. D02 — Voice Gate、Device MCP 与 Level-2

- 前置：本地Reminder Gate、B02、D01验证；负责人：Voice/MCP agent，按语音稳定→设备工具→播报分checkpoint。
- 允许：现有`firmware/main/mcp/`与`interaction/`窄扩展、设备薄适配、专属测试/报告；vendor变更单独白名单。
- 验收：20轮Wake/ASR/LLM/TTS/Idle无循环/watchdog/持续heap下降；reminder查询/Snooze/ACK工具鉴权、参数和scope正确；主动AI操作拒绝，仅明确用户指令可授权Snooze/ACK，ACK不等Complete；跨child、旧实例、伪造来源、过期session测试拒绝；AI不能直接Complete；Level-2超时不影响本地响铃；联合语音占用场景不漏提醒。

## 15. D03 — 真实家庭试用与最终候选 Gate

- 前置：M1–M4；负责人：Security/Integration agent，主 agent最终审查。
- 范围：开发身份stub→家庭/孩子/设备隔离、HTTPS/WSS证书与hostname、时间启动、凭据生命周期、保留/删除/恢复、构建证据链、完整合成E2E及长稳。
- 初始只允许形成差距测试/方案；实际身份迁移/部署/真实数据操作另行界定，不默认为本任务书授权。
- 验收：最小权限与越权失败、网络故障不损坏数据、家长计划→设备提醒→完成→离线重启→补传→复盘唯一可见；24h运行与资源趋势；用户按统一候选验收单反馈。技术通过不等于用户已批准发布。

## 16. E01/E02 — 后置范围

- E01：Screen Off → Light Sleep → Wake，全流程恢复LVGL/背光/声音/Overlay；依赖M1稳定且新板级配置与设备授权。先做方案与功耗测量设计，设备实现保持HOLD。
- E02：个性化/重复规则/学校日规则；依赖客观行为数据可信与MVP主线稳定，BACKLOG。不得扩大儿童数据采集来完成个性化。

## 17. V5.3 checkpoint 承接表

| 原文 | 新工作包 |
| --- | --- |
| C0 / §38–40 | 本轮Fact Sync、根规则、看板；A01可复现构建 |
| P0A-a/b/c/d | 旧问题保留回归；A02新数据保护、A03并发、A04一致性、A05联合候选 |
| P0B-a/b/c/d/e/f | B01 Time Host + C03a SNTP + C03联合误差；E01单列睡眠漂移 |
| P0C-a/b | B03 |
| P0C-c/d/e/f | B02仲裁 + C03b本地提示/离线 |
| P0C-g/h/i | B03端口 + E01独立电源阶段 |
| P0C-j | D02，依赖本地/Voice/server三方Gate |
| P0D-a/b/c/d | A05验证已有生命周期修复；D01/D02语音路线与稳定；AEC仅定向实验 |
| P1a/b/c/d | A04 + C01/C02/C03c，复用已有家长/网络链 |
| P2 / P6 | C04 |
| P3 | D01，不默认部署 |
| P4 / P5 | D02 |
| P7 | E02 |

## 18. 主 agent 每次审查清单

确认基线/依赖/独占路径；检视关键不变量（pending、sequence、终态、时间来源、线程、音频、物理确认）；验证失败路径而非只走happy path；运行匹配变更的既有Gate；检查新模块是否进入Host与镜像清单；分开源码/构建/真机结论；审scope和二进制容量；再整合单个提交并解锁下一包。

接口/共享头/测试汇总脚本/manifest由主 agent单点整合。疑难排障采用最小复现→证据→假设→实验→回归；不因旧报告“已排除”跳过当前源码真实失败出口。
