# V5.3 项目现状核验与重规划报告

日期：2026-09-13；主 agent：Codex；工作性质：远端事实核查、只读源码审计、Host 基线验证、架构与任务优化。

## 1. 不可变基线

- 仓库：`https://github.com/revercgy-hub/claw4-learning-habit-ai.git`。
- 远端最新开发分支：`workbuddy-app-first-l3-acceptance`。
- Base SHA：`7dd6511ab0125962d37f039955298819bdbb77be`，提交时间 2026-09-13 11:43:32 +08:00。
- 标题：`fix(WB-LEARNING-L4): replace one-shot lv_timer with tick deadline to stop double-free heap corruption`。
- 2026-09-13 14:16 +08:00 再次 `git ls-remote --heads origin workbuddy-app-first-l3-acceptance` 返回同一 SHA。
- `main` 仍为 `03383dbda702e95f69e5e59eab0332526a2915bd`（9/2）；最新开发分支比它多 111 个提交，不能用 main 表示设备开发现状。
- 原工作区 `E:/workbuddy/学习习惯培育AI` HEAD 无法解析且大量暂存/未跟踪文件；未 reset/clean/checkout 或覆盖它。
- 新建独立 clone：`E:/workbuddy/claw4-v53-control-20260913`；规划分支 `codex/v53-architecture-task-plan`，没有复用共享 refs 故障仓库。

V5.3 输入文件：`C:/Users/rever/Downloads/CLAW4_学习伙伴_后续开发任务_V5.3 (1).md`；原文件 SHA256：`ee7253099fb462c8893c8b65ec69039a3534e1fc14450c13385a499a26437b30`。仓库内保存字节相同参考副本，执行指令不自动生效。

## 2. 最新进展与证据等级

| 能力/问题 | 当前判断 | 证据 |
| --- | --- | --- |
| L1 Pause/Complete PersistFailed | 旧阻塞已修复，保留回归；不能重开为未知设备问题 | 根 AGENTS 9/4 更新；`CODEX_WB_LEARNING_V4_L1_DEVICE_TEST_2026-09-04.md`；`device/core/random_id.h` |
| 根因 | nano-newlib `%llx` 格式化导致重复 ID；已改 hex 生成并加批内重复保护 | 上述报告与当前源码；原 V5.3 §2 SUPERSEDED |
| L2/L3 设备后端闭环 | 历史报告 6/6，通过断网操作/恢复补传与家长可见；本轮未重新上机 | `WB-APP-FIRST-L3_NETWORK_ACCEPTANCE_2026-09-12.md` §3 |
| pause_count 丢失 | 陈旧对象漏编；报告追加真机验证已通过 | `WB-APP-FIRST-L3_BUILD_STALE_FIX_2026-09-12.md` §11，提交 `82983dd` |
| D2 reset 序号失配/数据清空 | 源码证实未修；最高优先级数据保护 | `learning_screen.cc` 全完成按钮→`ResetToSeed()`→`eraseAll()`；上述报告 §12 |
| D8 网络拖住实时交互 | 主循环同步 I/O + runtime 共享锁跨网络等待，源码证实 | `metalio_http_transport.cpp`、`learning_runtime.cpp::RunOnlineCycle()` |
| 快照覆盖离线完成状态 | 源码可疑路径，高风险待定向回归；不冒充已复现 | `runOnlineCycle()` 先 pullToday；`applyTodaySnapshot()` 仅保护 Running/Paused |
| L4 STT 确定性命令 | 报告称功能链路打通；稳定性未过 | `WB-LEARNING-L4_TECH_HANDOFF_2026-09-13.md` |
| D3/D4 | 报告记载音频让步/UI 入队修复；集成 manifest 与报告状态表述不一致，需同一候选联合验证 | L4 报告 §5 与 `integration_manifest.md` #5/#6 |
| D5 double-free | 修复源码存在于 `7dd6511`；报告明确修复版未刷入验证 | L4 报告 §2/§5-D5；最新 diff |
| 可信时间 | 未实现：epochSeconds 返回 uptime，isTimeSynced=false | `device/ports/learning_clock.cpp` |
| Reminder | 尚无独立 ReminderEngine/持久化模块；属于新增工作 | 当前 firmware/main 文件树 |
| 家长计划 | PWA/Backend/设备同步已存在；需追加具体开始时间与提醒契约 | Task 已有 estimated_minutes/priority/scheduled_date/version；TodayResponse date/tasks |
| MCP | Host learning MCP 已存在；设备注册与新提醒工具待做 | `firmware/main/mcp/`；L4 报告 L5/L6 未开始 |
| NAS 自建语音 | 方案建议，尚不能认定部署或 ASR-only 已验证 | L4 报告 §8/§10 |

## 3. 固件、硬件与报告限制

L4 报告写作时列 `5d30e49` 和 9,269,392 B / SHA256 `d9bd2e6c1100aa542cdcb15eaa573541ed745d5cd0c7d9173bdfcba427d280d2`，并明确含 D5；最新 Git `7dd6511` 不等于当前设备已运行此修复。本轮未读串口/Flash，当前实机精确镜像仍待批次核验。

报告中部分“字节一致”证据仅为首 8 KiB 与 app_desc，不能升级为整片 Flash 哈希证明。D1 的 touch 方案是现场修复，不等于构建可复现性已永久修复。L3 报告中的数据库清理是历史临时绕过，绝不作为后续恢复操作指令。

当前库只有 vendor 修改文字 manifest，未见完整版本化 patch 文件。新增构建任务须补齐源/补丁/配置/产物链，不能只引用 `E:/c` 外部可变目录。

已知 I2C 触摸异常、模型分区与配置唤醒词不一致分别列硬件待验证；本轮不改模型分区，不把改分区或更换硬件作为 MVP 默认路径。

## 4. 本轮方法

1. `git ls-remote --heads origin` 列全部远端分支。
2. 独立 `git clone --no-checkout`，`git for-each-ref --sort=-committerdate`、`git log --all` 对照最新提交。
3. 从明确基线建立独立 `codex/` 规划分支，阅读最新 AGENTS/看板/报告/代码。
4. 两个只读子 agent 分别审计设备风险、V5.3 契约；主 agent复核关键路径。
5. 第三个子 agent 运行现有 Host C++ 验证；结果见下一节。本轮不重跑 Backend/PWA，不把历史全链 PASS 当作本轮测试。

## 5. 本轮验证

本轮在原始产品基线7dd6511运行现有验证脚本，退出码0，耗时79.245秒：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/dev/verify-host-cpp-tests.ps1 `
  -CompilerPath 'E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe' `
  -CrossCompilerPath 'E:/workbuddy/claw4-idf-tools/tools/riscv32-esp-elf/esp-14.2.0_20260121/riscv32-esp-elf/bin/riscv32-esp-elf-g++.exe' `
  -OutputDir 'E:/workbuddy/claw4-v53-control-20260913/out/v53-baseline'
```

- 原生GCC实测16.2.0；RISC-V GCC14.2.0。
- Smoke编译/链接/运行PASS，依赖扫描PASS，18/18 suites PASS。9组打印数值合计151 cases/0 failures，其余9组只打印PASS，不能把151称为全部用例数。
- RISC-V `-fsyntax-only`：41/41 headers、3/3 implementation sources、1/1 contract test PASS；不是设备镜像编译或实机验证。
- 日志：独立clone的 `out/v53-baseline/host_result.txt`、`interface/verify_result.txt`、`timing.txt`、`runner.log`，不提交生成目录。
- Backend/PWA/完整E2E、IDF build、真机/24h漂移本轮未执行。

文档检查：核心四文档9个相对链接均存在；旧看板原文完整保留；原V5.3副本SHA与输入一致。staged whitespace检查中，参考原文第3–6行自带Markdown双空格换行，按原样保留；排除该字节保真参考文件后 `git diff --cached --check`通过。独立子agent复审未发现依赖环，提出工具授权语义、C03 Host/设备前置分层、Host E2E负责人与白名单三个修订项，均已落实；A03 UI白名单和A05实验入口变更另行限定，避免越权。定向复审结论ACCEPTED（仅规划），提交范围为9个治理/参考文档，无产品源码。

## 6. 交付与范围

- 新架构：`docs/ARCHITECTURE_V5_3.md`。
- 子 agent 工作包：`docs/project_management/tasks/CODEX-V53_AGENT_WORK_PACKAGES.md`。
- 更新根 AGENTS、任务看板与原架构导航，旧看板移入历史快照并标 SUPERSEDED。
- 原 V5.3 作为参考保存，不将其命令直接执行。
- 本轮仅规划/治理文档修改，未修产品代码、未刷机、未删除事件/数据库、未改 vendor/BSP/配置/分区、未部署或上传儿童数据。
- 主 agent 恢复架构与代码审查职责，子 agent 无权自验收、合 main、扩范围。历史验收保留其当时口径，不倒改。
