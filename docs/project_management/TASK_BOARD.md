# 项目任务看板

- 更新时间：2026-09-02
- 维护者：Codex
- 当前阶段：G0/G1 已通过；WB-STREAM-001 已复检并由 Codex 修复，启动 WB-STREAM-002 主机侧领域与离线核心流
- 调度规则：任意时刻只允许一个活动 WorkBuddy 工作流；流内 checkpoint 按任务包顺序连续执行
- 项目远端：[`revercgy-hub/claw4-learning-habit-ai`](https://github.com/revercgy-hub/claw4-learning-habit-ai)（私有）

## 当前活动工作流

`WB-STREAM-002` 为唯一活动工作流。WorkBuddy 在 `workbuddy/domain-offline-stream` 按 `WB-STREAM-002_DOMAIN_OFFLINE.md` 连续执行 CP0（本机 C++ 运行门槛）→ CP1（纯领域 reducer）→ CP2（平台无关 transactional outbox），每个 checkpoint 提交并 push 后不等待 Codex，直接继续下一项。不得越出任务包路径或进入真机、NVS 适配、Flash、UI 或非 MVP 功能。

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
| 4.5 | WB-HW-001 | WorkBuddy | `ACCEPTED` | WB-001、设备 USB 枚举 | 实施提交 `9867a56`；COM/USB 映射、官方固件日志、`BOARD_REVISION.md`、`DEVICE_LOG_REFERENCE.md` |
| 4.6 | CR-HW-001 | Codex | `ACCEPTED` | WB-HW-001 `REVIEW_READY` | `CODEX_REVIEW_WB-HW-001_2026-09-01.md`；原始证据哈希与日志结论通过 |
| 4.7 | USER-HW-EVIDENCE-001 | 用户 / Codex 记录 | `ACCEPTED` | WB-HW-001 | `CODEX_USER_HW_EVIDENCE_2026-09-01.md`；4 张仓库外照片及一次 COM3 打开授权 |
| 4.8 | WB-HW-002 | WorkBuddy | `ACCEPTED` | USER-HW-EVIDENCE-001 | 初次提交 `63281b2`、修订提交 `9fe38c3`；一次受控启动采集与照片证据已验收 |
| 4.9 | CR-HW-002 | Codex | `ACCEPTED` | WB-HW-002 初次 `REVIEW_READY` | `CODEX_REVIEW_WB-HW-002_2026-09-01.md`；结论 `CHANGES_REQUIRED`，禁止新增硬件操作 |
| 4.10 | CR-HW-002-R2 | Codex | `ACCEPTED` | WB-HW-002 修订提交 `9fe38c3` | `CODEX_REVIEW_WB-HW-002_ROUND2_2026-09-01.md`；技术内容通过，保留原采集脚本未留档的过程限制 |
| 5 | WB-002 / STREAM-001 CP0 | WorkBuddy | `ACCEPTED` | WB-HW-002 `ACCEPTED` | 提交 `72b77ee`；契约收口并经 Codex 复检/修复 |
| 6 | CR-002 | Codex | `ACCEPTED` | WB-002 初次 `REVIEW_READY` | `CODEX_REVIEW_WB-002_2026-09-02.md`；结论 `CHANGES_REQUIRED` |
| 6.1 | CR-002-R2 | Codex | `ACCEPTED` | WB-002 修订提交 `6251486` | `CODEX_REVIEW_WB-002_ROUND2_2026-09-02.md`；结论 `CHANGES_REQUIRED` |
| 6.2 | CR-002-R3 / CR-WB-STREAM-001 | Codex | `ACCEPTED` | WB-STREAM-001 checkpoint `bc41ff3` | `CODEX_REVIEW_WB-STREAM-001_2026-09-02.md`；`ACCEPTED_WITH_CODEX_FIXES` |
| 6.5 | WB-STREAM-001 | WorkBuddy | `ACCEPTED` | 用户连续开发授权；远端工作流分支 | CP0 `72b77ee`、CP1 `a38dfad`、CP2 `bc41ff3` |
| 6.6 | CR-WB-STREAM-001-FIX | Codex | `ACCEPTED` | WB-STREAM-001 checkpoint 冻结 | `f021233`；身份/绑定、设备级并发、challenge 原子消费、event envelope、outbox 接口修复，pytest 28/28×5 |
| 6.7 | WB-STREAM-002 / CP0 | WorkBuddy | `READY` | `f021233`；本任务发布基线 | 本机 C++17 compile/link/run 门槛；`WB-STREAM-002_REPORT.md` |
| 6.8 | WB-STREAM-002 / CP1 | WorkBuddy | `QUEUED` | CP0 checkpoint | 纯领域 reducer、状态机与主机单测 |
| 6.9 | WB-STREAM-002 / CP2 | WorkBuddy | `QUEUED` | CP1 checkpoint | 平台无关 transactional outbox、故障注入与恢复单测 |
| 7 | WB-BRINGUP-S1 | WorkBuddy | `BACKLOG` | WB-HW-001；恢复路径；涉及刷写时需用户明确授权 | B001/B002/B003/B004/B005/B009/B013 + `BRINGUP_STAGE1_REPORT.md` |
| 8 | CR-BRINGUP-GATE | Codex | `BACKLOG` | WB-BRINGUP-S1 `REVIEW_READY` | Stage 1 复检和 GO/NO-GO |
| 9 | WB-MVP-INTERFACES / STREAM-001 CP1 | WorkBuddy | `ACCEPTED` | CP0 checkpoint | 提交 `a38dfad`，Codex 接口修复并入 `f021233` |
| 10 | WB-MVP-DOMAIN / STREAM-002 CP1 | WorkBuddy | `QUEUED` | STREAM-002 CP0 | Task、StudySession、DeviceEvent、DomainState reducer 与主机单元测试 |
| 11 | WB-MVP-MOCK / STREAM-001 CP2 | WorkBuddy | `ACCEPTED` | CP1 checkpoint；CP0 契约 | 提交 `bc41ff3`，Codex 回归修复 `f021233`；pytest 28/28×5 |
| 12 | WB-MVP-UI-HOME | WorkBuddy | `HOLD` | Mock Backend 验收 | Home 页面与 Mock Tasks |
| 13 | WB-MVP-UI-FOCUS | WorkBuddy | `HOLD` | Home 验收 | Focus 页面与本地倒计时 |
| 14 | WB-MVP-EVENTS | WorkBuddy | `HOLD` | Focus 验收 | task.start、task.complete |
| 15 | WB-MVP-QUEUE / STREAM-002 CP2 | WorkBuddy | `QUEUED` | STREAM-002 CP1 | 平台无关 outbox 核心；真实 NVS/掉电/真机适配仍 `HOLD` |

## 当前阻塞

| 阻塞 ID | 影响任务 | 证据 | 解除条件 |
| --- | --- | --- | --- |
| BLK-FLASH-AUTH-001 | 任何固件刷写、Flash 读取/擦除、分区或 OTA 操作 | 用户尚未明确授权；恢复路径、当前 boot partition 和 SKU 尚未确认 | Codex 完成只读接收复检后，另行向用户说明目标、风险和恢复方案并取得明确授权 |

## 已解除阻塞

| 阻塞 ID | 状态 | 解除证据 | 结果 |
| --- | --- | --- | --- |
| BLK-GIT-REMOTE-001 | `RESOLVED` | [`GIT_PROJECT_SETUP_2026-09-01.md`](reports/GIT_PROJECT_SETUP_2026-09-01.md) | 已创建独立私有仓库并推送 `main` 与 `workbuddy/wb-001-platform-map`；未使用 Metalio 官方 origin |
| BLK-HW-001 | `RESOLVED` | [`CODEX_DEVICE_INTAKE_2026-09-01.md`](reports/CODEX_DEVICE_INTAKE_2026-09-01.md) | 已检测 COM3/4/5/6；COM3 明确枚举为 Espressif USB JTAG/serial debug unit，可进入只读接收检查 |
| BLK-USER-EVIDENCE-001 | `RESOLVED` | [`CODEX_USER_HW_EVIDENCE_2026-09-01.md`](reports/CODEX_USER_HW_EVIDENCE_2026-09-01.md) | 用户已提供 4 张设备照片并授权下一任务打开 COM3 一次；照片未显示可辨识 SKU 标签，相关字段继续标记待确认 |

## 已知风险

- 官方分区 `ota_0` 约 9 MiB、`ota_1` 约 4 MiB，当前应用约 8.62 MiB，不能假定双槽对称 OTA 可用。
- 官方 `sdkconfig` 中 Flash mode 选择项与字符串值存在不一致迹象，WorkBuddy 只能记录，不能擅自修正。
- 真机 SKU、屏驱、PSRAM、Flash、C5、触摸等均需实机确认。
- 历史 `docs/系统检查报告_2026-09-01.md` 已被后续主机准备报告取代，不得继续据其重新安装工具链。
