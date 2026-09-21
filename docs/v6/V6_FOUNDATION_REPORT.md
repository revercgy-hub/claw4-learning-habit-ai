# CODEX-V6-FOUNDATION 首批交付

日期：2026-09-21。分支 `codex/v6-foundation`，基线 `5657ebed64ad5962c889fcf90f0979f5891ab862`。交付状态 REVIEW_READY；M0 仍 IN_PROGRESS，非设备验收。

## 实际修改

- `AGENTS.md`、`docs/project_management/TASK_BOARD.md`：按本轮用户要求恢复 Codex 直接实施重要工作的职责，建立 V6 唯一入口，历史记录保留。
- `docs/v6/V6_ARCHITECTURE.md`、`V6_MIGRATION_MAP.md`、`V6_BASELINE.md`、`V6_TASK_BOARD.md`、`V6_M0_IDF61_CLAW4_PORT.md`、`V6_M1_XIAOZHI_NAS_VOICE.md`：分层、线程/持久化所有权、pre-LLM 命令设计、离线提醒缺口、真实代码路径、M0/M1 执行与验收。
- `integration/v6/baseline.lock.json`：四方不可变源码 pin，明确工具链/组件/分区尚未冻结。
- `integration/v6/learning_core/CMakeLists.txt`：独立构建 14 个现有核心源码，不包含旧 Voice/STT/UI 路由；复用四个真实行为测试程序。
- `tools/v6/baseline.py`、`test_baseline.py`：源码 pin/干净树检查，迁移依赖闭包扫描与负向用例。

本轮未修改旧产品源码，也没有把历史 WorkBuddy 分支整体标记 ACCEPTED。新 checkout 在忽略的 vendor 下；没有向 Metalio 官方仓库推送。

## 验证结果

| 检查 | 本轮真实结果 |
| --- | --- |
| 四方源码定位 | Learning 远端 fetch 核实；XiaoZhi v2.5.0、IDF v6.1、Metalio ca3aa3fa SHA/干净树 PASS |
| 依赖边界扫描 | 14 源文件、46 个源码/头文件闭包 PASS；无旧 VoiceSession 或平台 include |
| 新工具测试 | 5/5 PASS；覆盖传递依赖污染、平台头、循环、缺失 include、错误 SHA 与未跟踪输入 |
| CMake configure/build | PASS；GNU C++16.2.0 / CMake3.30.2，核心以 -Wall -Wextra -Werror 编译，4 个测试程序链接成功 |
| Domain reducer | 28 cases / 0 failures |
| Outbox | 22 cases / 0 failures |
| UI presenter | 28 cases / 0 failures |
| Coordinator | 编译/链接 PASS；运行被 Windows 应用程序控制策略阻断，NOT_RUN，不记通过 |
| CTest 总结果 | 3/4 suites 运行通过；总命令失败，不能宣称全部测试 PASS |
| upstream Python tests | 81 tests，5 errors；均发生于 Windows 清理当前工作目录的临时目录（WinError32/5），76 tests 无报错；上游保持未改，不能标套件 PASS |
| git diff --check | PASS |
| IDF6.1 Claw4 build / hardware / 20轮 | NOT_RUN |

## 可复验命令与失败证据

工作目录 `E:/workbuddy/claw4-v6`。Python 可用路径为 `E:/workbuddy/claw4-idf-tools/python_env/idf5.5_py3.12_env/Scripts/python.exe`（3.12.14），仅运行 stdlib；CMake `E:/workbuddy/claw4-idf-tools/tools/cmake/3.30.2/bin/`，Host compiler `E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe`。

```powershell
$env:PATH = 'E:/workbuddy/toolchains/w64devkit-2.9.1/bin;' + $env:PATH
python -m unittest discover -s tools/v6 -v
python tools/v6/baseline.py --xiaozhi vendor/xiaozhi-esp32 --idf vendor/esp-idf --metalio E:/workbuddy/学习习惯培育AI/vendor/MetalioClaw4
cmake -S integration/v6/learning_core -B out/v6-core -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe -DCMAKE_MAKE_PROGRAM=E:/workbuddy/toolchains/w64devkit-2.9.1/bin/make.exe
cmake --build out/v6-core -j 4
ctest --test-dir out/v6-core --output-on-failure -V
python -m unittest discover -s vendor/xiaozhi-esp32/scripts/tests -v
```

上述 python/cmake/ctest 简写须使用所列实际程序路径或配置当前进程 PATH。初次 configure 缺 compiler bin PATH，GCC `cannot execute 'as'`，补进程 PATH 后解决；未修改系统配置。

CTest 证据：`out/v6-core/Testing/Temporary/LastTest.log`；直接执行 coordinator 返回“应用程序控制策略已阻止此文件”。未修改安全策略或绕过阻断。上游证据：`out/v6-upstream-tests.txt`，`test_build.py` 五项在 TemporaryDirectory 清理前仍处于其目录，Windows 不允许删除；未据此修改冻结 upstream。原始本地日志按仓库规则不提交。

## 风险、偏差与下一步

- 工作区路径调整是必要隔离：用户当前旧目录 HEAD 无法解析且有大量 staged/untracked 工作；本轮无覆盖、重置、合 main 或修复旧 Git。
- 源码 pin 不等于可重复固件：IDF submodules、工具链、组件锁、Board、配置、布局/恢复尚未齐备。下一步由 Codex 完成 M0-1 环境冻结与 M0-2 Board Port。
- 现有 ReminderCore 的时钟不可信抑制规则不满足全部 V6 离线场景，需 M2.5 有证据的时钟设计；先不扩大复杂提醒。
- 全量回归仍有环境阻断；未验收旧 WorkBuddy 全流。复核重点：迁移源文件清单、命令来源/确认权限、短锁网络边界、NVS schema、C5/屏幕/音频型号与实际分区。
- HARDWARE_VERIFY_REQUIRED：全部 V6 设备项。没有把9/20历史设备记录升级为本轮检测事实，也未执行设备写入。
- WorkBuddy 当前无新任务下发；核心架构由 Codex 直接承担，接口冻结后才拆辅助包。
