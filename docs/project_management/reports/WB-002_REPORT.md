# WB-002 实施报告：Claw4 学习习惯终端 MVP 架构定义

- 任务 ID：WB-002
- 状态声明：REVIEW_READY
- 分支：`workbuddy/wb-002-architecture`
- 任务包：`docs/project_management/tasks/WB-002_ARCHITECTURE.md`
- 调度基线（开始前远端任务分支 HEAD）：`69f0ef57790dbbf9924f441e0bd668009f95874b`
- 提交信息：`docs(WB-002): define MVP architecture`
- 本地/远端 HEAD：本提交自身（推送后以 `git rev-parse HEAD` 与 `git ls-remote` 为准；报告文件位于该提交内，无法自引用其 hash，详见 §5）
- 日期：2026-09-01

## 1. 实际修改文件

- `docs/ARCHITECTURE.md`（新建）
- `docs/project_management/reports/WB-002_REPORT.md`（新建）

**仅上述 2 个允许文件**。未创建源码、目录骨架、示例工程、构建配置、Docker、测试工程或任何其他文件；未修改官方源码/BSP/固件/分区；未操作任何硬件或串口。

## 2. 实现摘要

`docs/ARCHITECTURE.md` 按任务包 12 章完整覆盖：

| 章 | 内容要点 |
| --- | --- |
| 1 | MVP 唯一闭环、复用/新建/禁止边界、非目标清单、证据标签约定 |
| 2 | 系统上下文依赖方向（家长端不直连设备、设备不直连 AI Provider）、数据权威所有者与设备缓存副本表 |
| 3 | 设备六模块边界（device_services / learning_domain / sync / ui / assistant / telemetry），每模块职责/允许依赖/禁止依赖/接口/落点 |
| 4 | 设备一级状态 + 学习会话子状态分层、与官方 `device_state.h` 枚举的桥接、Task/StudySession 生命周期、5 条异常路径、7 条不变量 |
| 5 | Task / StudySession / Event envelope 契约（字段/必填性/版本/时间语义）、幂等/顺序/at-least-once/冲突策略、服务器 JSON 覆盖禁令 |
| 6 | MVP 端点表、最小 JSON 示例（注册/auth/今日任务/事件批量）、HTTPS/WSS 与证书校验、每设备凭据、状态码与重试分类 |
| 7 | 离线能力、NVS+既有 FAT 落点（不新增分区/不强依赖 microSD）、事件队列规则（顺序/确认删除/掉电恢复/容量/退避/死信）、断点续传 |
| 8 | Home/Focus/Done/Offline 四页 intent 流转、LVGL 线程归属、基于 360 MHz/32 MB PSRAM 已确认事实的预算（未实测项不写已达成） |
| 9 | 后端推荐目录与职责（ARCH_DECISION）、数据库最低模型、PWA 4 页范围、AI Gateway Provider 接口边界 |
| 10 | 儿童数据最小化、日志脱敏与禁止记录、10 项失败矩阵 |
| 11 | 8 项分阶段实施任务（全部 HOLD，等待 G2/G3，本文不自动授权编码） |
| 12 | 8 项决策表（理由/证据/影响/可逆性）、12 项 HARDWARE_VERIFY_REQUIRED/UNKNOWN 清单（链接证据文档）、5 项产品待确认 |

## 3. 验收标准逐项自检

| # | 验收标准 | 结果 |
| --- | --- | --- |
| 1 | Git diff 只有两个允许文件 | PASS（见 §4） |
| 2 | 架构覆盖 12 章节，术语与状态机前后一致 | PASS（状态机术语统一：Task 生命周期、StudySession 生命周期、异常路径、不变量；设备一级状态与子状态分层一致） |
| 3 | 设备/后端/PWA/AI 与数据所有权边界清晰，无循环依赖 | PASS（§2 依赖方向图 + §3 模块禁止依赖表；无反向依赖） |
| 4 | Task/StudySession/Event 契约足以指导单测与 Mock Backend，但未写实现代码 | PASS（§5 完整字段表 + JSON 示例；全文零代码实现） |
| 5 | 离线队列、幂等、顺序、重试和掉电恢复规则可验证 | PASS（§5.4 + §7.3 表格式规则，可直接转测试用例） |
| 6 | 无业务层直连 GPIO/LVGL/网络、无家长端直连设备、无设备直连 AI Provider | PASS（不变量 I5/I6 + 模块禁止依赖） |
| 7 | 未把 4G、屏驱、触摸、Flash 容量、分区实际布局或照片接口功能写成实机已确认 | PASS（§12.2 清单全部标 HARDWARE_VERIFY_REQUIRED/UNKNOWN/USER_EVIDENCE_REQUIRED；§8.3 预算未实测项明确标注） |
| 8 | 未引入真实儿童数据、凭据、服务账号或付费外部依赖 | PASS（JSON 示例全为 `00000000-...` 合成 uuid 与占位 token；无 API key/密钥） |
| 9 | 未修改源码、官方 BSP、固件、分区、构建或硬件状态 | PASS（仅 2 个 docs 文件新建；未运行构建/未连接设备） |
| 10 | 报告包含实际验证命令、结果、风险和 Codex 复检重点 | PASS（§4/§6/§7） |
| 11 | `git diff --check origin/main...HEAD` 无错误 | PASS（§4） |

## 4. 验证命令与结果

```powershell
# 空白检查
git diff --check
# 结果：无输出（PASS）

# 修改文件清单（相对调度基线）
git diff --name-status 69f0ef57790dbbf9924f441e0bd668009f95874b
# 结果：仅 A docs/ARCHITECTURE.md、A docs/project_management/reports/WB-002_REPORT.md

# 证据标签检查（任务包 §验证要求）
rg -n "PRODUCT_REQUIRED|SOURCE_CONFIRMED|DEVICE_LOG_CONFIRMED|ARCH_DECISION|HARDWARE_VERIFY_REQUIRED|UNKNOWN" docs/ARCHITECTURE.md
# 结果：命中 140 处，标签计数：
#   ARCH_DECISION 71 / PRODUCT_REQUIRED 29 / UNKNOWN 16 / SOURCE_CONFIRMED 11 / HARDWARE_VERIFY_REQUIRED 9 / DEVICE_LOG_CONFIRMED 4

# 关键契约词检查
rg -n "Task|StudySession|event_id|sequence|at-least-once|幂等|离线|GPIO|LVGL|HTTPS|WSS|verify=false" docs/ARCHITECTURE.md
# 结果：全部命中（GPIO 6 / HTTPS 7 / LVGL 11 / StudySession 13 / Task 17 / WSS 8 / at-least-once 5 / event_id 8 / sequence 19 / verify=false 1 / 幂等 16 / 离线 9）

# JSON 示例可解析性（9/9 PASS）
# python 脚本提取全部 ```json 代码块并 json.loads 校验

# 本地 Markdown 链接存在性（21/21 PASS）
# python 脚本解析 [text](path) 并 os.path.exists 校验（含 ../../ 相对路径）
```

**敏感表述扫描**：对 4G/屏驱/触摸/Flash 容量/分区/摄像头/SD/NVS/FAT 等关键词做越级断言扫描，剩余命中均为非目标清单、职责描述、ARCH_DECISION 设计或"未确认/禁止/保持"语境，无越级断言。

## 5. 关于提交号的说明（消除占位符）

本报告文件与 `docs/ARCHITECTURE.md` 同属一个提交（`docs(WB-002): define MVP architecture`）。Git 提交 hash 由提交内容决定，报告文本无法自引用自身 hash，故"本地/远端 HEAD"字段如实写为"本提交自身"，推送后由 Codex 按 `WORKBUDDY_GIT_SYNC.md` §七 复检入口（`git rev-parse` + `git ls-remote`）验证。本报告不含任何"提交后填"式占位符。

## 6. 未解决问题、风险与 HARDWARE_VERIFY_REQUIRED 项

### 6.1 HARDWARE_VERIFY_REQUIRED / UNKNOWN 清单

见 `docs/ARCHITECTURE.md` §12.2（12 项，均链接现有证据文档，不复制过期结论）。核心项：屏驱 SKU、触摸、Flash 容量、PSRAM 实际分配、C5 状态、实际网络通道/4G、partition/OTA 实际布局、电池/充电 IC、音频硬件、摄像头型号、SD 热插拔、外观 SKU。

### 6.2 风险

| 风险 | 说明 |
| --- | --- |
| 非对称 OTA 槽位 | ota_0=9M / ota_1=4M，当前固件 ≈8.62 MiB；未来固件更新受 `BLK-FLASH-AUTH-001` 与分区门禁约束 |
| 后端技术栈未锁定 | FastAPI/PostgreSQL 为 `ARCH_DECISION` 推荐，未选型重大替代方案（无需上报 Codex 的停止条件，因未发现现有产品事实无法决定的冲突） |
| WSS vs 轮询 | MVP 采用 REST 批量同步，家长端可见性为秒级延迟；如需实时推送再引入 WSS |
| 持久化选型 | NVS vs 既有 FAT 分区列为 `ARCH_DECISION`，实现时按队列规模与实测磨损再定，不新增分区 |

## 7. 范围偏差

无。全程未创建任务包允许列表之外的文件，未运行构建/刷写，未连接设备，未修改官方源码，未触碰 TASK_BOARD/任务包/Codex 文档，未 force push/rebase/reset。

## 8. 复检回执

```text
任务 ID：WB-002
状态声明：REVIEW_READY
分支：workbuddy/wb-002-architecture
开始提交：69f0ef57790dbbf9924f441e0bd668009f95874b
本地 HEAD：本提交自身（docs(WB-002): define MVP architecture）
远端 HEAD：以 git ls-remote --heads origin workbuddy/wb-002-architecture 为准
实际修改文件：docs/ARCHITECTURE.md, docs/project_management/reports/WB-002_REPORT.md
git diff --check：PASS（无输出）
12个必需章节：逐项 PASS（§1 范围与证据分层 / §2 系统上下文与数据所有权 / §3 设备侧模块边界 / §4 状态机与不变量 / §5 数据契约 / §6 API与同步协议边界 / §7 离线与持久化策略 / §8 UI并发与性能边界 / §9 后端PWA与AI边界 / §10 隐私安全可观测性与失败矩阵 / §11 分阶段实施图 / §12 决策与未决项）
证据标签检查：140 处命中（ARCH_DECISION 71 / PRODUCT_REQUIRED 29 / UNKNOWN 16 / SOURCE_CONFIRMED 11 / HARDWARE_VERIFY_REQUIRED 9 / DEVICE_LOG_CONFIRMED 4）
JSON 示例检查：9/9 块 json.loads PASS
Markdown 链接检查：21/21 本地链接存在（含 ../../项目总规划/AGENTS.md）
范围偏差：无
未决架构选择：后端技术栈最终确认（P1）、激励体系范围（P2）、WSS 是否进 MVP（P3）、OTA 策略（P4）、家长权限模型（P5）
HARDWARE_VERIFY_REQUIRED：见 ARCHITECTURE.md §12.2（12 项：屏驱/触摸/Flash容量/PSRAM分配/C5/网络通道/分区布局/电池/音频/摄像头/SD/外观SKU）
建议 Codex 复检重点：
  1. 数据所有权与"服务器 JSON 覆盖禁令"表述是否一致
  2. 状态机分层（官方 DeviceState 桥接 + 学习状态机）是否有矛盾转换
  3. 事件契约（event_id 幂等 + sequence 顺序 + at-least-once 收敛）是否可驱动 Mock Backend 与单测
  4. 不变量 I1~I7 是否完整、有无遗漏
  5. 离线队列容量/死信/掉电恢复规则是否可验证
  6. §8.3 性能预算是否严格区分"已确认设备事实"与"未实测目标"
  7. §12.2 清单是否全部链接现有证据、无过期结论复制
```

## 9. Codex 复检入口（供复核）

```powershell
git fetch --prune origin
git rev-parse origin/main
git rev-parse origin/workbuddy/wb-002-architecture
git diff --check origin/main...origin/workbuddy/wb-002-architecture
git diff --name-status origin/main...origin/workbuddy/wb-002-architecture
git log --oneline --decorate origin/main..origin/workbuddy/wb-002-architecture
```
