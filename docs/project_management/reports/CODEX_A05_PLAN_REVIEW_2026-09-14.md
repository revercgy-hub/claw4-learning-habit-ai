# A05下一阶段方案审查

## 结论

原方案方向合理：先形成可复现M0构建候选，再单独授权真机。**需修订后执行，当前只放行CP0前置取证，完整构建待前置验收。** 不能把上一流的REVIEW_READY或历史Host日志改写为当前完整ACCEPTED。

用户本轮要求Codex只负责任务编排、审查及疑难问题定位，不直接完成实现。本轮仅创建独立审查工作树、读取源码/环境、运行既有Host门禁和编写文档；未改业务代码、脚本、CMake、配置，未执行IDF构建或硬件操作。文档原文是参考，不自动授权执行其中命令。

输入原文SHA256：`a3b14dfab63239299ddad6b3c983b768a5e18d57fccbbddc037794600da8a21b`；保留于 `references/WB-A05-BUILD-001_ORIGINAL.md`。

## 1. 当前提交和验证事实

- 远端与WorkBuddy本地HEAD一致：`19fd979d4222093ff4ce7464e5b58407586594a2`，远端 `workbuddy/v53-next-001-reliability`。原WorkBuddy工作树clean。
- 新审查树：`E:/workbuddy/claw4-a05-plan-review`，分支 `codex/a05-build-task-review`。不在WorkBuddy分支写入。
- 本次增量复核 `7b080e0`、`86db779`、`6736fda`、`19fd979`：snapshot ownership检查与publish在同一state临界区；锁顺序state→holder；authenticate不再读coordinator counters；locked同步尾部无额外锁外authPaused读取。对应最终cleanup套件6 cases、本次均PASS。这是限定增量审查，不是对整个前序8千余行改动重新签发无条件验收。
- WorkBuddy现存 `out/fcc-final/host_result.txt`：29/29 PASS；报告最终状态仍为REVIEW_READY，没有找到本流Codex最终ACCEPTED记录。
- Codex独立运行既有Host脚本，本次结果 **exit 1，24/29 PASS**。5项均为Windows `An Application Control policy has blocked this file`，不是已观察到的断言失败：outbox_codec_tests、restart_recovery_tests、backend_client_tests、learning_backend_session_tests、sync_diagnostics_tests。没有重命名、改代码或关闭策略绕过阻断。
- 本次RV32语法：49/49头、5/5实现、1/1契约PASS。该脚本并未覆盖所有sync/reminder实现；不能用它代替完整IDF编译。
- 新并发清理6 cases、生产路径8 cases、backend session编排8 cases、Reminder21 cases均实际通过；上述5项未运行，不能拼成29/29。
- CI未见 `.github/workflows`；TCP loopback报告为ENV_VERIFY_REQUIRED，不能凭socket桩改成TCP已通过。

复现命令：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/dev/verify-host-cpp-tests.ps1 -CompilerPath E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe -CrossCompilerPath E:/workbuddy/claw4-idf-tools/tools/riscv32-esp-elf/esp-14.2.0_20260121/riscv32-esp-elf/bin/riscv32-esp-elf-g++.exe -OutputDir E:/workbuddy/claw4-a05-plan-review/out/review-host
```

## 2. 本地构建配置：最重要的阻断点

| 输入 | 实测 |
| --- | --- |
| 官方vendor pin | `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950` |
| 本地IDF | `E:/workbuddy/esp-idf-5.5.4-ascii`；version.cmake标记5.5.4，但不是Git仓库，不能伪造Git提交身份 |
| 已有build cache | `E:/workbuddy/claw4-idf-cold-c5-20260906/CMakeCache.txt`：CMAKE_HOME_DIRECTORY=E:/c；SDKCONFIG指向该build目录内sdkconfig |
| 历史有效C5配置 | build内及 `claw4-idf-cold-c5-20260906-frozen-20260912/sdkconfig` SHA256相同：`a901f20491671a9cbca8cbe60c4ac4b26d91ac1916d36530b9475b6704fabc26` |
| E:/c当前配置 | SHA256 `436050e9b95265fa9ad6f806b5006fb52d7cce9d3f8bfef0864b4bfa031fbdec`；CP_TARGET=ESP32H2，IDF_SLAVE_TARGET=esp32h2，不能作为C5候选配置 |
| C5配置关键项 | esp32p4、METALIO_CLAW_4、CP_TARGET_ESP32C5、SLAVE_IDF_TARGET_ESP32C5、32MB、FreeRTOS_HZ=1000 |
| 选用分区 | `partitions/v1/32m_dual.csv`；SHA256 `3522639424052778951248d32b02cff0690ddb86d84968b357d43fffc79f2ff6`；ota_0@0x200000/9MiB，ota_1@0xb00000/4MiB |
| 当前依赖锁文件 | E:/c/dependencies.lock SHA256 `90af1addf9daf8ae1ec3094a798d7ab56cd12a4fef5702696d08a524458f91e1`；尚未证明它与历史C5 managed_components完全匹配 |

因此新目录不能默认取E:/c/sdkconfig，更不能通过set-target/menuconfig或重选H2/C5“让它编过”。CP0应核对历史C5配置、components和依赖来源，CP1在隔离副本显式指定SDKCONFIG。IDF、组件、资源身份仍需WorkBuddy补齐。

## 3. 对原方案的修改

| 问题 | 完善后的安排 |
| --- | --- |
| 所有新模块必须Linked=YES，但又禁止设备接线 | SyncExecutor/SingleFlight已有生产调用，应证明Referenced/Linked。Time/Arbiter/Reminder未有设备调用，可Compiled=YES、Archive member=YES、Final ELF=discarded；不得加假调用、whole-archive、强制符号保留或关闭节裁剪来制造“装进镜像” |
| “必要编译选项/薄适配”白名单过宽 | 限CMake源/头路径、既有依赖登记；若需业务TU、异常/RTTI/优化/ABI/SDK开关或新底层依赖修改，列出最小建议补丁供Codex审定后再实施 |
| 未确定配置路径/冷构建目录 | 固定C5配置候选及hash；新短ASCII构建根，不复用旧cache/object，不改E:/c和vendor |
| “超出当前partition立即停”含义模糊 | ota_0超过9MiB硬停；既有4MiB ota_1不足独立记录，不能改布局/绕过检查。只在官方构建命令本身成功且ota_0符合时形成application-only候选，不宣称双槽OTA可用 |
| patch和D5混为一类 | D5在repo learning_screen中；audio yield在既有upstream项目补丁中，分别记录源码/patch hash。1000Hz配置下1ms转换为1tick，不能沿用历史“10ms”文字作当前证据 |
| 覆盖原A01patch | 保留A01历史patch/hash；新增有明确base和顺序的A05补丁层，正向/反向验证，更新integration manifest，不篡改旧取证 |
| bootloader/partition产物容易误用 | 仅作构建证据，另列application-only候选，禁止生成/执行烧录指令；完整flash_args、合并镜像不是候选烧录授权 |
| Host PASS、网络deadline、真机稳定性混淆 | 分别保留Host、Build、TCP环境与Hardware状态；生成候选不等于UI实时/总deadline/语音/真NVS通过 |
| A05通过后所有Host开发都串等真机 | C01契约、C03-HOST可在后续独立任务中推进，不必技术上等待设备连接；本包仍不自动启动它们，DEVICE必须过候选与授权门禁 |

## 4. 调度决定

当前上一流保持 **REVIEW_READY（验收待环境证据收口）**。两个cleanup修改的增量复核通过，但本轮不能给整个流最终ACCEPTED。

下一唯一可领取事项为修订任务书 `WB-A05-BUILD-001` 的 **CP0 / READY**：补齐前序可复现Host证据及C5环境/依赖清单，只做取证报告。CP1～CP4 **QUEUED且受门禁约束**：Codex记录前序ACCEPTED、确认配置/依赖清单后才能继续。WorkBuddy不自行跨越此门禁。

A05-DEVICE保持HOLD；实际设备测试仍需明确的新候选授权。Codex后续只审查提交与证据、定位疑难并给修订意见；实现与补测由WorkBuddy完成。
