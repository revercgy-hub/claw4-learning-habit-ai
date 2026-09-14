# CODEX-V53-WAVE1 收口与WorkBuddy交接

日期：2026-09-14。审查人：Codex主agent。结论：**A01、A02、B01 ACCEPTED（下述源码/Host范围）**。当前子agent批次结束；后续实施交WorkBuddy，Codex负责代码审查、架构与疑难问题。不派发新子agent，不向外部应用自动发送任务。

## 1. 不可变基线

- 最新设备来源仍为 `workbuddy-app-first-l3-acceptance` @ `7dd6511ab0125962d37f039955298819bdbb77be`，本轮远端复核未变化。`main` 仍为 `03383dbda702e95f69e5e59eab0332526a2915bd`，不能作为接续基线。
- 规划基础：`1ae2a0c546615666a57abdf601b343bc88a043c4`。
- **冻结代码基线：`e70c2830d1f7ea43629a61ce01a33f78b6f11128`**；整合分支 `codex/v53-foundation-wave1`。后续本次收口提交仅修改文档/看板。WorkBuddy fetch后从含本报告的远端交接HEAD建立独立分支，记录完整Base SHA；核对相对冻结代码基线只有文档变化，发现额外代码先报告。
- 工作目录：`E:/workbuddy/claw4-v53-control-20260913`。原项目目录HEAD损坏，未重置、清理或覆盖。`E:/c` 和官方vendor只读取证，未修改。

## 2. 三项结果与审查修订

| 任务 | 实际交付 | 结论及边界 |
| --- | --- | --- |
| A01 | 内容hash驱动同步、变更mtime更新、stale留存/人工清理恢复、受控静态CMake检查、完整来源清单、四文件项目补丁及反向验证 | ACCEPTED：工具与取证；不是完整IDF构建证明 |
| A02 | 删除Runtime破坏性Reset和自动SELFTEST；完成态等待新计划；未配对且确实空存储才seed；配置/读取/恢复/seed失败关门；真实NVS适配器只读分类 | ACCEPTED：源码与Host；未操作真实NVS、未验证本批真机页面 |
| B01 | 独立TimeAuthority；UTC/单调分离、boot_id、三态质量、可选漂移界、日历预算、前跳/回拨与溢出处理 | ACCEPTED：Host与RV32语法；没有接SNTP或校时硬件 |

子agent均按用户要求使用Luna/medium。主agent没有直接采信初版PASS，修订了以下实质问题：

1. A01初版自动删除stale文件存在越界风险，取消自动删除；补齐合法mirror绑定下的路径负测和旧manifest兼容。静态CMake检查只识别支持的SRCS/SOURCES语法，不把注释、message、未使用变量或INCLUDE_DIRS当源码注册。该检查不执行CMake条件/生成器，真实构建仍由A05验证。
2. A01导出的第一份patch实际上是upstream父提交到pin的变化，不能重建项目。主agent删除错误最终产物，重新捕获pin→E:/c四文件差异，逐文件hash验证应用和反向恢复。保留历史提交用于追溯，不发布错误patch为有效基线。
3. A02原先在配置判定前恢复、以布尔presence混淆读错/空存储、忽略seed失败。改为生产使用的启动编排、Missing/Present/Error和只读读取；主agent进一步将无效配置拒绝提前到LearningApp构造前。
4. B01初版误差等于同步年龄，且后续非int128分支仍可能在RV32溢出。修为统一可移植商余运算和向上取整；默认漂移未知时误差界为空、不允许日历提醒。主agent补UTC零点/边界和无效同步不污染锚点测试。
5. 原Host门禁可能在程序被操作系统拒绝启动时沿用上一条编译成功退出码。主agent用显式进程启动结果替换测试执行，并为编译/交叉脚本调用设置失败哨兵；启动失败不能再假PASS。曾有独立A04诊断程序启动被拒，其结果未被当作缺陷复现证据，也未绕过策略。

## 3. 提交追溯

| 项目 | 子agent原提交 | 整合/主agent最终修订 |
| --- | --- | --- |
| A01 | `af2daa7` → `3030fae` → `afc9ec2` → `8e2baec` | `04186ee` → `910ad19` → `3e20f2b` → `0c56a86`；主修 `3346691`、fixture `e70c283` |
| A02 | `a07944a`；其余工作树修订由主agent收口为 `efb97f8` | `6d84ea3` → `7d266c2`；主修 `b1071f9` |
| B01 | `beac193` → `5a67243` → `9d9c5aa` → `9c57da8` | `0b70d60` → `55b8da7` → `f3bf370` → `d2397f2`；主补测试 `b741a7d` |
| 门禁/独立复检 | 主agent | `aa0b905`、`a42bbc2`；实际NVS适配器shim `b07547b` |

实际文件可用 `git diff --name-status 1ae2a0c e70c283`复核；没有提交工具链、固件、完整vendor、凭据或原始设备日志。

## 4. 可复现验证

本机工具：native `E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe`；cross `E:/workbuddy/claw4-idf-tools/tools/riscv32-esp-elf/esp-14.2.0_20260121/riscv32-esp-elf/bin/riscv32-esp-elf-g++.exe`。Python使用Codex依赖运行时；复现可使用正常Python 3。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/dev/verify-host-cpp-tests.ps1 -CompilerPath E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe -CrossCompilerPath E:/workbuddy/claw4-idf-tools/tools/riscv32-esp-elf/esp-14.2.0_20260121/riscv32-esp-elf/bin/riscv32-esp-elf-g++.exe -OutputDir out/v53-wave1-final
pwsh -NoProfile -File tools/dev/tests/sync-app-first-mirror.tests.ps1
pwsh -NoProfile -File tools/dev/tests/verify-nvs-read-contracts.ps1 -CompilerPath E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/dev/tests/invoke-host-test.tests.ps1
python tools/dev/tests/verify-metalio-patch.py --vendor E:/workbuddy/学习习惯培育AI/vendor/MetalioClaw4
pwsh -NoProfile -File tools/dev/write-app-first-manifest.ps1 -RepoRoot E:/workbuddy/claw4-v53-control-20260913 -OutputDir out/v53-wave1-manifest
```

| 检查 | 主agent实测结果 |
| --- | --- |
| Native编译/链接/实际运行 | exit 0；20/20 suites PASS；TimeAuthority 14 cases；LearningApp glue 7 cases；不把suite数混当case总数 |
| RV32接口检查 | exit 0；42/42头、4/4实现、1/1契约PASS；日志历史标签仍写interaction/mcp，实际4源包含time_authority.cpp |
| 镜像fixture | exit 0；相同内容mtime不变、旧mtime变更、删除恢复、未知文件保留、路径/旧manifest/CMake负测PASS |
| 真实NVS适配器+模拟API | exit 0；namespace/key缺失、open/read失败、损坏blob、sequence42/ACK41恢复、配置状态分类PASS；模拟写调用0 |
| 进程启动门禁 | 缺失exe正确失败；正常系统exe确实运行，PASS |
| 固定upstream项目patch | 四文件apply/check/hash/reverse PASS；SHA256 `58bfbe5266a9fa00980be5e5078b570b9915165e199981e6dd54dcb21aa81937` |
| 来源manifest | 79项无重复、Runtime/Screen/NVS/Time/patch均包含；记录真实git_commit与git_dirty；非Git目录拒绝写成功manifest |

Host全量执行对应 `b1071f9` 的产品代码；随后 `e70c283` 只把CMake负测字符串改为有效换行，镜像fixture已重跑PASS。日志在本地忽略目录 `out/v53-wave1-final/host_result.txt`、`interface/verify_result.txt`、`out/a01-fixture-review.log`；关键结果以上述报告及可重跑脚本保留。

## 5. 未覆盖与后续任务

本批没有IDF完整构建、设备运行、串口、Flash或真实NVS验证。NVS shim验证实际适配器逻辑，不证明ESP-IDF ABI/Flash原子性。TimeAuthority只处理UTC与单调时钟；家庭时区计划转换属于C01/B03，不在这里复制另一套时区机制。

仍需处理：A03网络主循环/锁阻塞，A04旧快照覆盖离线终态的定向复现及保护，B02仲裁，B03提醒恢复。已有D5代码修复与音频yield有效性仍须A05同一候选验证。原有CMake补丁尚未登记新增time模块，不能拿本报告宣称固件已包含本批功能。

**唯一下一实施包：[WB-V53-NEXT-001](../tasks/WB-V53-NEXT-001.md)，READY。** CP0复核 → CP1网络并发 → CP2离线终态 → CP3仲裁Host → CP4提醒Host → CP5联合验证。WorkBuddy单分支按checkpoint顺序提交并push，Codex按不可变SHA审查；CP5交REVIEW_READY后停止。真机候选、Flash、vendor配置和真实儿童数据继续受原门禁约束。
