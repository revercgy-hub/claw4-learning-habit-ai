# V6 唯一阶段看板

> **SUPERSEDED 历史快照（2026-09-23）**：Candidate19 已构建但未刷机；Candidate17 DEVICE 观察仅属于 Candidate17。Candidate19 没有 DEVICE PASS。旧工具测试统计按发生时间分别为 C19 构建时 68 项、接管基线 83 项（增加 15 项）；均为 Host 工具测试，不是设备验收。AEC NOT PASS；旧 `clipped=0` 计数无效。

## 2026-10-20 L-17 NAS TTS repair follow-up（当前状态）

L-17 source `812b12905cda01843f5aeb2d80835abb9bc2b084` 已由独立 R-11 复核 `PASS` 并在 NAS 部署。Root 基于同一 digest-pinned XiaoZhi v0.9.6 base 构建本地 EdgeTTS 7.3.1 image；依赖差异仅为 `edge-tts` 从 7.2.6 到 7.3.1。image ID `sha256:acf7314b488ad7e49232797fb42711faad637c00d29142ec782124a92b1d9291` 运行正常；部署后 production-provider service probe 返回 11,664 bytes、可解码 1,944 ms 音频。除 image/build 外，Compose/service config 与 NAS-local provider config 身份保持不变。原失败窗口保留历史；当前只判 `NAS_TTS_SERVICE_REPAIR=PASS`，不推断设备已播放。隔离 probe JSON 的音频 SHA 字段长度异常，未作为有效音频 hash；详见 L-18 报告。Root 未重刷 S-17 app/model，`preflight-02` 已结束：600.014 秒按上限正常停止，用户报告有响应但慢且答错，3 次 NAS interrupt、0 个新 HTTP 403；文本与状态 marker 不证明音频播放。设备状态为 `PREFLIGHT_CHANGES_REQUIRED_SLOW_INCORRECT_RESPONSE`，completed rounds `0/2–3`；正式 20 轮未开始，M1 未验收。Ollama native probe exit 1/result null、错误详情未保留；先前 production-provider 请求 90 秒总超时 exit 124，仅表示请求未完成，不能推断首字延迟。NAS 相同 image 仍运行且 restart count 0。M0 仍 `CHANGES_REQUIRED`，AP outage/recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。详见 [L-18 NAS TTS repair evidence report](V6_M1_NAS_TTS_REPAIR_REPORT.md) 与 [S-17 device report](V6_M1_WAKE_MODEL_REPAIR_DEVICE_REPORT.md)。

## 2026-09-24 接管与用户指令状态（当前有效）

> **SUPERSEDED as current NAS TTS status by the 2026-10-18 L-18 report:** The S-17 first-attempt Wake observation and NAS 403 warning below remain historical evidence. Current NAS service repair and pending Preflight status are recorded above; S15 results do not transfer to S17, and Candidate20 M0/AP recovery state is unchanged.

> 2026-09-27 更新：S-08 同一 Candidate 的用户辅助 Wake 尝试已连续采集并记录于 [Preflight attempt report](V6_M1_WAKE_PREFLIGHT_ATTEMPT_REPORT.md)。结论 `PREFLIGHT_BLOCKED_WAKE_NOT_OBSERVED / ROOT_CAUSE_UNRESOLVED`；不能概括为通用 WakeNet FAIL。Preflight 0/2–3，正式 20 轮未开始。

> 此前被动检查摘要仍由 [S-08 device report](V6_M1_WAKE_DIAGNOSTIC_DEVICE_REPORT.md) 保留；本次用户辅助尝试的状态与计数以 [Preflight attempt report](V6_M1_WAKE_PREFLIGHT_ATTEMPT_REPORT.md) 为准。

> **SUPERSEDED 设备状态：** 2026-09-27 S11 初始被动捕获及后续用户辅助片段仍按 [S11 AFE diagnostic device report](V6_M1_AFE_DIAGNOSTIC_DEVICE_REPORT.md) 保留；其 Candidate 与结果不转移至 S15。当前 S15 证据及 2026-10-18 只读镜像核验见 [S15 WakeNet Control 设备报告](V6_M1_WAKENET_CONTROL_DEVICE_REPORT.md)。

接管基线 `760b2aa66819f8d90186b43af4c02d9b33c8849e` 已保护至 `origin/takeover-v6-m0-c19-baseline`。Candidate20（`claw4-learning-v6-m0.20`）已构建并仅对 live `ota_0` 做 app-only 刷写；读回与 app hash 一致。候选构建布局仍与 live 分区布局不同，未执行恢复写回。A-02 结论为 `M0=CHANGES_REQUIRED`。

2026-09-24 时用户要求跳过 AP outage/recovery 刺激并直接开始下一阶段开发，随后授权在铭凡 N5 x86 NAS 部署小智服务；当时 M1 真机语音轮次尚未开始。该历史指令不改变 M0 结论、不把 AP failure/recovery 标成通过，也不代表 M1 20 轮验收已完成。后续 S11 app-only 写入及其范围见 [S11 设备报告](V6_M1_AFE_DIAGNOSTIC_DEVICE_REPORT.md)。NAS 服务已部署并通过合成连通性验证。

Candidate20 的 CODE/HOST/BUILD/DEVICE 证据分开记账；Candidate17/18/19 DEVICE 结果不得转移；旧 `clipped` 计数不用于验收。

第一实施波 L-01、L-02、S-01、S-02 均已提交并整合至 `7263f67`；L-02 定向复核修复整合至 `a128341`。独立 R-01 在 `a164f11ab5f5843d6ace5c4b990d86a29195a2fe` 复审通过，Host 结果 96/0。

| 队列 | 状态 | 说明 |
| --- | --- | --- |
| L-01 / L-02 / S-01 / S-02 | ACCEPTED | CODE/HOST 经 R-01 通过；DEVICE 证据仍按 Candidate20 单独闭合 |
| R-01 | PASS | 独立复审 source `a164f11ab5f5843d6ace5c4b990d86a29195a2fe`；Host 96/0 |
| S-03 | COMPLETE_WITH_GAPS | app-only readback PASS；设备矩阵部分观察；AP recovery 为 `SKIPPED_BY_USER / NOT_VERIFIED`；详见 [S-03 报告](V6_M0_CANDIDATE20_DEVICE_REPORT.md) |
| A-02 | CHANGES_REQUIRED | Camera 抓帧失败，AEC/唤醒、网络恢复、冷启动、稳定性、故障注入和 recovery evidence 未闭合；布局仍不匹配；见 [Gate 报告](V6_M0_CANDIDATE20_GATE.md) |
| M1-DEV-01 | ACCEPTED | Sol 定向收口；base `db97ad5994f3ee0c5e1fb4deb2fa4fa2ec3ee1ee`；提交 `75d0059a1f0ac6380c1e1e3ee5f41125a44139f2`；定向 13/13、V6 tools 109/109；仅证据工具，不代表 M1 语音验收 |
| M1-NAS-DEPLOY | SERVICE_DEPLOYED | 固定 v0.9.6 镜像、SenseVoiceSmall、本地 Ollama 和 EdgeTTS；OTA/WS/合成 LLM/TTS 连通性通过；[部署报告](V6_M1_NAS_DEPLOYMENT_REPORT.md)；DEVICE 20 轮 NOT_VERIFIED |
| L-03 | COMPLETE | Luna：根看板已同步；集成于 `bde85a5`，不改变架构或历史证据 |
| L-04 | COMPLETE | Luna：M1 Preflight/20 轮模板、日志解析与运行清单已集成至 `ee1a83b`；不改 Voice 架构或既有证据契约 |
| S-04 | COMPLETE | Sol：两处 NVS 初始化已失效关闭；新 Candidate `claw4-learning-v6-m1-preflight-s04-r02-20260925-02` 的 BUILD/Host/身份已冻结至 `f5343c2`；旧 app/ELF `SUPERSEDED` |
| R-02 | PASS | 独立 Sol reviewer：新 stage/镜像/锁哈希匹配，构建来源与集成代码 21/21 Git blob 相等；Host 120/120，设备仍 `NOT_VERIFIED`；仅放行 S-05 的 2～3 轮 Preflight |
| S-05 | BLOCKED_ENDPOINT_MISMATCH | 设备身份/live 布局与 app-only 写入及独立读回通过；启动后实际访问外部 `api.tenclass.net` HTTPS/MQTT，而非 NAS 7443/7444，语音轮次 0；用户已断电、硬件 owner 已释放；见 [设备报告](V6_M1_VOICE_PREFLIGHT_DEVICE_REPORT.md)。修正端点后须新 Candidate/复审/重核身份；AP outage/recovery 不重派 |
| S-04-ENDPOINT | COMPLETE | 原 Sol owner：M1 staged source 固定 NAS OTA/WS 端点，忽略旧 NVS OTA URL、拒绝外部 MQTT/WS 与自动升级；新 Candidate `claw4-learning-v6-m1-preflight-endpoint-s04-r03-20260925-03` 已冻结，旧 Candidate 作废 |
| R-03 | PASS | 独立 Sol reviewer：Host 123/123、22/22 源码映射、13/13 staged 输入、7/7 产物及锁/缓存哈希匹配；仅放行新 Candidate app-only 启动/端点核验，现场只访问 NAS 7443/7444 后才可做 2～3 轮 Voice Preflight |
| S-05-RETRY | BLOCKED_WS_ENDPOINT_NOT_VERIFIED | 新 Candidate app-only/独立读回 PASS，NAS OTA 7443 现场连接；两段 Wake 探针捕获之间有 53 秒空档，用户语句无法绑定，WS 7444 和 Wake 均不得判 PASS/FAIL，完整 Voice 0 轮；用户已断电。见 [重试报告](V6_M1_VOICE_PREFLIGHT_RETRY_DEVICE_REPORT.md) |
| S-05-WS-VERIFY | BLOCKED_WS_ENDPOINT_NOT_VERIFIED | 同一 Candidate 未重建/重刷；连续 326 秒捕获覆盖用户一次唤醒词尝试及回复，但未见 Wake/WS/外连/崩溃标记；不能判一般性 Wake FAIL，WS 7444 仍 NOT_VERIFIED，完整 Voice 0 轮；见 [连续捕获报告](V6_M1_VOICE_PREFLIGHT_WS_VERIFY_REPORT.md)。用户已确认设备断电，COM7 已释放 |
| S-06-WAKE-DIAG | DIAG_COMPLETE / ROOT_CAUSE_UNRESOLVED | 只读检查 324 条参考队列状态均为 idle/零计数，M1 未编译输入 RMS/读取失败诊断；未读出语音内容。不能定位该次未唤醒原因；详见 [诊断报告](V6_M1_WAKE_DIAGNOSTIC_REPORT.md)。设备保持断电，未操作硬件 |
| S-08-WAKE-DIAG-CANDIDATE-EVIDENCE | DEVICE_DIAGNOSTIC_COMPLETE / WAKE_AND_WS_NOT_VERIFIED | R-05 `PASS` 于 manifest revision `36d6443`；merged source `70d463938526ba768f889ebd548f8afd8a03f3af`。同一 S-08 Candidate app-only write/readback PASS，启动于 `ota_0`，观察到 NAS OTA HTTP `192.168.3.100:7443`，未观察外部 Tenclass；WS 7444 与 M1 Wake marker 未观察。48 个输入诊断窗口、771840 mic samples、RMS max 536、peak max 6080、读失败 0、crash markers 0；heap/PSRAM unknown。无 speech，Preflight 0 轮，正式 20 轮未开始。详见[设备报告](V6_M1_WAKE_DIAGNOSTIC_DEVICE_REPORT.md)；不得继承旧 Candidate Voice 结果 |
| S-08-PREFLIGHT-ATTEMPT | PREFLIGHT_BLOCKED_WAKE_NOT_OBSERVED / ROOT_CAUSE_UNRESOLVED | 同一 S-08 Candidate；只读 app 身份核验成功后，进行一次 201.328 秒连续 UART 捕获。188 个输入窗口 / 3,023,200 mic samples，RMS max 2,507、peak max 21,979、I2S read failures 0；观察到 NAS OTA 7443 一次，未观察到 Wake、WS 7444、外部端点或 crash marker。两次用户报告均无法绑定到准确声学帧；第一轮问题语句 `NOT_VERIFIED`。Preflight 0/2–3，正式 20 轮未开始。见 [尝试报告](V6_M1_WAKE_PREFLIGHT_ATTEMPT_REPORT.md)；根因未确定 |
| L-08-M1-CONTINUOUS-CAPTURE | HOST_COMPLETE / DEVICE_CAPTURE_USED | Luna high；`c4600aa` + 定向修复 `d5326fa`。有界连续 UART 采集、时间/字节索引和无载荷状态文件；定向 9/9、V6 Host 138/138。首次 COM7 冒烟检查因串口未枚举而未启动；之后同一工具成功完成 201 秒连续采集并用于 S-08 Preflight 取证。此采集仍不代表 Voice PASS |
| L-09-S08-PREFLIGHT-EVIDENCE | DOCS_COMPLETE | 同一 S-08 Candidate 的用户辅助尝试报告与两级看板更新；捕获/用户报告的证据边界、私有哈希均已记录。仅文档整理，不作根因判定或 WakeNet FAIL 推断 |
| L-11-M1-AFE-DEVICE-EVIDENCE | DOCS_COMPLETE | S11 Candidate 的 app-only/readback、分区不变和初始被动 AFE 输入诊断已记录；用户讲话未确认，不判 WakeNet FAIL/PASS、不计 Preflight。后续捕获由 L-12 同步。见 [设备报告](V6_M1_AFE_DIAGNOSTIC_DEVICE_REPORT.md) |
| L-12-M1-VOICE-OBSERVATION-SYNC | DOCS_COMPLETE | 同步 S11 后续用户辅助捕获：`voice-probe-02` 与 `post-restart-01` 各观察到 Wake/WS 与状态活动，其他捕获有用户报告无响应；不精确绑定话语与日志、不判一般性 WakeNet PASS/FAIL、不计完整 Preflight。S14 opt-in 诊断补丁 R-08 `PASS`（Host diagnostics only），未构建、未设备验证，不构成 S14 Wake PASS；见 [设备报告](V6_M1_AFE_DIAGNOSTIC_DEVICE_REPORT.md) |
| L-14-S15-EVIDENCE-RESUME | DOCS_COMPLETE | 补录 S15 Candidate 历史 app-only 写入/readback 与分区不变摘要、R-09 构建复核状态、9/27 诊断及 10/18 app 身份核验/被动启动摘要；另记录运行加载模型与候选目标模型不一致。`voice-rearm-02` 串口异常中断，用户确认一次 Wake 尝试但未观察到即时响应；捕获不构成完整 Preflight 或 WakeNet 通用 FAIL/PASS。模型修复授权待定。见 [S15 设备报告](V6_M1_WAKENET_CONTROL_DEVICE_REPORT.md) |
| S-15-M1-WAKENET-CONTROL-CANDIDATE | R-09 BUILD_REVIEW_PASS / MODEL_MISMATCH_CONFIRMED | S15 app-only 写入/readback 与分区不变；当前 app 身份核验匹配，但 live model catalog/runtime load 为 `wn9l_hai1tai4ling2`，S15 构建目标为 `wn9_nihaoxiaozhi_tts`。模型修复授权待定；不做目标词验收，不将被动/中断捕获记为 Voice round。Preflight 仍 0/2–3。见 [Candidate 报告](V6_M1_WAKENET_CONTROL_CANDIDATE_REPORT.md) 与 [设备报告](V6_M1_WAKENET_CONTROL_DEVICE_REPORT.md) |
| L-15-SRMODEL-READONLY-VERIFIER | HOST_ACCEPTED | `c4bb339`；只读模型目录、权重哈希与前缀校验；定向 10/10、集成 V6 Host 158/158。真实 live dump 正确返回目标模型缺失；不构成 DEVICE PASS |
| S-17 / R-10 | MODEL_REPAIR_VERIFIED / WAKE_OBSERVED / VOICE_BLOCKED_NAS_TTS_403 | Bundle `claw4-learning-v6-m1-model-repair-s17-20261018-01` 复用 S15 app、不重建；R-10 `PASS`，用户授权的单地址模型写入、prefix/full readback、边界与正确模型启动均核验通过。首个同步尝试用户确认 Wake 并提问但未听到答案；NAS EdgeTTS 403 warning 后续由 L-17 处理。Preflight 已完成 0 轮。见 [修复方案与回执](V6_M1_WAKE_MODEL_REPAIR_PLAN.md) 及 [设备报告](V6_M1_WAKE_MODEL_REPAIR_DEVICE_REPORT.md) |
| L-16-S17-DEVICE-EVIDENCE | DEVICE_EVIDENCE_RECORDED / WAKE_OBSERVED / VOICE_BLOCKED_NAS_TTS_403 | 记录授权写入/读回/边界核验、目标模型启动及 395.281 秒捕获。用户报告 Wake 成功并提问，但无可听答案；NAS 检查见 EdgeTTS 403 warnings，后续修复归 L-17。不是完整 Preflight round；完成轮数仍 0。见 [S-17 设备报告](V6_M1_WAKE_MODEL_REPAIR_DEVICE_REPORT.md) |
| L-17-NAS-EDGE-TTS-FIX / R-11 | NAS_IMAGE_DEPLOYED / REVIEW_PASS | L-17 source `812b12905cda01843f5aeb2d80835abb9bc2b084`；R-11 `PASS`；同一 pinned base，仅 EdgeTTS 7.2.6→7.3.1；NAS image 与 provider probes 详见 [L-18 report](V6_M1_NAS_TTS_REPAIR_REPORT.md)。不判 DEVICE playback |
| L-18-NAS-TTS-EVIDENCE | DOCS_COMPLETE / NAS_TTS_SERVICE_REPAIR_PASS / PREFLIGHT_CHANGES_REQUIRED_SLOW_INCORRECT_RESPONSE | 汇总 NAS build/rollout 与 provider probe，记录 image/config identity；同一 S-17 app/model、未重刷，`preflight-02` 完成但用户反馈慢且答错，completed rounds 0。Ollama latency probe 未得有效结果；不推断首字延迟。M0/AP recovery 与 M1/20-round 门禁保持原状态。见 [报告](V6_M1_NAS_TTS_REPAIR_REPORT.md) 与 [S-17 device report](V6_M1_WAKE_MODEL_REPAIR_DEVICE_REPORT.md) |

Candidate20 已有有限 DEVICE 日志、会话和 readback 事实，但不满足同候选 M0 完整证据；旧全片备份的分区表不同于当前设备，不能作为当前布局的已验证恢复基线。M0 Gate 保持 `CHANGES_REQUIRED`。M1 主机工具与 NAS 部署/合成连通性已完成；M1 验收仍定义为同一候选、同一服务版本下 NAS 连续语音 20 轮和方案规定的故障测试。旧 relay 学习闭环及 Candidate05–08 的 READY 入口均为 `SUPERSEDED`，不构成当前授权。

以下未再次标注的旧看板内容均为历史快照（SUPERSEDED），其中遗留的“当前有效”、READY 或派发入口只保留历史证据，不构成现行队列。

| 顺序 | 工作 | 负责人 | 状态 | 放行与证据 |
| --- | --- | --- | --- | --- |
| 0 | 架构/迁移构建入口/边界检查 | Codex | REVIEW_READY | 代码/文档已交付；78 case PASS，协调器受应用控制阻断，不能标全量 PASS |
| M0-1 | 四方源码冻结 + 环境/恢复清单 | Codex | IN_PROGRESS | SHA/IDF6.1/依赖锁已冻结；32MiB 完整备份和实际布局已核对，恢复写回尚未实测 |
| M0-2 | Claw4 Board Port | Codex | IN_PROGRESS | SD 挂载、metadata-only camera sensor init、触摸与电源键短/长按识别已实机确认；Candidate18 的限时单帧 RAW8 诊断已构建，尚未刷机验证。硬件矩阵解析已覆盖 SD 单次挂载、电源键短/长按和相机单帧/清理结果（接管基线共 83 项 Host 工具测试；C19 构建时为 68 项，之后新增 15 项；不代表新增实机验证）。电源键产品动作未实现。物理 RX ch1 保持静默；诊断版软件参考可进入 AFE ch1，但 AEC 效果未通过 |
| M0-NET | 隐藏网络回退与可复现依赖补丁 | Codex | REVIEW_READY | f47afda；62工具测试、7生产方法Host场景、IDF构建通过；真机未验证 |
| M0-NET-ERR | 网络失败恢复增量 | Codex | REVIEW_READY | 候选10编译链接通过，正常隐藏网络回退及取IP实测；同步调用失败分支未故障注入 |
| M0-3 | Candidate13 回归与音频闭环 | Codex | IN_PROGRESS | Candidate14/15 证实软件参考注入至 AFE ch1；90ms 与 0ms 两组各两次播放结果不一致，且非配对测试，AEC NOT PASS。Candidate17 五分钟静置采样无复位，读取失败/参考丢帧为 0；旧 clipped 字段不具削波证据，Candidate19 的 near_full_scale_n 已构建、未刷机。交互/AEC/播放期 WakeNet 仍待复测。Candidate18 单帧相机取帧等待实机验证；还需设备网络同步失败注入与完整长稳 |
| M1 | NAS 连续语音 20 轮 | Astra/Sol/Luna 按任务拆分 | NAS_TTS_SERVICE_REPAIR_PASS / PREFLIGHT_CHANGES_REQUIRED_SLOW_INCORRECT_RESPONSE; completed=0/2–3 | L-17/R-11 的 EdgeTTS 7.3.1 NAS service repair 与合成 provider probe 通过；不证明设备播放。S-17 未重刷，`preflight-02` 用户反馈响应慢且答错；播放、AEC、near-end、next Wake 均未验证，正式 20 轮未开始，M1 未验收。Ollama latency probe 失败/超时，没有有效首字延迟数据。M0 仍 `CHANGES_REQUIRED`，AP recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。S-17 首次 403 阻断记录保留历史，详见 [S-17 报告](V6_M1_WAKE_MODEL_REPAIR_DEVICE_REPORT.md) 和 [L-18 service repair report](V6_M1_NAS_TTS_REPAIR_REPORT.md)；其他 Candidate 结果不转移 |
| M1.5 | 七命令 pre-LLM router | Codex | BACKLOG | M1；认证/幂等/结果确认契约 |
| M2 | 真实学习闭环 | Codex；可拆 WorkBuddy 辅助 | BACKLOG | M1.5；在线/离线/重启/补传 |
| M2.5 | 离线主动提醒 MVP | Codex | BACKLOG | M2；RTC/TimeAuthority、缓存/内置音频恢复 |
| P0 总验收 | V6 原方案 §10 十一步 | Codex | BACKLOG | 20 轮 + 命令 + 断 NAS 提醒 + 完成 + PWA |
| M3 | Camera/WrongBook 业务 | 待分配 | HOLD | P0 总验收通过 |
| M4 | Learning Memory | 待分配 | HOLD | M3；事实与对话记忆独立 |

历史辅助任务建议（SUPERSEDED，不构成当前授权）：冻结 Board 接口后补硬件矩阵记录模板与重跑脚本；冻结 NAS schema 后补合成集成用例。尚未 READY，不能自行修改 Board/Voice 架构或沿 V5.3 继续接线。实际下发时另给允许路径、Base SHA、独立分支、测试、停止条件，Codex 按不可变提交复核。
