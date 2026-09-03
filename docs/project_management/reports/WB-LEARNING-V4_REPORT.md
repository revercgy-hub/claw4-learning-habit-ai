# WB-LEARNING-V4 工作流报告：V4 主机侧先行（§3~§6）

## 1. 工作流状态表

| 项 | 值 |
| --- | --- |
| 任务 ID | WB-LEARNING-V4-HOST（V4 主机侧先行，阶段 1：§3~§6） |
| 分支 | `workbuddy/learning-v4-host-sync`（基线 planning-v4 `1310ca3d`） |
| 提交 | `f8513f1`（§3/§4 Fact Sync）→ `ef5d22d`（§5 tracking）→ `cbdd5ba`（§6 integration map）→ 本轮 §3 补核 + 报告提交（见 §10） |
| 日期 | 2026-09-03 |
| 范围 | 纯主机侧 + 文档侧：V4 §3 Final Fix 正式标志、§4 Project Fact Sync、§5 XiaoZhi Upstream Tracking、§6 Metalio Integration Recheck |
| 授权 | 用户三项决策：主机侧先行 / 真机未连接（本轮不做真机与 flash）/ 基线基于 planning-v4 |

## 2. 实际修改文件（相对 `1310ca3d`，6 文件 +189/−13 已核验无越界）

| 文件 | 变更 |
| --- | --- |
| `docs/HOST_MVP_ACCEPTANCE.md` | 新增 §0：12 项 Final Fix checklist 逐项代码证据，正式标志 **`HOST_MVP_FINAL_FIX=PASS`**；修正修复后数字（backend 70/70、C++ domain 28/28） |
| `docs/project_management/TASK_BOARD.md` | WB-STREAM-002 标注收口（不再活动）；`WB-LEARNING-V4-HOST` 为唯一活动工作流；旧 QUEUED 行（10/12/15/16/17）标 `SUPERSEDED` 指向 CP0~CP8 |
| `AGENTS.md` | 补 2026-09-03 状态变更段（取消 Codex 复检、V4 规划基线、V4 host 流范围）；唯一事实源次序更新（Codex 复检不再作流收口前提） |
| `docs/ARCHITECTURE.md` | 顶部状态同步横幅：Host MVP（已验证）与 Device MVP（设计层/HOLD）分离，V4 规划文档为新事实源 |
| `docs/XIAOZHI_UPSTREAM_TRACKING.md` | **新增**（V4 §5/P12）：八域分类表 KEEP_METALIO / TRACK_UPSTREAM / BACKPORT_CANDIDATE / HOLD_MIGRATION |
| `docs/METALIO_LEARNING_INTEGRATION_MAP_V4.md` | **新增**（V4 §6/P13）：11 项集成确认点 + openclaw 核实 + 参考模式 + 分区约束 |

## 3. 实现摘要

1. **§3 正式标志**：12 项 Host Final Fix（ready/pending 契约、today cache、dashboard 本地日、monotonic 恢复、auth_paused 短路、ACK 连续前缀、deadletter 持久化、UNIQUE(device_id,sequence)、PWA token 恢复、completed task_id、contract fixture、回归/E2E）逐项落到代码位置并引用既有测试证据，标志 `HOST_MVP_FINAL_FIX=PASS`（仅主机侧；真机项边界不变）。
2. **§4 Fact Sync**：TASK_BOARD / ARCHITECTURE / AGENTS / acceptance 四方同步，CP0~CP8 不再呈 queued（旧条目 SUPERSEDED），Host MVP（已验证）与 Device MVP（HOLD）分开，真实 HEAD（planning-v4 `1310ca3d`）与测试结果（FIX 后数字）成为唯一事实源。
3. **§5 tracking**：以 Metalio 官方源码与 README 为证据，对 MCP/AudioService/Protocol/WS·MQTT/ESP-SR/P4·C5/security/DeviceStateMachine 八域给出分类判定与动作；明确禁止整仓 merge、只 selective backport。
4. **§6 integration map**：11 项确认点全部给出源码证据与集成含义；核心结论——Metalio 官方固件已演进为 **LVGL Screen 应用架构**（Home 3×3 `AppEntry`/`kApps[]` + `screen_attach_lifecycle` LOAD/UNLOAD）；Learning App 建议 = Home 网格新增原生 `learning_screen`（对接已验收纯 presenter），**openclaw 已核实为云 Agent 对话 App（非学习宿主）**，其 mic/wakeword 冲突处理与 lifecycle 兜底为可复用参考模式；ESPClaw 占用 ota_1 → Learning 禁占 ota_1/改分区。

## 4. 验收标准逐项自检

| V4 要求 | 结果 |
| --- | --- |
| §3 完成 12 项并给标志 | ✅ `HOST_MVP_FINAL_FIX=PASS`（证据在 `HOST_MVP_ACCEPTANCE.md` §0） |
| §4 同步 TASK_BOARD / ARCHITECTURE / AGENTS / acceptance，Host 与 Device MVP 分开 | ✅ 四方已同步；看板活动流切换为 WB-LEARNING-V4-HOST |
| §5 新增 `docs/XIAOZHI_UPSTREAM_TRACKING.md`，只 selective backport | ✅ 已新增；铁律与流程 §5 写明 |
| §6 输出 `docs/METALIO_LEARNING_INTEGRATION_MAP_V4.md`，确认 11 项 | ✅ 已输出；11 项逐项 SOURCE_CONFIRMED / 集成含义 |
| 不触碰真机 / LVGL 实机 / NVS / Flash / 发布固件 | ✅ 零触碰（见 §8 门禁核对） |
| 不合并 main / 不 force push / 不修改官方 vendor 源码 | ✅（见 §8） |

## 5. 验证命令与结果

| 验证 | 命令 | 结果 |
| --- | --- | --- |
| 基线内容核验 | `git cat-file -t 1310ca3d`；`git diff --name-only 03383db 1310ca3d` | planning-v4 全树可达；增量 = WB-STREAM-002 代码 + 6 份 V3/V4 规划文档 |
| 变更无越界 | 隔离 index（Temp `v4sync-*.idx`）`git diff --cached` / `git diff 1310ca3d cbdd5ba --stat` | 仅 6 目标文件，+189/−13，无代码/越界文件 |
| 提交链完整性 | `git commit-tree -p` 链式生成 C1→C2→C3 | C1=`f8513f1`、C2=`ef5d22d`、C3=`cbdd5ba`，父链正确 |
| 远端推送 | `GCM_INTERACTIVE=Never git push origin cbdd5ba:refs/heads/workbuddy/learning-v4-host-sync`（代理 51846，后台长窗口） | `[new branch] cbdd5ba -> workbuddy/learning-v4-host-sync`（54s） |
| §6 源码侦查 | `grep`/`sed`/`ls` on `E:\c`（= `vendor/MetalioClaw4` @ `ca3aa3fa` 同源） | openclaw/home/screen_util/mcp_server/application 等证据已落盘（见 integration map） |
| trailing whitespace | `git diff --cached --check` | 本流文件无告警（仅历史 V4 文档 3 处既有告警，不属本流） |

> 说明：本流为纯文档/Fact Sync 变更，不涉及 C++/backend/PWA 代码改动，故不重跑代码测试；既有测试证据（C++ 28/28+、backend 70/70、PWA 30/30、E2E 5 轮 + 59s 复核）由 WB-STREAM-002 报告 §10/§11 承载并在 §0 checklist 引用。

## 6. 未解决问题、风险与 `HARDWARE_VERIFY_REQUIRED`

- **openclaw_screen 行为**（云 Agent 会话刷新、录音链路）与 Learning 是否共享后端通道等，需真机/联调确认（`HARDWARE_VERIFY_REQUIRED`）；其云 API base（`api_endpoints.h`）为 Metalio 生态，Learning 家庭后端是独立服务，两者无耦合。
- 本地 git refs 竞争（外部进程抢占）持续：本地 ref 写入即删、HEAD 无法解析；已全程用隔离 index + 裸 SHA 操作绕开，产物在远端安全。修复本地语义需在外部进程空闲时进行。
- GitHub 经代理慢（~1min 级）：push 均走后台长窗口；代理端口随环境变化（本轮 51846 存活）。
- 真机未连接：屏幕/触摸/LVGL 实机渲染、Flash 实际分区、Wi-Fi/TLS、音频 codec 全部 `HARDWARE_VERIFY_REQUIRED`；`BLK-FLASH-AUTH-001` 仍未解除。

## 7. 范围偏差

无。严格限于用户授权的 V4 §3~§6 主机侧 + 文档侧；未进入 §7~§10 实现、未申请 flash 授权、未触碰 vendor 源码与 partition/bootloader 配置。

## 8. 门禁核对

- 真机/串口/Flash/分区/Bootloader/OTA 操作：**0**
- `vendor/MetalioClaw4` / BSP / 官方固件修改：**0**
- force push / rebase / reset（远端）/ 合并 main / 越界文件：**0**
- 凭据/密钥/儿童数据/外部服务：**0**

## 9. 建议复检重点（供用户决策）

1. §0 checklist 的代码位置引用与 `HOST_MVP_FINAL_FIX=PASS` 结论是否认可；
2. Fact Sync 对 TASK_BOARD / AGENTS / ARCHITECTURE 的改写是否准确反映项目现状；
3. tracking 八域分类与 integration map 11 项判定是否有遗漏或误判（尤其 MCP/audio 两域）；
4. 是否授权进入下一段（V4 §7 Interaction Router / §8 Learning MCP Host / §9 Platform Ports——均可用主机侧 + fake 方式先行，建议与 WB-STREAM-002 同模式；§10 Metalio Adapter 起涉设备侧，建议等真机授权）。

## 10. 本报告提交

- 本报告 + §6 integration map 的 openclaw 源码级补核（UNKNOWN → SOURCE_CONFIRMED、ESPClaw/ota_1 约束、4 条可复用参考模式）作为后续提交（见状态表）。
