# Claw4 项目协作总则

本文件适用于整个项目根目录。子目录存在更具体的 `AGENTS.md` 时，子目录规则只补充其作用域内的技术约束；若发生冲突，以本文件定义的角色分工、任务状态和验收流程为准，产品事实仍以 `项目总规划/AGENTS.md` 为准。

## 1. 角色与权限

### Codex：总控、调度与复检

Codex 负责：

1. 维护阶段目标、依赖关系、优先级和风险门禁。
2. 每次只向 WorkBuddy 发布一个可执行的 `READY` 任务包。
3. 明确任务输入、允许修改路径、禁止事项、验收标准和停止条件。
4. 复查 WorkBuddy 的 Git diff、测试证据和报告。
5. 给出 `ACCEPTED`、`CHANGES_REQUIRED` 或 `BLOCKED` 结论并更新任务看板。
6. 只有验收通过后才安排下一个依赖任务。

除非用户明确要求或为了修复本调度体系，Codex 不代替 WorkBuddy 实施产品功能。

### WorkBuddy：实施

WorkBuddy 负责：

1. 只领取 `docs/project_management/TASK_BOARD.md` 中唯一的 `READY` 任务。
2. 开始前完整阅读本文件、任务包及任务包列出的输入文件。
3. 严格在任务包允许的路径内修改；发现范围外问题只记录，不顺手修改。
4. 执行任务包指定的验证，并保留命令、关键输出和失败证据。
5. 按模板生成任务报告并提交到自己的任务分支。
6. 完成后请求 Codex 复检；WorkBuddy 无权自行标记 `ACCEPTED` 或启动后续任务。

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
6. 产品总规划和 Bring-up 手册；
7. 历史报告与 WorkBuddy memory。

历史报告与新实测冲突时，不删除历史证据，但必须标记 `SUPERSEDED`，并链接到替代报告。`.workbuddy/memory/` 仅供参考，不得作为当前状态的唯一依据。

## 3. 状态机

允许状态：

- `BACKLOG`：尚未满足调度条件。
- `READY`：输入、依赖和范围已明确，可以领取。
- `IN_PROGRESS`：WorkBuddy 正在执行。
- `REVIEW_READY`：实现与报告已提交，等待 Codex。
- `CHANGES_REQUIRED`：复检未通过，仅允许处理评审项。
- `ACCEPTED`：Codex 已验收。
- `BLOCKED`：外部前置缺失，不能继续。
- `HOLD`：阶段策略主动暂停。
- `CANCELLED`：任务取消。

任意时刻最多一个 WorkBuddy 任务处于 `READY` 或 `IN_PROGRESS`。任务看板由 Codex维护；WorkBuddy 在报告中声明状态，不直接把自己标为验收通过。

## 4. 标准工作流

```text
Codex 盘点与拆解
  -> 发布唯一 READY 任务包
  -> WorkBuddy 建任务分支并实施
  -> WorkBuddy 验证、写报告、提交
  -> Codex 复检 diff 与证据
  -> ACCEPTED / CHANGES_REQUIRED / BLOCKED
  -> Codex 更新看板并调度下一任务
```

WorkBuddy 开始任务时使用分支：

```text
workbuddy/<task-id>-<short-name>
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
- Bring-up Stage 1 未由 Codex 验收前，学习业务代码保持 `HOLD`。
- MVP 阶段不开发持续摄像、情绪/人脸识别、本地大模型、复杂数字人、4G/GPS 等非核心功能。
