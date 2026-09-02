# Codex 预检与完善报告：WB-STREAM-002

## 1. 结论

- 结论：`GO_TO_START_WB-STREAM-002_HOST_MVP`
- 含义：允许 WorkBuddy **开始当前已发布工作流**，不是跳过它进入 UI、真机或下一工作流。
- 预检时远端：`main` 与 `workbuddy/domain-offline-stream` 均为 `70f762dc16a7d75c7f99066d391a1050cfe1a8d0`。
- 实施状态：CP0～CP8 均未开始；远端无 WorkBuddy 新提交，工作目录干净，`WB-STREAM-002_REPORT.md` 尚不存在。

本报告随预检完善提交进入新的工作流起始基线。WorkBuddy 首次开始时必须以 Codex 最新调度回执给出的精确 hash 为准，不得继续使用上面的旧预检 hash。

## 2. 本轮完善

预检发现原任务包在 sequence 所有权上存在歧义：CP1 曾要求 reducer 获得 next sequence，而 CP2 又要求 outbox 成为唯一持久化 sequence 所有者。若不先收敛，可能出现事务失败后 sequence 被提前消耗或状态与事件计数器不一致。

统一契约如下：

1. 领域 reducer 只输出“下一状态 + 有序 `EventDraft`”。
2. Draft 包含稳定 event_id、身份、时间、类型和 payload，但不包含已提交 sequence。
3. Transactional outbox 在同一事务中分配连续 sequence，物化 `DeviceEvent`，持久化下一状态、事件和计数器。
4. 任一步失败全部回滚，不消耗 sequence；调用方用同一 event_id 重试。
5. UI、reducer 和网络层都没有分配已提交 sequence 的权限。

对应接口、架构和契约测试已经先行更新，WorkBuddy 必须以此为 CP1/CP2 唯一解释。

用户要求 WorkBuddy 一次性持续开发、Codex 最后统一验收。因此当前任务包已扩展为 CP0～CP8：本机 C++ 门槛、领域 reducer、transactional outbox、应用协调器、纯 UI presenter、持久化家庭后端、家长 PWA、主机端到端闭环和稳定性收口。checkpoint 之间不再等待 Codex；只有触发硬门禁或无法在当前范围修复的阻塞才停止。

## 3. 主机工具链预检

当前 PATH/常见位置未发现 `clang++`、`g++`、`cl`、CMake 或 Ninja；只发现 `winget.exe`。这与 CP0 的“先建立真实 compile/link/run 门槛”一致，不构成阻塞。

2026-09-02 的只读 `winget show` 结果：

- 包：`LLVM.LLVM`
- 版本：`22.1.8`
- 发布者：LLVM
- Installer type：Nullsoft
- URL：`https://github.com/llvm/llvm-project/releases/download/llvmorg-22.1.8/LLVM-22.1.8-win64.exe`
- SHA256：`16e5709785fef73c854646241c4a92c5cd574318d1b33c63330dd7721903e55c`

这只是预检证据。WorkBuddy 必须重新读取当次 manifest 并验证实际下载文件 SHA256；不得因本文记录而跳过验证。若只能提权安装、来源不一致或本机程序不能运行，CP0 必须 `BLOCKED`。

前端/部署预检：主机已有 Node 24.14.1、npm 11.11.0；未检出 Docker CLI。PWA 可以实际测试和构建；Compose 只允许生成并静态校验，不得宣称容器已启动。Docker 缺失不阻塞 CP0～CP8 的 SQLite/主机验证。

## 4. 仍然有效的门禁

- 不操作串口、真机、Flash、分区、Bootloader、OTA 或官方 BSP。
- 不实现真实 NVS/文件系统适配，不宣称真实掉电安全。
- 不开始 LVGL UI、Wi-Fi/网络适配、Voice、Camera、AI 或非 MVP 功能。
- 不使用真实账号、儿童数据、密钥、付费或公网业务服务。
- 每个 checkpoint 单独提交、普通 push；CP0～CP8 连续执行，CP8 收口后交由 Codex 一次性最终复检。
