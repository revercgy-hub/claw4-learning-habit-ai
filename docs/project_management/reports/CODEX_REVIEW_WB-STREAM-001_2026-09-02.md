# Codex 复检报告：WB-STREAM-001 MVP 核心连续开发流

## 1. 结论

- 复检对象：`workbuddy/mvp-core-stream`
- CP0：`72b77ee9cd328bccf27fecbdf32194ab12505e00`
- CP1：`a38dfad3b5d307e734af0b82999151c7878092ef`
- CP2：`bc41ff3bfb26a02b2df7041e1ed7ea2b4e85a63e`
- Codex 修复：`f021233851a386977aba72b8d15bd8cdd4c0c33f`
- 修复分支：`codex/wb-stream-001-review-fixes`
- 最终结论：`ACCEPTED_WITH_CODEX_FIXES`

WorkBuddy 三个 checkpoint 的提交边界、允许路径和基础验证有效。复检发现的身份、并发、事件 envelope 与原子接口问题已由 Codex 在独立分支修复并补充回归测试；不要求 WorkBuddy 回头修改已经冻结的工作流分支。

## 2. Git 与范围检查

| 检查 | 结果 |
| --- | --- |
| CP0 相对父提交 | 仅架构与工作流报告，符合任务包 |
| CP1 相对 CP0 | 仅设备侧纯接口、契约测试与验证脚本，符合任务包 |
| CP2 相对 CP1 | 仅 Mock Backend 与工作流报告，符合任务包 |
| force/rebase 迹象 | 未发现；远端为普通快进提交链 |
| 官方源码、Flash、串口、分区 | 未修改、未操作 |
| 真实凭据、儿童数据、外部业务服务 | 未引入 |

说明：Codex 修复提交创建时继承了仓库已有 `WorkBuddy <workbuddy@local>` Git identity；提交内容和本报告实际执行者为 Codex。为避免改写已推送历史，保留原提交对象，后续 Codex 提交显式指定 Codex identity。

## 3. 复检问题与修复

| ID | 级别 | 问题 | 修复与证据 |
| --- | --- | --- | --- |
| CR-WBS001-01 | P1 | `/events/batch` 未逐事件校验 `device_id` | 校验 token、批次与每个事件的设备身份；伪造事件回归测试 |
| CR-WBS001-02 | P1 | 同设备并发批次可能同时接受相同 sequence | 增加稳定的设备级批次锁；并发同序号测试保证仅一个 accepted |
| CR-WBS001-03 | P1 | token 的 child 声明未与当前服务端绑定重验 | 每次任务查询和事件请求取 token 声明与当前绑定交集；解绑旧 token 测试 |
| CR-WBS001-04 | P2 | 错误签名会提前消费 challenge，且并发重放缺少原子消费 | 验签成功后原子消费；错误后正确重试与并发双请求仅一胜测试 |
| CR-WBS001-05 | P1 | 非法 type/version/source/sequence 可能推进连续 ACK | envelope 语义校验前置，非法事件返回 422 且 ACK 不变 |
| CR-WBS001-06 | P1 | 幂等摘要未覆盖完整不可变 envelope | 摘要覆盖 child、timestamp、source、version 等；篡改重放返回 conflict |
| CR-WBS001-07 | P1 | `EventSink` 只接收事件，无法兑现“状态快照+事件”原子提交 | 改为 `PendingTransition`，一次提交完整下一状态与全部事件 |
| CR-WBS001-08 | P2 | 活动 session 默认 `completion_type=Normal`，与未结束语义冲突 | 改为 optional；只在 Completed/Aborted 设置，并增加契约断言 |
| CR-WBS001-09 | P2 | Task 缓存在 DomainState/ViewState 重复 | Task 缓存归并为领域状态唯一来源，UI 只读映射 |
| CR-WBS001-10 | P3 | 架构状态值和主机侧开放策略落后于实际门禁 | 补齐 conflict/gap、身份重验、处理顺序和主机侧条件开放文本 |

## 4. 独立验证

| 验证 | 结果 |
| --- | --- |
| Backend 全量 pytest | 5 轮，每轮 `28 passed` |
| 并发事件批次 | 相同 sequence 并发时恰好一个 accepted，另一请求 rejected |
| 并发 challenge 重放 | 恰好一个 200，另一请求 401 |
| Python bytecode 编译 | PASS |
| `pip check` | `No broken requirements found` |
| C++17 接口契约 | P4 RISC-V GCC 14.2.0，16/16 头语法检查与契约单元 PASS |
| 架构 JSON | 12/12 可解析 |
| 架构本地链接 | 21/21 存在 |
| `git diff --check` | PASS（仅 Git 的 CRLF 转换提示，无空白错误） |

残余非阻塞项：FastAPI 0.141.1 的 `TestClient` 产生一条上游 `StarletteDeprecationWarning`；不影响本轮契约正确性，后续依赖维护时处理，不在本轮扩大范围。

## 5. 门禁与下一步

- `WB-002 / CP0`、接口 CP1、Mock Backend CP2 均转为 `ACCEPTED`。
- 下一唯一活动流为 `WB-STREAM-002`，只实施本机 C++ 运行测试门槛、纯领域状态机和平台无关 outbox 核心。
- 真实 NVS、LVGL、Wi-Fi、设备固件集成、串口与 Flash 操作继续 `HOLD`。
- 未经用户另行明确授权，不得刷写/擦除 Flash、修改分区/Bootloader/OTA，也不得接入真实儿童数据或外部付费服务。
