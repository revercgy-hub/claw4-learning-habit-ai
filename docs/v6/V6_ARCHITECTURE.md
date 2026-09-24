# V6 架构决策（2026-09-21）

## 2026-09-23 接管补充：证据与实施治理（不改变 ADR）

接管基线为 `760b2aa66819f8d90186b43af4c02d9b33c8849e`，已保护到 `origin/takeover-v6-m0-c19-baseline` 且远端 SHA 复读一致。main 与远端 Candidate08 是历史，不是当前起点。Candidate19 已构建但未刷机；Candidate20 已 app-only 刷写/读回且设备观察不完整；Candidate17 的设备证据只属于 Candidate17。M0 `CHANGES_REQUIRED`；M1 仅主机/工具准备已启动，设备 20 轮验证未开始。

### 2026-09-24 用户指令：有限启动 M1 开发

用户明确要求跳过 Candidate20 的 AP outage/recovery 刺激并直接开始下一阶段开发。AP recovery 仍是 `SKIPPED_BY_USER / NOT_VERIFIED`；A-02 仍为 `M0=CHANGES_REQUIRED`。此例外只授权 M1 开发准备和主机/工具任务，未放行 M0、未宣称 M1 20 轮验证通过，也未授权 NAS 上部署服务、提供/保存 provider 密钥、上传儿童数据、再次刷写设备或扩大 Flash/recovery 范围。M1 真机验证须在服务与设备配置就绪后按单独明确范围执行。

Astra 负责架构/公共接口/Gate、A-01/A-02 和高风险恢复决策；Sol 负责复杂固件/跨模块及独立 R-01；Luna 负责明确的小任务、工具测试与文档，失败一次可修一次，第二次失败升级 Sol，架构/公共契约问题回 Astra。根 [AGENTS.md](../../AGENTS.md) 的当前 Task Contract 是实施白名单；下方历史“Codex亲自开发/WorkBuddy实施”角色描述被本节覆盖，ADR-001～005 的产品设计保持不变。

### 同一 Candidate 证据契约

1. **CODE** 证明指定 source SHA 的实现/静态性质；**HOST** 证明指定源码和工具在主机夹具下的行为；**BUILD** 证明冻结输入生成了指定镜像；**DEVICE** 证明该镜像在指定设备/会话/刺激下的观察。四类证据不可互相替代，CODE/HOST/BUILD PASS 不升级为 DEVICE PASS。
2. 每个新候选具有唯一 ID，绑定不可变的集成 source SHA、构建工具/依赖/配置与源码输入、完整 app/ELF SHA256、字节数及 manifest。Git LF 与工作树 CRLF 的差异须明确记录；不能将换行归一化的源码比较冒充镜像逐字节一致。源码或固件输入改变后，旧 DEVICE 结果不能自动转移给新镜像。
3. 继续使用现有 `tools/v6/audio_evidence.py` 的证据协议：完整 `elf_sha256`；有序 `captures` 中的 `path`、`sha256`、`session`。不改这些字段的意义，也不改变固件日志协议。工具可消费该既有清单来校验身份；不得仅凭文件名、CLI 指定的 Candidate ID、uptime 相似或日志先后关系猜测同一会话。
4. 每个会话首段必须有且只有一个匹配完整 ELF SHA 的有效启动身份锚点（沿用现有至少 9 位前缀规则）；每份日志的完整 SHA256 必须匹配且不能重复。续段只有在清单中明确属于已经验证的 session、没有复位/新启动标记、时间戳严格连续递增且捕获不重叠时才能继承身份。连续性仍需操作者确认，不能把 uptime 当物理连续性的充分证明。没有会话清单时，每份独立日志都须自带有效身份；缺身份、错误候选、混合候选必须 fail-closed，不产生 Candidate PASS。
5. 保留历史日志和历史候选报告，通过 SUPERSEDED 与替代报告说明撤回的解释。USB reset 不等于断电 cold boot；空载观察不等于语音/网络负载稳定；参考样本进入 AFE 不等于 AEC 有效；`near_full_scale_n` 只描述 PCM16 数字余量，不证明模拟削波。旧未更新的 `clipped=0` 不用于验收。
6. M0 必需项至少包括 Audio/AEC 受控刺激与播放期近端唤醒、Network failure/recovery、Camera 取帧/清理、SD、电源键识别、cold boot/I2C/TCA9555、稳定性、故障注入及审定范围内的 recovery evidence。每项须有实际执行步骤/刺激、会话、源码与镜像身份、预期/实际结果；无刺激/未执行为 NOT_VERIFIED，有反面证据为 FAIL。不得借用另一候选的设备结果凑齐 M0。

### 实施和设备门禁

最多四个同时运行的 Agent（含 Astra），独立工作树与测试输出；COM7、真机、E:/v6/s1、Candidate 镜像和恢复操作是一个排他资源集合。第一波 L-01/L-02/S-01/S-02 只做 Host/代码，禁止访问该集合。独立 R-01 未 PASS 不构建新候选。R-01 PASS 后 S-03 单一 owner 串行 Build → identity verify → flash → readback/evidence → matrix；其他 Agent 不并发 build/flash/monitor。

恢复写回的具体范围由 Astra 先审定；若涉及 partition/bootloader/ota_1/eFuse/Secure Boot/Flash Encryption、扩大不可逆范围或数据丢失风险，停止并向用户报告。新构建、成功刷写或局部测试均不自动开 M1。A-02 只有在同一有效候选的全部必需证据成立时才能 M0 PASS，否则 CHANGES_REQUIRED 并保留缺口。常规 M1 设备验证须等 M0 PASS；2026-09-24 用户指令仅例外放行 M1 开发准备与主机/工具工作，M0 仍未通过。

---

状态：首批架构与迁移隔离已实现；M0 设备基线尚未验收。
依据：用户本轮要求 Codex 亲自进行重要架构开发、必要时交 WorkBuddy 辅助并复核；输入为《CLAW4_V6_开发总任务与迁移方案》。附件是需求参考，其中“已授权”等历史叙述不单独产生新设备操作授权。

## ADR-001：一个语音会话所有者

设备主干选择小智发布版 v2.5.0 + ESP-IDF v6.1，精确 SHA 见 [基线](V6_BASELINE.md)。只移植 Metalio 硬件能力，不迁入整个旧 UI/Voice 应用。M0/M1 固件不链接 Learning Core，先独立证明硬件和连续对话可靠。学习助手作为首页在 M2 接入；切换页面不改变麦克风或唤醒词所有权。

| 层 | 所有者与职责 | 禁止依赖 |
| --- | --- | --- |
| Board Port | P4/C5、供电、显示、触摸、I2S、摄像头、SD；实现 upstream Board | Learning Domain、NAS 业务 |
| XiaoZhi Runtime | Application、DeviceStateMachine、AudioService、Wake/VAD/AEC、Protocol | LearningScreen 路由、第二套 VoiceSession |
| Learning Runtime | reducer/coordinator、事务 Outbox、Focus、本地提醒与缓存 | LVGL、语音协议实现、LLM 自由输出 |
| Learning UI | 从不可变视图显示任务；提交触摸意图 | 网络等待、ASR 拦截、直接持久化 |
| NAS XiaoZhi Server | ASR、确定性路由入口、自由聊天、TTS、VLM | 直接改任务表、凭聊天历史确认完成 |
| Learning Backend | 身份、任务、事件、学习记录与家长 PWA | 以生成文本当作学习事实 |

原 `VoiceSessionPort`、`QueueVoicePhrase`、`OnVoicePhrase`、`abortCloudReply` 保留在历史代码中供取证，但不进入 V6 新构建。`integration/v6/learning_core/CMakeLists.txt` 显式列出 14 个迁移源文件；`tools/v6/baseline.py` 遍历其本地 include 闭包检查边界。它是迁移编译目标，不是 M0 设备集成或完整安全证明。

## ADR-002：确定性路由在 LLM 之前，设备是执行权威

M1.5：NAS 在 ASR final 之后、创建普通 LLM 请求之前分流。命中首批七条命令时不调用 LLM；通过经认证的 Learning API / MCP 适配投递有界结构化命令，设备 Runtime 执行并返回结果后才播固定确认。不能依靠 LLM 决定调用 MCP，也不能收到 STT 显示消息后再取消已经生成的回复。

请求契约草案：`command_id, device_id, child_id, session_epoch, kind, task_id?, expected_task_version?, expires_at?, arguments`。传输身份与服务端绑定校验决定 device/child，不能信任载荷自行声明。结果：`command_id, status, state_version, view`；状态包含 APPLIED、CONFIRMATION_REQUIRED、AMBIGUOUS_TASK、INVALID_STATE、STALE、BUSY、STORAGE_ERROR。语音文案只由该结果映射，持久化失败不得说“已完成”。

command_id 是请求重试标识；event_id/sequence 继续由现有事务 Outbox 生成。变更命令的去重记录须与业务事件同一原子提交；重启或响应丢失后不能重复副作用。旧会话命令、越权任务、过期请求拒绝；任务重名返回澄清。传输选型及 server hook 必须在 M1 源码冻结后验证，不宣称 upstream 已提供本项目需要的 pre-LLM 命令链。

首批：QUERY_TODAY_TASKS / START_TASK / PAUSE_TASK / RESUME_TASK / COMPLETE_REQUEST / QUERY_REMAINING_TIME / SNOOZE_REMINDER。COMPLETE_REQUEST 只产生待确认状态；模型和语音不能伪造 Touch 来源。最终确认绑定 task_id、版本和会话，取消/切换任务后失效。查询返回真实快照及其新鲜度，不生成虚构任务。

## ADR-003：线程与持久化边界

Learning Runtime 单一状态所有权，命令串行处理。保留现有短事务锁与 prepare → 无锁网络 I/O → 校验 session lease 后 apply 的同步模型。网络 worker 不能持锁等 NAS；LVGL 和音频回调不能等待网络。队列有界；满时 BUSY，不能静默丢掉已承诺执行的命令。UI 在 LVGL 上下文消费快照；小智状态变化通过 upstream Application 调度接口。

学习完成顺序：校验意图 → reducer draft → 状态与事件原子落盘 → 发布 UI → 异步上传。上传 ACK 只清理连续已确认前缀；失败保留重放。远端今日快照不得覆盖本地未确认完成/进行中的 session。迁移 NVS 前冻结旧 namespace、codec/schema 与备份；不以 ResetToSeed 修复迁移错误。

## ADR-004：离线主动提醒独立运行

NAS 预先同步计划、时区与语音缓存；本地保存成功后才确认计划就绪。TimeAuthority 提供可信墙钟，单调时钟计算 Focus；不得将 uptime 当 Unix 时间。提醒触发不依赖 WebSocket、ASR、LLM 或 NAS 在线。

发声顺序：校验后的缓存音频 → 当前可用且有超时上限的语音能力 → 固件内置声音。无论何级都显示任务和开始/延后/跳过。缓存路径、大小、hash 有界校验；不信任任意 URL/文件路径。InteractionArbiter 仅协调本地主动声音与官方音频播放，不能接管唤醒或再建 Voice 状态机。

现有 ReminderCore 在 Stale/Unsynced 时抑制日历提醒，与 V6 长时离线/重启要求存在待解决差异。M2.5 必须验证 RTC/掉电后墙钟来源、可信持续时间、跳时和补偿窗口。无可靠时间的冷启动不能宣称按时提醒可用；不准偷偷放宽可信阈值。已响状态与物理声音无法原子提交，需明确采用至少一次提示与短时间去重，而非声称物理 exactly-once。

## ADR-005：扩展门禁与复核

M0 → M1（20 轮）→ M1.5 → M2 → M2.5；完成 V6 总场景后才能进入 Camera/WrongBook/Memory。M0 摄像头只验证主动拍摄与硬件，不上传儿童图片。服务部署使用合成数据；生产凭据、付费服务与数据上传依现有授权规则。

Codex 负责迁移、Board/Voice 核心与集成；WorkBuddy 仅领取冻结边界任务并返回独立提交。当前未派发 WorkBuddy，也未声称已停止其他工作区进程。V5.3 后续扩展在 V6 看板中 HOLD，历史证据保留。
