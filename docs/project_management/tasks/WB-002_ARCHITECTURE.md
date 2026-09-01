# WB-002：Claw4 学习习惯终端 MVP 架构定义

## 调度信息

- 负责人：WorkBuddy
- 复检人：Codex
- 优先级：P0
- 当前状态：`READY`
- 目标分支：`workbuddy/wb-002-architecture`
- 前置：WB-001、WB-HW-001、WB-HW-002 已验收
- 性质：开发前架构设计；**只写文档，不实现业务代码**

## 目标

基于已验收的平台源码映射和真机证据，形成可直接用于后续小步开发的 MVP 架构基线，明确设备固件、家庭后端、家长 PWA、AI Gateway、数据模型、状态机、事件同步、离线策略、安全隐私和模块依赖边界。

本任务必须把“已确认的平台事实”“产品要求”“架构决策”“仍待硬件验证”分开，不得用源码默认配置替代实机结论。

## 开始前必须读取

1. `AGENTS.md`
2. `项目总规划/AGENTS.md`
3. `docs/project_management/TASK_BOARD.md`
4. `docs/project_management/WORKBUDDY_GIT_SYNC.md`
5. 本任务包
6. `docs/CLAW4_AUDIT.md`
7. `docs/CLAW4_PLATFORM_MAP.md`
8. `docs/HARDWARE_ASSUMPTIONS.md`
9. `docs/BOARD_REVISION.md`
10. `docs/DEVICE_LOG_REFERENCE.md`
11. `docs/PHYSICAL_INSPECTION.md`
12. `docs/BUILD.md`
13. `docs/project_management/reports/CODEX_REVIEW_WB-HW-002_ROUND2_2026-09-01.md`
14. 官方只读源码 `vendor/MetalioClaw4` 中与板级、应用入口、网络、协议、显示、存储相关的实际目录；只读检查，不修改

## 允许修改

- `docs/ARCHITECTURE.md`
- `docs/project_management/reports/WB-002_REPORT.md`

不得创建源码目录、示例代码、构建配置、Docker 文件、API 实现、测试工程或上述列表之外的文件。

## `docs/ARCHITECTURE.md` 必须包含

### 1. 范围与证据分层

- MVP 唯一闭环：今日任务 → 开始 → StudySession → 专注/暂停/恢复/完成 → 本地事件 → 后端同步 → 家长端可见。
- 明确 MVP 非目标：4G/GPS、持续摄像、情绪/人脸识别、本地大模型、复杂数字人、排行榜、社交等。
- 使用 `PRODUCT_REQUIRED`、`SOURCE_CONFIRMED`、`DEVICE_LOG_CONFIRMED`、`ARCH_DECISION`、`HARDWARE_VERIFY_REQUIRED`、`UNKNOWN` 标签。

### 2. 系统上下文与数据所有权

- Claw4、家庭后端、家长 PWA、AI Gateway、数据库之间的依赖方向。
- 家长端不得直连设备；AI Provider 不得由固件直接访问。
- 明确 Task、StudySession、Event、DeviceConfig、用户/儿童数据的权威所有者与设备缓存副本。

### 3. 设备侧模块边界

至少定义：

- `device_services`：对官方 BSP/网络/存储/电源的适配边界；
- `learning_domain`：Task、StudySession、Timer、Reward，禁止依赖 LVGL、Wi-Fi 或 GPIO；
- `sync`：API client、event queue、offline store、retry、time sync；
- `ui`：只消费状态和发出 intent，不拥有业务真相；
- `assistant`：MVP 只定义 intent/router 边界，不接入复杂 LLM；
- `telemetry`：结构化日志和最小运行指标。

为每个模块写职责、允许依赖、禁止依赖、输入/输出接口和未来源码落点。不得承诺尚不存在的接口已经实现。

### 4. 状态机与不变量

- 设备一级状态与学习会话子状态的分层关系。
- Task 与 StudySession 生命周期和关键转换。
- 暂停、恢复、重复点击完成、设备重启、断网、时间未同步、后端拒绝等异常路径。
- 至少列出这些不变量：Task 不等于 StudySession；AI 不得完成任务；完成事件不可丢；UI 不直接改服务器真相；业务层不直接访问 GPIO。

### 5. 数据契约

- Task、StudySession 和统一 Event envelope 的字段、类型、必填性、版本和时间语义。
- `event_id` 幂等、device sequence 排序、at-least-once delivery、重复事件处理和冲突策略。
- 不得把完整服务器 JSON 作为设备状态覆盖方案。

### 6. API 与同步协议边界

- 只定义 MVP 必需契约：设备注册/认证、今日任务、StudySession、事件批量同步、配置/心跳。
- 给出请求/响应的最小 JSON 示例、状态码类别、重试分类和幂等键；示例不得含真实密钥或儿童信息。
- HTTPS/WSS、CA 与 hostname 校验、短期 token、每设备凭据；禁止 `verify=false` 和固件共享永久 API Key。

### 7. 离线与持久化策略

- 断网时仍能查看缓存任务、开始/暂停/完成和记录事件。
- 事件队列写入顺序、确认删除、掉电恢复、容量上限、退避和死信/隔离策略。
- 不把 microSD 作为 MVP 强依赖；具体 NVS/FAT/Flash 落点标为后续实现决策，且不得修改分区表。

### 8. UI、并发与性能边界

- Home/Focus/Done/Offline 四页和 intent 流转。
- LVGL 线程归属、跨任务消息传递、定时器与网络回调不得直接操作 UI。
- 基于 360 MHz、32 MB PSRAM 的已确认设备事实制定预算；屏驱、触摸和 Flash 容量仍不得写成实机确认。
- 列出后续可测指标，不把未实测 FPS、触摸延迟或内存余量写成已达成。

### 9. 后端/PWA/AI 边界

- 推荐目录与组件职责可以提出，但必须标 `ARCH_DECISION`，不得写成现有实现。
- 家庭后端负责身份、任务、session、事件、统计、权限和 AI 调用。
- PWA MVP 只覆盖 Dashboard、今日任务、学习记录和设备状态。
- 摄像头在 MVP 默认关闭；语音只保留未来 intent 接口，不在本任务扩展。

### 10. 隐私、安全、可观测性和失败矩阵

- 儿童数据最小化、日志脱敏、音频/图片上传触发与保留边界。
- 结构化日志字段和禁止记录项。
- 至少覆盖：无任务、断网、DNS错误、token失效、API 500、重复事件、队列满、时间同步失败、设备重启、后端重启。

### 11. 分阶段实施图

- 把后续工作拆成可独立验收的小任务，至少包括：接口骨架、领域模型、Mock Backend、Home、Focus、事件、离线队列。
- 每项写输入、允许路径建议、测试类型、依赖和停止条件。
- 明确 G2/G3 未通过前这些任务保持 `HOLD`，本架构文档不自动授权编码。

### 12. 决策与未决项

- 建立精简 ADR/Decision 表：决策、理由、证据、影响、可逆性。
- 建立 `HARDWARE_VERIFY_REQUIRED` 与产品待确认清单，链接现有证据文档，避免复制过期结论。

## 验收标准

- [ ] Git diff 只有两个允许文件。
- [ ] 架构覆盖上述 12 个章节，术语和状态机前后一致。
- [ ] 设备、后端、PWA、AI 与数据所有权边界清晰，无循环依赖。
- [ ] Task、StudySession、Event 契约足以指导后续单元测试与 Mock Backend，但未写实现代码。
- [ ] 离线队列、幂等、顺序、重试和掉电恢复规则可验证。
- [ ] 没有业务层直连 GPIO/LVGL/网络，没有家长端直连设备，没有设备端直连 AI Provider。
- [ ] 没有把 4G、屏驱、触摸、Flash 容量、分区实际布局或照片接口功能写成实机已确认。
- [ ] 没有引入真实儿童数据、凭据、服务账号或付费外部依赖。
- [ ] 没有修改源码、官方 BSP、固件、分区、构建或硬件状态。
- [ ] 报告包含实际验证命令、结果、风险和 Codex 复检重点。
- [ ] `git diff --check origin/main...HEAD` 无错误。

## 验证要求

至少执行并记录：

```powershell
git diff --check
git diff --name-only
rg -n "PRODUCT_REQUIRED|SOURCE_CONFIRMED|DEVICE_LOG_CONFIRMED|ARCH_DECISION|HARDWARE_VERIFY_REQUIRED|UNKNOWN" docs/ARCHITECTURE.md
rg -n "Task|StudySession|event_id|sequence|at-least-once|幂等|离线|GPIO|LVGL|HTTPS|WSS|verify=false" docs/ARCHITECTURE.md
```

还要人工检查所有本地文件链接存在、JSON 示例可解析、状态转换无明显矛盾。

## 停止条件

- 需要选择会显著影响后续实现且现有产品事实无法决定的技术栈或存储方案；
- 需要修改两个允许路径之外的文件；
- 发现官方源码与已验收平台映射存在新的实质冲突；
- 需要硬件操作、账号、密钥、外部服务、下载或安装依赖；
- 任务自然扩大为源码实现。

完成后只提交本任务并声明 `REVIEW_READY` 或 `BLOCKED`。不得创建后续代码任务、修改看板、合并 `main` 或开始实现。
