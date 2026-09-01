# 项目任务看板

- 更新时间：2026-09-01
- 维护者：Codex
- 当前阶段：主机基线已通过；补齐平台证据；等待真机连接
- 调度规则：任意时刻只允许一个 WorkBuddy 任务为 `READY` 或 `IN_PROGRESS`
- 项目远端：[`revercgy-hub/claw4-learning-habit-ai`](https://github.com/revercgy-hub/claw4-learning-habit-ai)（私有）

## 当前唯一指令

WorkBuddy 先按 [`WORKBUDDY_GIT_SYNC.md`](WORKBUDDY_GIT_SYNC.md) 将本地任务分支与远端安全对齐，然后只处理 `WB-001` 的 Codex 复检修订项，依据：[`reports/CODEX_REVIEW_WB-001_2026-09-01.md`](reports/CODEX_REVIEW_WB-001_2026-09-01.md)。其他任务不得提前开始。

## 看板

| 顺序 | 任务 ID | 负责人 | 状态 | 依赖 | 交付物/证据 |
| ---: | --- | --- | --- | --- | --- |
| 0 | CTRL-001 | Codex | `ACCEPTED` | 无 | 根协作规范、看板、模板、Git 边界 |
| 1 | AUD-001 | 历史产出 / Codex 复核 | `ACCEPTED` | 无 | `docs/CLAW4_AUDIT.md`、`docs/HARDWARE_ASSUMPTIONS.md` |
| 2 | BLD-001 | 历史产出 / Codex 复核 | `ACCEPTED` | AUD-001 | `docs/BUILD.md`、`docs/CLAW4_主机准备情况报告_2026-09-01.md`、`E:\b` 构建产物 |
| 2.1 | WB-ENV-REVIEW | WorkBuddy / Codex 复核 | `ACCEPTED` | BLD-001 | `docs/CLAW4_报告复核_2026-09-01.md`，独立复跑主机检查 |
| 3 | WB-001 | WorkBuddy | `CHANGES_REQUIRED` | AUD-001、BLD-001 | 分支 `workbuddy/wb-001-platform-map` @ `dbdc691`；按 Codex 复检修订 |
| 4 | CR-001 | Codex | `ACCEPTED` | WB-001 `REVIEW_READY` | `CODEX_REVIEW_WB-001_2026-09-01.md`；本轮结论为 `CHANGES_REQUIRED` |
| 5 | WB-002 | WorkBuddy | `BACKLOG` | CR-001 `ACCEPTED` | `docs/ARCHITECTURE.md`，仅架构文档，不写业务代码 |
| 6 | CR-002 | Codex | `BACKLOG` | WB-002 `REVIEW_READY` | 架构验收报告 |
| 7 | WB-BRINGUP-S1 | WorkBuddy | `BLOCKED` | 真机、数据线、串口、用户授权 | B001/B002/B003/B004/B005/B009/B013 + `BRINGUP_STAGE1_REPORT.md` |
| 8 | CR-BRINGUP-GATE | Codex | `BACKLOG` | WB-BRINGUP-S1 `REVIEW_READY` | Stage 1 复检和 GO/NO-GO |
| 9 | WB-MVP-INTERFACES | WorkBuddy | `HOLD` | G2/G3 门禁 | `learning_domain`、`sync`、`ui`、`assistant`、`telemetry` 接口骨架 |
| 10 | WB-MVP-DOMAIN | WorkBuddy | `HOLD` | 接口骨架验收 | Task、StudySession、DeviceEvent、DomainState 与单元测试 |
| 11 | WB-MVP-MOCK | WorkBuddy | `HOLD` | Domain 验收 | Mock Backend：today tasks、events、session |
| 12 | WB-MVP-UI-HOME | WorkBuddy | `HOLD` | Mock Backend 验收 | Home 页面与 Mock Tasks |
| 13 | WB-MVP-UI-FOCUS | WorkBuddy | `HOLD` | Home 验收 | Focus 页面与本地倒计时 |
| 14 | WB-MVP-EVENTS | WorkBuddy | `HOLD` | Focus 验收 | task.start、task.complete |
| 15 | WB-MVP-QUEUE | WorkBuddy | `HOLD` | Events 验收 | Local Event Queue |

## 当前阻塞

| 阻塞 ID | 影响任务 | 证据 | 解除条件 |
| --- | --- | --- | --- |
| BLK-HW-001 | WB-BRINGUP-S1 及所有业务实现 | 2026-09-01 复检返回 `NO_SERIAL_PORTS_DETECTED` | 连接 Claw4，记录 USB/COM 枚举并确认 P4 调试口 |

## 已解除阻塞

| 阻塞 ID | 状态 | 解除证据 | 结果 |
| --- | --- | --- | --- |
| BLK-GIT-REMOTE-001 | `RESOLVED` | [`GIT_PROJECT_SETUP_2026-09-01.md`](reports/GIT_PROJECT_SETUP_2026-09-01.md) | 已创建独立私有仓库并推送 `main` 与 `workbuddy/wb-001-platform-map`；未使用 Metalio 官方 origin |

## 已知风险

- 官方分区 `ota_0` 约 9 MiB、`ota_1` 约 4 MiB，当前应用约 8.62 MiB，不能假定双槽对称 OTA 可用。
- 官方 `sdkconfig` 中 Flash mode 选择项与字符串值存在不一致迹象，WorkBuddy 只能记录，不能擅自修正。
- 真机 SKU、屏驱、PSRAM、Flash、C5、触摸等均需实机确认。
- 历史 `docs/系统检查报告_2026-09-01.md` 已被后续主机准备报告取代，不得继续据其重新安装工具链。
