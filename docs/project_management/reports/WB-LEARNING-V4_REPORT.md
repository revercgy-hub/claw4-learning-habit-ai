# WB-LEARNING-V4 工作流报告：V4 主机侧先行（§3~§9 / P14–P16）

## 1. 工作流状态表

| 项 | 值 |
| --- | --- |
| 任务 ID | WB-LEARNING-V4-HOST（V4 主机侧先行：阶段 1 §3~§6 + 阶段 2 §7~§9/P14–P16） |
| 分支 | `workbuddy/learning-v4-host-sync`（基线 planning-v4 `1310ca3d`） |
| 阶段 1 提交 | `f8513f1`（§3/§4 Fact Sync）→ `ef5d22d`（§5 tracking）→ `cbdd5ba`（§6 integration map）→ `3e72cd8`（报告 + openclaw 补核 + 看板 6.17） |
| 阶段 2 提交 | `a862cb0`（C5 任务包）→ `35e3d45`（C6 P14 interaction）→ `1467527`（C7 P15 MCP host）→ `e10a7e1`（C8 P16 ports）→ `49cf136`/`86e0f22`（C9/C10 收口）→ `a4d35a`（C11 host funnel 集成测试 + target-ISA 实现语法）→ 本轮 C12 docs 复核补记 |
| 远端头 | `current_remote_head=9f62c12`（C20，2026-09-03 重新 fetch 核实；后续 commit 见 §12，历史 head 表述不再作为当前） |
| 日期 | 2026-09-03 |
| 范围 | 纯主机侧 + 文档侧：V4 §3 Final Fix 正式标志、§4 Fact Sync、§5 Upstream Tracking、§6 Integration Recheck（阶段 1）；V4 §7 Interaction Router、§8 Learning MCP Host、§9 Platform Ports 抽象 + fakes（阶段 2，主机可验证） |
| 授权 | 用户三项决策：主机侧先行 / 真机未连接（本轮不做真机与 flash）/ 基线基于 planning-v4；阶段 2（§7~§9）按看板候选入队、纯主机+fake 路径与 WB-STREAM-002 同模式 |

## 2. 实际修改文件（相对 `1310ca3d`，阶段 1 六文件 +189/−13 已核验无越界）

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

---

## 11. 阶段 2 收口：V4 §7 Interaction / §8 Learning MCP Host / §9 Platform Ports（P14–P16）

### 11.1 任务包与 checkpoint 状态

任务包：`docs/project_management/tasks/WB-LEARNING-V4_INTERACTION_MCP_PORTS.md`（C5 新增；基线 `3e72cd8`）。

| CP | V4 范围 | 提交 | 状态 |
| --- | --- | --- | --- |
| P14 | §7 Interaction Router / Dispatcher + STT mapper（含 P14.1 集成修正） | `35e3d45` | `CHECKPOINT_READY`（host gate 15/15 + 非回归；host funnel 6/6 见 C11） |
| P15 | §8 Learning MCP Host（8 个 `learning.*` 工具） | `1467527` | `CHECKPOINT_READY`（15/15 + 非回归；host funnel 6/6 见 C11） |
| P16 | §9 Platform Ports（8 抽象 + 确定性 fakes） | `e10a7e1` | `CHECKPOINT_READY`（8/8 + 非回归） |

### 11.2 实际修改文件（C5→C13 主要变更，相对 `3e72cd8`）

| 文件 | 变更 |
| --- | --- |
| `docs/project_management/tasks/WB-LEARNING-V4_INTERACTION_MCP_PORTS.md` | **新增**（C5）：P14–P16 任务包（调度/禁项/语义决策/必测场景/验收/范围边界） |
| `firmware/main/interaction/command.h` | **新增**：输入无关 CommandKind + CommandPayload |
| `firmware/main/interaction/stt_mapper.h/.cpp` | **新增**：纯查表短语→Command 映射（无 ASR/LLM） |
| `firmware/main/interaction/dispatcher.h/.cpp` | **新增**：CommandSink + CommandDispatcher（统一漏斗 + 完成确认门禁：Voice/MCP 的 CompleteTask 只登记 pending，`confirmPendingComplete()` 才发 `Intent::Complete`） |
| `firmware/main/ui/presenters.cpp` | P14.1：`mapFocusTap` 补 `r.task_id = v.task_id`（reducer 强制 task_id 键） |
| `firmware/main/mcp/learning_mcp_host.h/.cpp` | **新增**：LearningBackend + 8 工具宿主；查询走快照只读投影（remaining 复用 `ui::buildFocus`），变更经 dispatcher(Mcp) |
| `firmware/main/ports/*.h`（8 文件） | **新增**：Clock/Storage/Network/Ui/VoiceSession/Stt/McpRegistration/Power 抽象 Port（纯接口，无 Metalio/IDF 头） |
| `firmware/tests/fakes/fake_command_sink.h` 等 3 个 fakes | **新增**：确定性 fake（sink / learning backend / 8 ports） |
| `firmware/tests/unit/interaction|mcp|ports/*.cpp`（4 测试） | **新增**：P14 dispatcher 15 case / P15 mcp host 15 case / P16 ports 8 case / **host funnel 6 case（C11：learning.* 经 dispatcher → 真实 AppCoordinator + reducer + outbox 全链）** |
| `firmware/tests/unit/ui/presenter_tests.cpp` | P14.1：两处 mapFocusTap case 增加 task_id 断言 |
| `tools/dev/verify-host-cpp-tests.ps1` | implRoots 增 interaction/mcp/ports；新增 §4b3 禁止 include 扫描（lvgl/esp_/freertos/driver/bsp/wifi/nvs/hal/metalio） |
| `tools/dev/verify-interface-contracts.ps1` | C11：新增 §2.5 对 interaction/mcp 实现源做 riscv32 target ISA `-fsyntax-only`（implsrcs 3/3 PASS） |
| `.gitignore` | C13：补 `**/node_modules/`、`**/dist/`（重建本地 frontend 工具链所需，防误提交） |

### 11.3 实现摘要

1. **P14 统一交互漏斗**：所有 Touch/STT/MCP 变更汇入同一 `CommandDispatcher`（V4 §7）；完成门禁内建——Touch 可直发 `Intent::Complete` 并消费 pending AI 请求，Voice/MCP 只登记待确认（重复请求不重复 emit），物理确认后才真正完成；4 个 Query 载荷不产生任何 sink 调用（只读）。
2. **P14.1 集成修正（发现并修复）**：`mapFocusTap` 只填 `session_id`，而 reducer 入口强制 `task_id` 存在（`reducer.cpp` `if (!request.task_id) return TaskNotFound`），直连 coordinator 会恒被拒。补 `task_id`（FocusView 已携带），原 28 个 presenter case 无回归。
3. **P15 MCP Host**：`learning.get_today_tasks/current_task/remaining_time/today_progress` 为只读投影；`start/pause/resume/request_complete_task` 经 dispatcher(Mcp) 进入域。`request_complete_task` 响应 `{"confirmation":"pending"}`，**AI 绝不直接 Complete**（V4 §8）。remaining 复用 `buildFocus` 数学避免公式漂移；today_progress 规则（排除 Pending、含 skipped 分母）写入代码注释并测例锁定。
4. **P16 Platform Ports**：8 个抽象接口 + 确定性 fake；Learning Domain/interaction/mcp/ports 零硬件头（4b3 扫描门禁证明）。UiPort 只表达语义动作与确认回调，LVGL 归 P17；NetworkPort 只留连接状态 + JSON POST 边界。
5. **C11 host funnel 集成（补强验收）**：新增 `host_funnel_tests.cpp`，让 `learning.*` MCP 工具经 CommandDispatcher 走**真实 AppCoordinator（domain reducer + transactional outbox + FakeDisk 持久化）**，替代脚本化 fake sink/backend——验证了 start→InProgress/2 事件提交、pause/resume 状态机迁移、`request_complete_task` 不触碰 domain、物理确认后 Completed 且 active_session 清空（Done 语义）、busy/Pending 任务的 domain 拒绝路径。

### 11.4 验收标准逐项自检

| 任务包要求 | 结果 |
| --- | --- |
| P14 dispatcher 门禁与映射 | ✅ 18/18（新增：Voice 非完成变更直发、Mcp Skip/Pause、pending cancel→重请求循环），含 Voice/MCP 完成零 emit、confirm 单发、Query 零变更 |
| P15 8 工具 + AI 禁止直接 Complete | ✅ 16/16（新增：无会话无 task_id → missing_arg 错误）；`request_complete_task` 后 sink 零 Complete 调用，confirm 后恰一次 |
| P16 8 接口 + fakes 往返 | ✅ 8/8（含失败注入、重复注册拒绝、回调触发） |
| P14/P15 对真实 domain 的 host funnel | ✅ 8/8（C11 6 case + C15 补：Completed 任务 start 幂等且零重复事件、Skipped 任务 start 拒绝） |
| 跨语言 host MVP E2E 复跑（阶段 2 后，C13） | ✅ `E2E RESULT: PASS`：C++ host gate exit=0（141 case）+ backend pytest **70 passed** + PWA typecheck/vitest **30/30**/build PASS |
| 新增目录无硬件/OS/Metalio include | ✅ §4b3 扫描 PASS |
| 全量非回归 | ✅ host gate 8/8 二进制 RUN PASS，总计 141 case 0 失败；P4 交叉契约 exit=0（headers 33/33 + implsrcs 3/3 + contract 1/1） |
| 不触碰真机 / 不改 domain/sync 语义 / 无越界 | ✅（见 11.8） |

### 11.5 验证命令与结果

| 验证 | 命令 | 结果 |
| --- | --- | --- |
| 全量 host 门槛 | `tools/dev/verify-host-cpp-tests.ps1 -CompilerPath w64devkit-2.9.1\bin\g++.exe -CrossCompilerPath riscv32-esp-elf-g++.exe` | `RESULT: NATIVE CPP TEST GATE PASS`；smoke compile/link/run PASS；4b/4b2/4b3 扫描 PASS；unit 8/8（coordinator 20 + domain 28 + dispatcher 18 + mcp 16 + host_funnel 8 + ports 8 + outbox 21 + presenter 28 = **147 cases, 0 failures**）；interface exit=0（headers 33/33 + implsrcs 3/3 + contract 1/1，riscv32 target ISA `-fsyntax-only`） |
| 跨语言 E2E 编排 | `tools/dev/run-host-mvp-e2e.ps1 -LogDir out\e2e_stage2`（backend/.venv 重建 + `npm ci` 313 包，均 git-ignored） | `E2E RESULT: PASS`（16:43:47→16:45:54）；C++ host gate exit=0、backend **70 passed**、PWA typecheck PASS + vitest **30/30** + `built in 779ms`；log `out/e2e_stage2/e2e_result.txt` |
| 远端冷克隆自足性（C17 后，C18 记录） | `git clone --depth 1 --branch workbuddy/learning-v4-host-sync` 到系统 Temp，在克隆内独立跑 `verify-host-cpp-tests.ps1`（不携带任何本地未提交文件） | `NATIVE CPP TEST GATE PASS`：8/8 单测二进制、**147 case 0 fail**、4b3 扫描 PASS；契约 headers 33/33 + implsrcs 3/3 + contract 1/1 —— 证明远端分支自足完整（克隆头 = C17 `d0466bb`，验证后已清理） |
| 变更无越界 | 隔离 index + `git diff <parent> <tree> --stat` | C5 +1 文件；C6 10 文件 +746/−5；C7 4 文件 +684；C8 10 文件 +580；均仅目标路径 |
| 提交链完整性 | `git cat-file -p` 逐提交核验 parent | `3e72cd8 → a862cb0 → 35e3d45 → 1467527 → e10a7e1` 父链正确 |
| 远端推送 | 代理 51846 后台长窗口普通 push | 每 CP `old..new -> workbuddy/learning-v4-host-sync`（8s~54s，快进） |
| trailing whitespace | `git diff --check` | 新增文件无告警 |

### 11.6 阶段 2 未解决问题、风险与边界（`HARDWARE_VERIFY_REQUIRED` 保持）

- `mapFocusTap` 修复仅为任务键补全，未触及 reducer/presenter 其他语义；遗留的「voice 完成请求→UI 确认弹窗→Touch 确认」设备链路依赖 P17 适配与真机 UI，属后续授权范围。
- STT mapper 为 MVP 固定短语表（无 ASR）：真实语音识别、唤醒词互斥、VoiceSession 与 mic/I2S 排他均 `HARDWARE_VERIFY_REQUIRED`。
- NetworkPort 的 `postJson`/真实 HTTP·TLS、StoragePort 的 NVS/SD 后端、McpRegistrationPort 桥接 Metalio `AddTool`：P17 设备适配 + 真机网络任务，未实现。
- 本地 git refs 竞争（外部进程抢占）依旧；本次继续用隔离 index + 裸 SHA push，远端分支为唯一可靠状态源（现头 = 本收口提交）。
- 真机未连接：LVGL 渲染、Flash 分区、Wi-Fi/TLS、音频 codec 全部保持 `HARDWARE_VERIFY_REQUIRED`；`BLK-FLASH-AUTH-001` 未解除。

### 11.7 范围偏差

无。阶段 2 严格限于任务包 P14–P16 允许路径（interaction/mcp/ports 新增 + fakes/tests/harness + P14.1 有界 presenter 修正 + 任务包/报告/看板文档）；未进入 §10 Metalio Adapter、未接真实 MCP transport、未触碰 vendor 源码与 domain/sync 语义。

### 11.8 建议用户决策点

1. 阶段 2 三个 checkpoint（P14/P15/P16）是否 ACCEPT；
2. 是否授权下一步：§10 Metalio Adapter（`integration/metalio_claw4/` thin adapter，起涉设备侧）与 P17 设备边界——建议等真机连接授权后再入队；
3. host 门槛新增的 4b3 include 扫描与 P14.1 presenter 修正是否认可（有界改动，28 case 无回归）。

### 11.9 独立复核记录（C15 复核轮）

- 复核方式：以 reviewer 视角逐提交读取最终提交版（`dispatcher.cpp` / `learning_mcp_host.cpp` / `stt_mapper.*` / 8 个 ports 头），对照任务包语义决策与必测场景逐项核对。
- 结论：**未发现缺陷**。以下行为经确认为「域权威设计」内预期并记录（供 P17 设备适配参考）：① 重复 Voice/MCP 完成请求覆盖 pending（不重复 emit，最新请求胜出）；② Touch 直接 CompleteTask 消费并清除 pending AI 请求；③ start 已运行/Completed/Skipped/Pending 任务由 reducer 拒绝或幂等，MCP host 原样透传 domain 结论；④ 变更工具无 task_id 且有活动会话时解析到会话任务（domain 仍按状态机裁决）。
- 复核驱动补充用例（C15，全部通过）：dispatcher 15→18（Voice 非完成变更直发 / Mcp Skip·Pause / pending cancel→重请求循环）；mcp host 15→16（无会话无 task_id → `missing_arg:task_id`）；host funnel 6→8（Completed 任务 start **幂等且零重复 outbox 事件** / Skipped 任务 start 拒绝）。
- 复核后全量：host gate 8/8 二进制、**147 case 0 失败**；契约 implsrcs 3/3 + headers 33/33；跨语言 E2E PASS（backend 70 + PWA 30/build）；`git diff --check` clean。

## 12. WB-V4-HOST-CORRECTION（阶段 A/B checkpoint，2026-09-03）

| 项 | 值 |
| --- | --- |
| Task ID | WB-V4-HOST-CORRECTION（FIX-V4-01~03，来自 `WB-LEARNING-V4-NEXT` 验收报告 阶段 A/B） |
| Base SHA | `6b787d1`（重新 fetch 核实的远端头） |
| New SHA | C19 `bba9356`（代码修正）→ C20 `9f62c12`（验收重声明） |
| Changed Files | C19：`application/coordinator.{h,cpp}`、`sync/outbox_core.cpp`、`tests/fakes/fake_outbox_storage.h`、`tests/unit/application/coordinator_tests.cpp`、`tests/unit/interaction/host_funnel_tests.cpp`；C20：`docs/HOST_MVP_ACCEPTANCE.md`（§0.1） |
| Implementation Summary | FIX-V4-01 auth_paused 后 runSyncOnce 入口短路（不 send/不 reauth/pending 不变/返 PausedAuth）+ `resetAuthPause()` 恢复 + `authPaused()`；FIX-V4-02 `mergeTodayTasks` → 权威快照 `applyTodaySnapshot`（server 缺席非 active 移除、live 会话任务保留且 status 不重置、version 不倒退、新任务追加、经 outbox 重启复原）；FIX-V4-03 `markDeadLetter()!=Committed` → StorageError 中止整个 ACK cleanup |
| Tests | coordinator_tests 20→25（+快照 7 项改写/新增、auth 停发 1、deadletter 失败 1；修正 2 项旧锁定用例）；host_funnel 改全量快照语义后 8/8；fakes 增 `fail_next_mark_deadletter` |
| Case Counts | C++ host gate 8/8 二进制、**152 case 0 failures**（coordinator 25 / domain 28 / dispatcher 18 / host_funnel 8 / mcp 16 / ports 8 / outbox 21 / presenter 28） |
| Cross-layer | backend pytest **70 passed**；PWA typecheck PASS + vitest **30/30** + build PASS；E2E `E2E RESULT: PASS`；`git diff --check` clean；cold clone（head `bba9356`）host gate PASS |
| Known Risks | 无新增；沿用 §11.6（真机/refs 竞争/scratch 环境） |
| Hardware Verify Required | 无（纯主机契约修正）；真机项保持 §11.6/§3 |
| Scope Deviations | 无：按 `WB-LEARNING-V4-NEXT` 阶段 A/B 执行，未触碰 vendor/设备 |
| Next Checkpoint | 阶段 C 事实同步（本文 + TASK_BOARD + AGENTS + ARCHITECTURE + acceptance）→ 阶段 D P17 策略修正 → 阶段 E/F P17+P18（build only，停 `FLASH_AUTH_REQUIRED`） |

### 12.1 契约修正说明（防再犯）
- 测试不得把错误行为固化为 PASS：`merge_empty_tasks_ok`（旧：`server=[]` 保留任务）与「以 UI 透传宣称 transport 停发」两处旧锁定已被改/弃，改为按契约的直接断言（send 计数、pending 保留、ACK 不推进等）。
- 代码行为先满足契约，测试证明契约（`HOST_MVP_ACCEPTANCE.md` §0.1 按此记录）。

## 13. P17a — LearningApp glue（WB-LEARNING-V4-NEXT 阶段 E checkpoint 1，2026-09-03）

| 项 | 值 |
| --- | --- |
| Task ID | P17a — App/Coordinator Glue |
| Base SHA | `911e5c2`（阶段 A–D 收口头） |
| New SHA | C23 `31cb6b8` |
| Changed Files | 新增 `integration/metalio_claw4/host_glue/coordinator_glue.h`（ContextBuilder / CommandSinkGlue / BackendGlue）、`host_glue/learning_app.h`（LearningApp 生命周期 + 统一漏斗/coordinator/快照/同步透出）；新增 `firmware/tests/unit/metalio/learning_app_glue_tests.cpp`；新增 `integration/metalio_claw4/integration_manifest.md`（政策 §2B 登记表，0 条改动）；harness：implRoots/include/4b3 扫描纳入 host_glue |
| Implementation Summary | 生产级 glue 替代测试内联 CoordSink/CoordBackend：LearningApp 组合 DomainReducer+AppCoordinator+ContextBuilder(ClockPort)+CommandSinkGlue+CommandDispatcher+BackendGlue+LearningMcpHost，device shell 直接复用；identity/id 工厂可注入（设备后续用真 UUID/时钟）；生命周期 start/stop/running；透出 applyTodaySnapshot/runSyncOnce/authPaused/resetAuthPause/pendingCount |
| Tests | `learning_app_glue_tests` 4 case：lifecycle+start 提交（pending 2）、权威快照+跨实例重启一致、MCP request_complete 门禁→confirm 完成、auth pause 透出 |
| Case Counts | host gate **9/9 二进制、156 case 0 failures**（coordinator25/domain28/dispatcher18/funnel8/mcp16/glue4/ports8/outbox21/presenter28）；backend/PWA 未触碰 |
| Known Risks | P17b–P17d 为 Metalio/LVGL 侧（E://c 构建树内编写，禁改动官方底层，补丁逐条入 manifest）；E://c/E://i/E://b 镜像在位（`xiaozhi.bin` 8.62 MiB 基线） |
| Hardware Verify Required | 无（纯 host glue）；UI/Home 注册待 P17b/c |
| Scope Deviations | 无 |
| Next Checkpoint | P17b UI Adapter（LearningScreen/Home/Focus/Paused/Done/Offline/Recovery/ConfirmOverlay，懒加载）→ P17c Home 注册（`home_screen.cc/kApps[]` 极小补丁+manifest）→ P17d Clock/Ui/CommandSink/LearningBackend 最小 Port 适配 → P18 `idf.py build` + 尺寸 gate → 停 `FLASH_AUTH_REQUIRED` |

## 14. P18 — Learning L0 App Shell BUILD（WB-LEARNING-V4-NEXT 阶段 F；P17b–P17d 最小实现并入）

| 项 | 值 |
| --- | --- |
| Task ID | P18 Learning App Shell — BUILD ONLY（Home→Learning→Mock Task→Start→Back） |
| Base / New | 阶段 E 头（见 §13）；本 checkpoint 产物 `E:/b/xiaozhi.bin`（构建树，不提交 git） |
| Changed（Metalio 官方文件，登记于 `integration_manifest.md`） | `home_screen.cc`（include+Launch+lifecycle cb+kApps 行）、`main/CMakeLists.txt`（SOURCES +1 行）——两条均 **APPLIED**、单文件可回滚 |
| Changed（repo 新增） | `integration/metalio_claw4/device/learning_screen/{learning_screen.h,learning_screen.cc,README.md}`（权威副本；构建时同步至 `E:/c/main/display/screen/learning_screen/`） |
| Implementation Summary | L0 screen：LVGL 最小屏（标题/Mock 任务卡/开始按钮/状态行/返回按钮→`HomeScreen::Create()`，参照计算器返回模式）；`LaunchLearning`/`learning_lifecycle_cb` 沿用 LaunchVibrate 模式；**懒加载**（进屏才 Create）；无 backend/NVS/voice/STT/MCP/AI，状态为 UI 本地 mock |
| Build | `idf.py -C E:/c -B E:/b build`（IDF 5.5.4 / esp32p4 / 原 sdkconfig）；**exit=0**，`Project build complete`；`learning_screen.cc.obj` 已编译入 `main` |
| Size Gate | baseline（同 tuned sdkconfig、无 Learning）= **9,023,424 B (0x89AFC0)**；Learning L0 = **9,025,520 B (0x89B7F0)**；**delta +2,096 B**；app 地址 0x200000；ota_0 余量 = 0x900000 − 0x89B7F0 = **0x64810 ≈ 411,664 B（≈0.39 MiB）**；ota_1 overflow 为基线已知（4M 槽，禁占）；最大段（elf）：.flash.rodata ≈ 4.89 MB、.flash_rodata_dummy ≈ 3.80 MB、.flash.text ≈ 3.76 MB |
| Diff Gates | **sdkconfig diff = 0**（与 vendor 调优基线逐字节一致，未改；构建环境曾因 IDF 无 git 重配置漂移过，已恢复并复核）；partition CSV/bootloader 源零改动；bootloader 策略 0 变化；无 eFuse/Secure Boot/Flash Encryption 触碰 |
| Warnings | 基线既有告警（pkg-config 缺失、git 版本提示、`llgok__cpp_bus_driver` SRC_DIRS 空目录提示）；无新增 Learning 相关告警 |
| Host Regression | host gate **9/9、156 case 0 fail** 保持（本 checkpoint 仅设备侧代码，host 面零改动） |
| Known Risks / Notes | ① **sdkconfig 保护**：用户明确「非必要勿改 sdkconfig（已按 Claw4 硬件调优）；自定义用 sdkconfig.defaults / 板级 config.json 的 sdkconfig_append 增量覆盖」——本 checkpoint 未做任何自定义，恢复并复核 diff=0；② 构建环境 recipe：剥离 `CODEBUDDY_SAFE_DELETE_*`/`PYTHONPATH`（防 safe-delete 拦截资产生成）、PATH 前置 venv/cmake/ninja/riscv32、`ESP_IDF_VERSION=5.5`+`IDF_VERSION=5.5.4`、`E:/i/version.txt=v5.5.4`（IDF 非 git 时让 idf_tag 变体生效）——已记入 `.workbuddy/memory`；③ E:/c/E:/i/E:/b 均为本地镜像/构建树，不入 repo git |
| Hardware Verify Required | 全部：屏幕/触摸渲染、Home 入口点按、返回、Start 交互、串口（COM7）——`FLASH_AUTH_REQUIRED` |
| Scope Deviations | 无（任务书授权到代码+编译；未刷写、未动 partition/ota_1/eFuse 等） |
| Next | **停 `FLASH_AUTH_REQUIRED`**：等待用户对真机调试批次授权的明确确认后，方可 `idf.py -p COM7 flash`（仅 ota_0 app-flash）+ monitor |

### 14.1 上机前提（批次授权语句，待用户确认）
> 「授权一个 Claw4 V4 真机调试批次：仅允许 **ota_0 application app-flash + monitor**；允许在该批次内重复 build → app-flash → monitor → fix → app-flash。禁止 erase_flash、bootloader、partition、ota_1、C5 firmware、eFuse、Secure Boot、Flash Encryption。」

### 14.2 真机首刷验证（批次内，2026-09-03）
- 授权批次确认（用户，仅 ota_0 application app-flash + monitor）→ 执行 `esptool --chip esp32p4 -p COM7 write_flash 0x200000 xiaozhi.bin`：**Wrote 9,025,520 B，Hash verified**，hard reset。
- `idf.py -C E:/c -B E:/b -p COM7 monitor`（约 90 s）：设备正常启动 L0（app 2.0.51，ELF SHA 816cd529d…，ESP-IDF v5.5.4）；Home 渲染（icon paths built for theme1）；**观测到 `HomeScreen/learning_screen load → LearningScreen load` 与随后 `unload`（Learning 进入与返回）**；系统监控健康（CPU 1–3%、内存余量 ~246 KB / 历史最低 189 KB、电池 86% @4081 mV、芯片 42 ℃）；无 panic / Guru / 复位循环。
- 基线告警沿用（gpio isr already installed、i2s bclk 提示等均非本改动引入）；monitor 停止时的 `ClearCommError` 为主机侧释放串口提示。
- 结论：**L0 上机可运行**；Home 入口→Learning 屏往返成立。Start 按钮交互与图标显示需用户在屏幕侧确认（视觉项）。

### 14.3 Start 交互修复与复测（2026-09-03，用户反馈驱动）
- 用户反馈：「看到学习入口，点击开始没有反应，返回正常」。
- 根因（代码审计，对照官方屏实现）：L0 屏背景为深色 0x0E1116，但标题/任务卡/状态行/按钮文字均未显式设文字色 → 继承主题默认深色 → 状态行「未开始/专注中/已完成」深字深底**不可见**；按钮文字落在按钮浅底上可见、可点，点击事件实际一直在触发（返回切屏直观所以「正常」）。
- 修复（`learning_screen.cc`，设备构建树 + repo 权威副本同步）：
  1. 所有 label 显式 `lv_color_white()`（与官方 vibrate 深底白字一致）；
  2. 按钮由 `lv_obj_create` 改为官方同款 `lv_button_create`，深色底+白字+圆角+**按下高亮反馈**（0x1F2733 → pressed 0x34415A）；
  3. OnStartClick 保留日志行（`button start -> running/done (mock)`）供 monitor 区分事件层/显示层。
- 验证：增量 build exit=0；`xiaozhi.bin` 9,025,520 → **9,026,000 B（+480 B）**；重刷 ota_0（Hash verified）；monitor 捕获用户实测：`button start -> running`（t=10214）→ `button start -> done`（t=11191），load/unload 往返完整，无 panic。**交互 PASS**。
- 期间设备 USB 短暂掉线（COM7 变 CM_PROB_PHANTOM）→ 用户重插后恢复，重刷成功。

## 15. WB-LEARNING-V4-NEXT 全任务收口（2026-09-03）

| 阶段 | 内容 | 提交 | 状态 |
| --- | --- | --- | --- |
| A | FIX-V4-01~03（auth 短路 / 权威快照 / deadletter 传播） | C19 `bba9356` | ✅ |
| B | 全量重验 + `HOST_MVP_FINAL_FIX_V4=PASS` 重声明 | C20 `9f62c12` | ✅（152 case / backend 70 / PWA 30 / E2E / cold clone） |
| C | 事实同步（current_remote_head 制） | C21 `8e0fdb3` | ✅ |
| D | P17 集成策略（底层少动上层深做 + 严格禁止清单 + manifest 制） | C22 `911e5c2` | ✅ |
| E | P17a LearningApp glue（host 可测）+ glue 测试 + harness | C23 `31cb6b8` / C24 docs | ✅（host gate 9/9、156 case） |
| F | P18 Learning L0 App Shell BUILD ONLY → 首刷 → Start 交互修复 | C25 `f8b801b` / C26 / C27 `117c31b` | ✅（见 §14/§14.2/§14.3） |

- 任务书授权范围（阶段 A–F，纯主机修正 + P17/P18 代码与编译 + 用户批次的 ota_0 app-flash/monitor）**全部完成**；本收口不含任何 erase/partition/ota_1/C5 触碰。
- 端到端状态：host gate 9/9、**156 case 0 fail**；L0 固件 9,026,000 B（delta vs 基线 +2,096 B +480 B 交互修复）；sdkconfig diff=0；真机首刷 + Start 交互日志级 PASS；无 panic。
- 剩余 `HARDWARE_VERIFY_REQUIRED`（非本任务书范围）：屏幕视觉观感确认（用户屏侧）、NVS/Wi-Fi/TLS/音频/真实 backend（L1+，需新授权）。

## 16. WB-LEARNING-V4-L1a — 设备核心编入（2026-09-03）

| 项 | 值 |
| --- | --- |
| Task ID | L1a（任务包 `tasks/WB-LEARNING-V4_L1_APP_BASICS.md`，看板 6.23） |
| Base / New | C29 `d34c4b7` → C30 |
| Device core | `integration/metalio_claw4/device/core/`：`outbox_codec.{h,cpp}`（OutboxState↔blob 文本 codec，纯 C++17，magic+长度前缀行+字段转义+map 分隔，16 KiB 上限）；`demo_seed.h`（首启演示今日任务 2 条，L2 server 快照替换） |
| Device ports（P17d 最小） | `device/ports/learning_clock.{h,cpp}`（ClockPort←esp_timer：monotonicMs/epochSeconds(boot 相对)/isTimeSynced=false）；`device/ports/nvs_outbox_storage.{h,cpp}`（OutboxStorage←NVS namespace `learning`/key `st`，codec blob，commit 失败不落盘=旧快照保留；语义镜像 host fake；`eraseAll/hasState` 供 seed） |
| 镜像/编入 | repo 权威 → `E:/c/main/learning/`（fw 核心 7 组件目录 + metalio_claw4/{host_glue,device}）；manifest #3：`main/CMakeLists.txt` INCLUDE_DIRS +learning、SOURCES +10 行（reducer/outbox_core/coordinator/dispatcher/stt_mapper/learning_mcp_host/presenters/outbox_codec/learning_clock/nvs_outbox_storage） |
| Host tests | `firmware/tests/unit/metalio/outbox_codec_tests.cpp`：full roundtrip（含中文/转义字符、会话、deadletter）/empty/idempotent(bit-identical)/corruption 负例全 PASS；harness implRoots + device/core（NVS/clock 含 IDF 头不 host 编译） |
| Host gate | **10/10 二进制 PASS**（+outbox_codec） |
| Device build | `idf.py build` **exit=0（8m25s）**；`xiaozhi.bin`=9,026,080 B（+80 vs L0——屏仍 L0 mock，新核心未被引用故增量极小）；learning obj 全部生成；**sdkconfig diff=0**；partition/ota 零改动 |
| Known Risks | 屏仍为 L0 UI mock（真实状态机接线在 L1b）；NVS 单 key blob 上限 16 KiB（>200 pending 极端时 commit 返 StorageError→coordinator Backoff，不丢旧态，语义安全） |
| Hardware Verify | 无（本轮纯编入/build）；NVS 真机读写行为待 L1c 上机 |
| Next | **L1b**：Learning 屏接真实 LearningApp（NVS 存储 + clock），seed 首启注入，Start/Pause/Resume/Complete(Touch 门禁) 真实驱动，1 s LVGL timer 渲染 state 投影；重启恢复 |

## 17. WB-LEARNING-V4-L1b — Learning 屏接真实状态机（2026-09-04）

| 项 | 值 |
| --- | --- |
| Task ID | L1b（任务包 L1，看板 6.23） |
| Base / New | C30 `187b544` → C31 |
| Changed（repo） | `host_glue/learning_app.h`（新增 `setIdentity/setEventIdFactory/setSessionIdFactory` 透传——设备用 NVS 持久计数器，重启后事件/会话 id 不碰撞）；新增 `device/app/learning_runtime.{h,cpp}`（LearningRuntime 单例：NVS storage+esp_timer clock+LearningApp 组装、首启 `hasState()==false` 时 seed `DemoTodaySnapshot()`、`evseq/sessseq` NVS 单调计数器 id 工厂）；`device/learning_screen/learning_screen.cc` 重写（L1 真实渲染）；`device/learning_screen/README.md`（L1 tree/同步命令）；manifest #3 → APPLIED（SOURCES +learning_runtime.cpp） |
| 屏实现要点 | UI 零业务状态（移除 L0 `s_ui.running/done`）：1 s LVGL timer 把 `state()` 投影为任务行/会话面板/按钮；唯一变更路径 `dispatcher.dispatch(CommandSource::Touch,…)`（P14 门禁，Touch Complete 直发）；按钮语义：无活动→开始第一项 Ready / Running→暂停 / Paused→继续；完成仅活动会话可见；专注秒 Running 时按单调时钟只读投影；header 返回按钮；unload 删 timer |
| Device build | `idf.py build` exit=0（3m59s，第 4 次；前 3 次：configure 截断/`LearningApp` 未声明→`auto&`/`-Werror=format-truncation`→Mmss 改纯字符串）；`xiaozhi.bin`=**9,169,056 B**（+142,976 vs L1a；核心被引用后的真实代码量）；符号 LearningRuntime/LearningScreen/NvsOutboxStorage 在 elf；ota_0 余量 ≈0x41EE0≈270 KB；sdkconfig diff=0 |
| Host regression | host gate **10/10 PASS**（learning_app.h setter 不破坏 glue 测试）；learning_runtime/屏为设备源不 host 编译 |
| Known Risks | NVS 首启 seed 后若想重演需 erase（禁 erase 权限内不做；L1c 首刷 NVS 无旧状态天然 seed）；demo child/device id（L2 provisioning 替换） |
| Hardware Verify | L1c 上机（批次 ota_0 app-flash+monitor）：seed→Start→暂停→继续→完成→返回→**重启后状态/任务保持**（NVS 恢复）+ 用户屏侧确认 |
| Next | **L1c 上机**（等待用户安排设备在线与操作） |

### 17.1 L1c 上机现场（2026-09-04）与阻塞缺陷 — 详见 `WB-LEARNING-V4-L1_HANDOFF.md`
- 首刷 boot/seed/Start(Accepted,NVS save OK) 均 PASS；**Pause/Complete（Touch）→ intent=3 PersistFailed，Save 从未执行**；重启恢复正常。历史初判“改 esp_random 后 duplicate 排除”已由 §17.3 **SUPERSEDED**；问题实际仍是 event_id 重复，但根在 nano-newlib 的 `%llx` 格式化。
- 调试资产已落地（learning_runtime random-id + SELFTEST 链 + NVS 操作日志）；设备现为干净 seed 态。
- 状态：**BLOCKED（问题交接，换模型/工程师接手）**。

### 17.2 独立记录（非 Learning 引入）
- 官方 esp_netif Wi-Fi 停止崩溃（`esp_netif_stop_api` Load access fault + SW reboot）为基线行为，会打断长 monitor 会话。
- 设备 USB 偶发掉线（CM_PROB_PHANTOM）；monitor 须在设备在线时 attach。

### 17.3 L1c PersistFailed 根因修复候选（Codex，2026-09-04）

| 项 | 值 |
| --- | --- |
| Base / Branch / Code SHA | 诊断冻结头 `e9141c8`；已整合 WorkBuddy C33 `7ee686d` / `codex/wb-learning-v4-l1-persist-fix` / fix `0e53265` |
| Root Cause | `CONFIG_NEWLIB_NANO_FORMAT=y` 下 `%llx` 不受支持；`esp_random()` 的 64-bit 组合虽变化，`snprintf("%llx", …)` 却生成恒定文字 ID。Start 的两个 draft 因缺少批内重复校验而同时入队，后续 Pause/Complete 命中 existing duplicate，恰在 commit 前失败。真机日志中的 `tasks=zu`、`lastAcked=ld` 为同一 formatter 限制的直接旁证。 |
| Product Fix | 新增纯 C++ `formatEntropyId(prefix, high32, low32)` 手工十六进制编码，不再依赖 64-bit printf；`OutboxCore` 增加 transition 内重复 ID 原子拒绝；设备日志改用 nano 安全格式。 |
| Regression | 整合 C33 `restart_recovery_tests` 的完整 Start→重启→Pause→Resume→Complete；LearningApp 新增 codec 存储的“Start→销毁实例→重启恢复旧 pending→Pause”用例；outbox 新增同批重复拒绝；codec 新增 entropy 格式用例。关键套件：restart_recovery PASS、LearningApp **5/5**、outbox **22/22**、codec PASS；全量主机 **11/11 binaries PASS**，接口 cross-check exit=0。 |
| Device Build | `idf.py -C E:/c -B E:/b build` exit=0；`xiaozhi.bin` **9,175,856 B**；SHA-256 `7f96c501903b25f2d3c37307e22cf6d7490b1bb987e854b0ef5cc963f974e6f3`；ota_0 容量 PASS，ota_1 仍溢出且未触碰。 |
| Scope | repo 权威与 `E:/c` 构建镜像逐文件一致；未修改 sdkconfig/partition/bootloader/ota_1/C5/eFuse；未写 Flash。 |
| Current Status | **FIX_BUILT / DEVICE_RETEST_REQUIRED**：设备保持干净 seed；下一步只需 ota_0 app-flash + monitor，确认 SELFTEST 四步、人工触摸与重启恢复。 |
