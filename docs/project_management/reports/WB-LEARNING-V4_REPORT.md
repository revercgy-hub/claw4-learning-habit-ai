# WB-LEARNING-V4 工作流报告：V4 主机侧先行（§3~§9 / P14–P16）

## 1. 工作流状态表

| 项 | 值 |
| --- | --- |
| 任务 ID | WB-LEARNING-V4-HOST（V4 主机侧先行：阶段 1 §3~§6 + 阶段 2 §7~§9/P14–P16） |
| 分支 | `workbuddy/learning-v4-host-sync`（基线 planning-v4 `1310ca3d`） |
| 阶段 1 提交 | `f8513f1`（§3/§4 Fact Sync）→ `ef5d22d`（§5 tracking）→ `cbdd5ba`（§6 integration map）→ `3e72cd8`（报告 + openclaw 补核 + 看板 6.17） |
| 阶段 2 提交 | `a862cb0`（C5 任务包）→ `35e3d45`（C6 P14 interaction）→ `1467527`（C7 P15 MCP host）→ `e10a7e1`（C8 P16 ports）→ `49cf136`/`86e0f22`（C9/C10 收口）→ `a4d35a`（C11 host funnel 集成测试 + target-ISA 实现语法）→ 本轮 C12 docs 复核补记 |
| 远端头 | `a4d35a…` 后接本收口提交（普通快进，无 force） |
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

### 11.2 实际修改文件（C5→C8，相对 `3e72cd8`）

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

### 11.3 实现摘要

1. **P14 统一交互漏斗**：所有 Touch/STT/MCP 变更汇入同一 `CommandDispatcher`（V4 §7）；完成门禁内建——Touch 可直发 `Intent::Complete` 并消费 pending AI 请求，Voice/MCP 只登记待确认（重复请求不重复 emit），物理确认后才真正完成；4 个 Query 载荷不产生任何 sink 调用（只读）。
2. **P14.1 集成修正（发现并修复）**：`mapFocusTap` 只填 `session_id`，而 reducer 入口强制 `task_id` 存在（`reducer.cpp` `if (!request.task_id) return TaskNotFound`），直连 coordinator 会恒被拒。补 `task_id`（FocusView 已携带），原 28 个 presenter case 无回归。
3. **P15 MCP Host**：`learning.get_today_tasks/current_task/remaining_time/today_progress` 为只读投影；`start/pause/resume/request_complete_task` 经 dispatcher(Mcp) 进入域。`request_complete_task` 响应 `{"confirmation":"pending"}`，**AI 绝不直接 Complete**（V4 §8）。remaining 复用 `buildFocus` 数学避免公式漂移；today_progress 规则（排除 Pending、含 skipped 分母）写入代码注释并测例锁定。
4. **P16 Platform Ports**：8 个抽象接口 + 确定性 fake；Learning Domain/interaction/mcp/ports 零硬件头（4b3 扫描门禁证明）。UiPort 只表达语义动作与确认回调，LVGL 归 P17；NetworkPort 只留连接状态 + JSON POST 边界。
5. **C11 host funnel 集成（补强验收）**：新增 `host_funnel_tests.cpp`，让 `learning.*` MCP 工具经 CommandDispatcher 走**真实 AppCoordinator（domain reducer + transactional outbox + FakeDisk 持久化）**，替代脚本化 fake sink/backend——验证了 start→InProgress/2 事件提交、pause/resume 状态机迁移、`request_complete_task` 不触碰 domain、物理确认后 Completed 且 active_session 清空（Done 语义）、busy/Pending 任务的 domain 拒绝路径。

### 11.4 验收标准逐项自检

| 任务包要求 | 结果 |
| --- | --- |
| P14 dispatcher 门禁与映射 15 测例 | ✅ 15/15，含 Voice/MCP 完成零 emit、confirm 单发、Query 零变更 |
| P15 8 工具 + AI 禁止直接 Complete | ✅ 15/15；`request_complete_task` 后 sink 零 Complete 调用，confirm 后恰一次 |
| P16 8 接口 + fakes 往返 | ✅ 8/8（含失败注入、重复注册拒绝、回调触发） |
| P14/P15 对真实 domain 的 host funnel（C11） | ✅ 6/6（真实 coordinator 提交/迁移/门禁/拒绝路径，非脚本化 fake） |
| 新增目录无硬件/OS/Metalio include | ✅ §4b3 扫描 PASS |
| 全量非回归 | ✅ host gate 8/8 二进制 RUN PASS，总计 141 case 0 失败；P4 交叉契约 exit=0（headers 33/33 + implsrcs 3/3 + contract 1/1） |
| 不触碰真机 / 不改 domain/sync 语义 / 无越界 | ✅（见 11.8） |

### 11.5 验证命令与结果

| 验证 | 命令 | 结果 |
| --- | --- | --- |
| 全量 host 门槛 | `tools/dev/verify-host-cpp-tests.ps1 -CompilerPath w64devkit-2.9.1\bin\g++.exe -CrossCompilerPath riscv32-esp-elf-g++.exe` | `RESULT: NATIVE CPP TEST GATE PASS`；smoke compile/link/run PASS；4b/4b2/4b3 扫描 PASS；unit 8/8（coordinator 20 + domain 28 + dispatcher 15 + mcp 15 + host_funnel 6 + ports 8 + outbox 21 + presenter 28 = **141 cases, 0 failures**）；interface exit=0（headers 33/33 + implsrcs 3/3 + contract 1/1，riscv32 target ISA `-fsyntax-only`） |
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