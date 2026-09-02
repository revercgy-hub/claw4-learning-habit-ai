# WorkBuddy 连续开发与 Codex 异步修复协议

## 1. 目的

减少“实现一个小任务 → 等待复检 → 再领取下一任务”的空转时间。WorkBuddy 在一个预授权工作流内连续实施，Codex 按不可变 Git checkpoint 异步复检，并在工作流后段直接修复普通缺陷。

本协议不扩大产品范围，也不降低 Flash、分区、安全、儿童隐私和真实外部服务门禁。

## 2. 分支模型

```text
main                              已验收基线
  └─ workbuddy/<stream>           WorkBuddy 连续开发，只追加普通提交
       └─ checkpoint commits      每项单独提交并 push

codex/<stream>-review-fixes       Codex 从已审查 checkpoint 建立修复分支
```

- WorkBuddy 不在 `main` 开发，不 force push、不 rebase、不改写历史。
- Codex 不在 WorkBuddy 正在写入的分支并发修改；复检以远端 checkpoint hash 为准。
- 普通修复由 Codex 在 `codex/` 分支完成并补测；待 WorkBuddy checkpoint 边界稳定后再普通合并。
- 只有验收后的提交进入 `main`。

## 3. 连续执行规则

1. 任务包一次给出有序 checkpoint、允许路径、验证与停止条件。
2. WorkBuddy 每次只实施当前 checkpoint。
3. 验证通过后更新工作流报告，以独立提交普通 push。
4. 立即开始下一个已列出的 `QUEUED` checkpoint，无需等待 Codex 回复。
5. 前一 checkpoint 测试失败、发生范围偏差或触发停止条件时，不得跳过；修复当前项或停止报告。
6. 未列入工作流的功能、路径或外部依赖不得顺手加入。

## 4. Codex 复检与修复等级

| 等级 | 示例 | 处理 |
| --- | --- | --- |
| P0 门禁 | Flash/分区、密钥泄漏、儿童数据、不可逆破坏 | 立即暂停受影响 checkpoint，必须取得用户授权或先恢复安全基线 |
| P1 契约 | 数据丢失、认证绕过、幂等/状态机错误 | Codex 可直接修复并补回归测试；依赖该契约的合入暂停 |
| P2 实现 | 边界条件、错误码、可维护性、测试缺口 | Codex 在修复分支直接处理，不阻断 WorkBuddy 开发非冲突 checkpoint |
| P3 文档 | 命名、报告、注释、格式 | Codex 直接修复，不中断工作流 |

## 5. 仍然有效的硬门禁

- 未经用户明确授权，不读取、擦除或刷写 Flash，不修改 partition table、Bootloader、Secure Boot、Flash Encryption 或 OTA 策略。
- 不向真实外部服务发送儿童数据，不引入真实密钥、付费账号或生产凭据。
- 不修改 `vendor/MetalioClaw4` 官方基线；需要集成时另发任务。
- 未完成 Stage 1 前，不把屏幕、触摸、音频、摄像头、存储、电源等写成真机已通过。
- 持续摄像、情绪/人脸识别、本地大模型、4G/GPS 等非 MVP 功能继续禁止。

## 6. 工作流报告

每个 checkpoint 至少记录：

- checkpoint ID、提交主题和父提交；当前报告与代码同属一个提交时写“本 checkpoint 提交自身”，精确 hash 由 push 回执给出，并在下一 checkpoint 报告更新时回填；
- 修改文件与范围偏差；
- 验收标准逐项结果；
- 实际验证命令、关键输出与失败；
- 后续 checkpoint 是否可继续；
- 建议 Codex 复检和直接修复的位置。
