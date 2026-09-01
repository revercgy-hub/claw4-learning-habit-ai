# 项目任务看板

- 更新时间：2026-09-01
- 维护者：Codex
- 当前阶段：G0/G1 已通过；真机已枚举；准备只读设备接收检查
- 调度规则：任意时刻只允许一个 WorkBuddy 任务为 `READY` 或 `IN_PROGRESS`
- 项目远端：[`revercgy-hub/claw4-learning-habit-ai`](https://github.com/revercgy-hub/claw4-learning-habit-ai)（私有）

## 当前唯一指令

WorkBuddy 先按 [`WORKBUDDY_GIT_SYNC.md`](WORKBUDDY_GIT_SYNC.md) 取得 `workbuddy/wb-hw-001-readonly-intake`，然后只执行 [`WB-HW-001_READONLY_DEVICE_INTAKE.md`](tasks/WB-HW-001_READONLY_DEVICE_INTAKE.md)。本轮只做枚举和被动证据采集，禁止刷写、擦除、读取 Flash、发送串口/AT 命令或修改固件。

## 看板

| 顺序 | 任务 ID | 负责人 | 状态 | 依赖 | 交付物/证据 |
| ---: | --- | --- | --- | --- | --- |
| 0 | CTRL-001 | Codex | `ACCEPTED` | 无 | 根协作规范、看板、模板、Git 边界 |
| 1 | AUD-001 | 历史产出 / Codex 复核 | `ACCEPTED` | 无 | `docs/CLAW4_AUDIT.md`、`docs/HARDWARE_ASSUMPTIONS.md` |
| 2 | BLD-001 | 历史产出 / Codex 复核 | `ACCEPTED` | AUD-001 | `docs/BUILD.md`、`docs/CLAW4_主机准备情况报告_2026-09-01.md`、`E:\b` 构建产物 |
| 2.1 | WB-ENV-REVIEW | WorkBuddy / Codex 复核 | `ACCEPTED` | BLD-001 | `docs/CLAW4_报告复核_2026-09-01.md`，独立复跑主机检查 |
| 3 | WB-001 | WorkBuddy | `ACCEPTED` | AUD-001、BLD-001 | 最终实施提交 `45b2c73`；`docs/CLAW4_PLATFORM_MAP.md` 与 `WB-001_REPORT.md` 已验收并纳入 main |
| 4 | CR-001 | Codex | `ACCEPTED` | WB-001 `REVIEW_READY` | `CODEX_REVIEW_WB-001_2026-09-01.md`；本轮结论为 `CHANGES_REQUIRED` |
| 4.1 | CR-001-R2 | Codex | `ACCEPTED` | WB-001 修订提交 `a829765` | `CODEX_REVIEW_WB-001_ROUND2_2026-09-01.md`；技术内容通过，证据行号仍需窄范围修订 |
| 4.2 | CR-001-FINAL | Codex | `ACCEPTED` | WB-001 最终提交 `45b2c73` | `CODEX_REVIEW_WB-001_FINAL_2026-09-01.md`；G1 `PASSED` |
| 4.5 | WB-HW-001 | WorkBuddy | `READY` | WB-001、设备 USB 枚举 | 只读设备接收、COM/USB 映射、官方固件被动日志、`BOARD_REVISION.md`、`DEVICE_LOG_REFERENCE.md` |
| 5 | WB-002 | WorkBuddy | `BACKLOG` | WB-HW-001 `ACCEPTED` | `docs/ARCHITECTURE.md`，仅架构文档，不写业务代码 |
| 6 | CR-002 | Codex | `BACKLOG` | WB-002 `REVIEW_READY` | 架构验收报告 |
| 7 | WB-BRINGUP-S1 | WorkBuddy | `BACKLOG` | WB-HW-001；恢复路径；涉及刷写时需用户明确授权 | B001/B002/B003/B004/B005/B009/B013 + `BRINGUP_STAGE1_REPORT.md` |
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
| BLK-FLASH-AUTH-001 | 任何固件刷写、Flash 读取/擦除、分区或 OTA 操作 | 用户尚未明确授权；恢复路径、当前 boot partition 和 SKU 尚未确认 | Codex 完成只读接收复检后，另行向用户说明目标、风险和恢复方案并取得明确授权 |

## 已解除阻塞

| 阻塞 ID | 状态 | 解除证据 | 结果 |
| --- | --- | --- | --- |
| BLK-GIT-REMOTE-001 | `RESOLVED` | [`GIT_PROJECT_SETUP_2026-09-01.md`](reports/GIT_PROJECT_SETUP_2026-09-01.md) | 已创建独立私有仓库并推送 `main` 与 `workbuddy/wb-001-platform-map`；未使用 Metalio 官方 origin |
| BLK-HW-001 | `RESOLVED` | [`CODEX_DEVICE_INTAKE_2026-09-01.md`](reports/CODEX_DEVICE_INTAKE_2026-09-01.md) | 已检测 COM3/4/5/6；COM3 明确枚举为 Espressif USB JTAG/serial debug unit，可进入只读接收检查 |

## 已知风险

- 官方分区 `ota_0` 约 9 MiB、`ota_1` 约 4 MiB，当前应用约 8.62 MiB，不能假定双槽对称 OTA 可用。
- 官方 `sdkconfig` 中 Flash mode 选择项与字符串值存在不一致迹象，WorkBuddy 只能记录，不能擅自修正。
- 真机 SKU、屏驱、PSRAM、Flash、C5、触摸等均需实机确认。
- 历史 `docs/系统检查报告_2026-09-01.md` 已被后续主机准备报告取代，不得继续据其重新安装工具链。
