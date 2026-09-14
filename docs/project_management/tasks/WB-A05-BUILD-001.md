# WB-A05-BUILD-001：M0隔离总装与唯一候选冻结（审查修订版）

编排日期：2026-09-14。实施：WorkBuddy；审查/架构/疑难定位：Codex。用户本轮明确Codex不直接实施。本包替代Downloads同名方案，原文作为参考保留。必读[本轮审查](../reports/CODEX_A05_PLAN_REVIEW_2026-09-14.md)、根AGENTS、看板、上一流报告、V5.3架构、A01证据和集成登记表。

## 0. 领取状态与基线

**CP0 READY；CP1～CP4 QUEUED，未经下述放行不得开始构建。** 当前上游工作代码固定为 `19fd979d4222093ff4ce7464e5b58407586594a2`，来自 `origin/workbuddy/v53-next-001-reliability`。本任务书在 `codex/a05-build-task-review` 分支仅追加文档；从含本任务书的交接HEAD建立独立 `workbuddy/a05-build-m0`，记录完整Base SHA，核对相对19fd979只有文档差异。若WorkBuddy源分支又推进，先列差异交Codex重定基线。

当前不能声称上一流ACCEPTED：WorkBuddy报告29/29 PASS，本次独立复跑24/29，5个exe被Windows应用控制拒绝启动；最终cleanup6 cases通过。既不将环境阻断当已证明的产品bug，也不跳过这5项盖全量PASS。

使用正常Git ref创建独立分支。本轮Codex含斜杠分支已正常建立；若实施环境仍失败，保存错误，不能编辑refs文件/强推/改写历史来规避。不写原损坏工作目录、E:/c、官方vendor、历史冻结目录或旧build。

## 1. CP0：前置验收证据与构建输入盘点

本checkpoint只允许新增/补充 `docs/project_management/reports/WB_A05_BUILD_001_REPORT.md`，不改代码/配置，不运行IDF configure/build。报告含：

1. 清洁工作树、远端/本地SHA、前序提交链和现存测试命令/日志。针对5个启动阻断项提供同SHA、编译参数、程序hash、实际运行证据及环境解释；如需重跑用完整现有Host Gate。只在正常获准的环境执行，不关闭应用控制、重命名/改二进制绕过阻断。无法执行则如实ENV_VERIFY_REQUIRED，提交后等待；不反复尝试制造偶然通过。
2. 29套件完整清单与新增cleanup/production-path测试；49头/5实现/1契约语法门禁覆盖范围；CI不存在则NOT PRESENT。TCP loopback仍单列ENV_VERIFY_REQUIRED，socket桩不是loopback。
3. C5配置来源：`E:/workbuddy/claw4-idf-cold-c5-20260906-frozen-20260912/sdkconfig`，预期SHA256 `a901f20491671a9cbca8cbe60c4ac4b26d91ac1916d36530b9475b6704fabc26`。旧build cache也指向其build内同hash配置；**禁止采用E:/c当前H2 sdkconfig**。列出target、板型、协处理器、Flash、PSRAM、tick、异常/RTTI、分区关键值，不输出凭据字段。
4. 固定vendor `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950`；现有A01四文件patch及JSON；核对完整镜像相对pin的额外源/根CMake/components差异。四文件patch不自动证明其它本地文件干净。
5. IDF路径 `E:/workbuddy/esp-idf-5.5.4-ascii`，源码version.cmake=5.5.4但无Git元数据；用发行来源/关键工具hash和版本识别，不伪造Git SHA。工具根 `E:/workbuddy/claw4-idf-tools`，Python环境 `python_env/idf5.5_py3.12_env`。记录CMake/Ninja/compiler/Python实际版本。
6. dependencies.lock、所有本地components/managed_components的版本与内容来源，特别esp_hosted/C5；若锁文件和缓存不一致先报告。资源/模型/自定义wakeword的来源与hash；不自动升级组件或重下载整套依赖。
7. 模块矩阵：time、sync_executor、single_flight_http_transport、interaction_arbiter、reminder、reminder_wake_port，以及session_lease/session_snapshot_publisher头文件；列出源、当前设备调用点、需要的CMake项和后续接线边界。
8. 提交精确拟修改文件、短ASCII隔离目录建议（例如未占用的 `E:/claw4-a05-<shortsha>/src`、`build`）、磁盘空间和输入复制方式。配置/依赖若无法在禁区不变下匹配，停在此处。

CP0提交并push后等待Codex。**只有Codex在看板记录前序WB-V53-NEXT-001 ACCEPTED、配置与依赖清单认可、CP1 READY，才可继续。** 环境问题由WorkBuddy整理证据，Codex定位并给处理意见，不私自绕过策略。

## 2. CP1：可重放的最小构建登记

允许的repo路径：`integration/metalio_claw4/patches/` 下新增A05补丁/元数据；`integration/metalio_claw4/integration_manifest.md`；`tools/dev/` 下本任务构建、清单与验证脚本；本流报告。实际upstream改动仅在隔离src副本的 `main/CMakeLists.txt` 中追加源/必要include与已存在依赖登记，由新增patch表达。不改官方只读vendor。

保留A01 `project-ca3aa3fa.patch/.json` 原身份，新增A05补丁层记录其base、应用顺序、前后hash与回滚。源码从pin+已认可patch重放，再同步19fd979或经审查的后续source SHA；未知漂移不得从E:/c整树继承。读取C5配置到新的专用SDKCONFIG副本并显式传入；不复用旧CMakeCache、object或ELF。

登记sync_executor、single_flight_http_transport、time、interaction_arbiter、reminder的实际cpp，头文件只需合法include路径，不伪装为链接实现。所有源用compile_commands/构建对象清单验证。同步工具现有静态CMake检查只支持有限语法，不替代实际configure/build结果。

禁止改Coordinator/ACK/tombstone/SessionLease/Time/Reminder/Arbiter语义；禁止加dummy调用、whole-archive、强制保留符号、禁用dead stripping。禁止在本包接SNTP、真实声音、Overlay、ReminderWakePort驱动或新任务线程。

如果发现C++/SDK不兼容，报告具体错误、最小建议diff、是否涉及异常/RTTI/优化/ABI和验证需求；由Codex审定扩展白名单后WorkBuddy实施。不能借“编译修正”改全局配置、底层组件或删测试。

CP1验证：补丁在隔离fixture应用/反向和hash通过；同步Check无未解释差异；配置关键项与批准输入一致；提交源/脚本/补丁后保持clean。可继续CP2。

## 3. CP2：完整ESP-IDF构建

只在新隔离根执行完整 `idf.py build`，脚本设置正确IDF环境，并显式传入source/build/SDKCONFIG路径。不得照抄旧目录ninja命令；不得用set-target/menuconfig重置配置。记录完整命令、exit code、日志与版本。

输入必须已提交且clean，再构建。生成app bin/ELF/map、bootloader bin、partition-table bin及其它被官方build生成的资源；它们全部是本地构建证据，不是烧录授权。不运行flash/app-flash/erase/monitor，不执行flasher_args，不生成“可直接全刷”的用户指令。

配置在configure前后均记录hash；若自动Kconfig迁移改变关键值或引入未审定默认值，立即报告，不能把自动生成视作授权。IDF、components、dependencies.lock前后核对，无静默升级。官方构建返回非零必须按失败报告，不手动跳过尺寸检查。

分区固定 `partitions/v1/32m_dual.csv`：ota_0 offset0x200000，容量9MiB（9,437,184 bytes）；ota_1 offset0xb00000，容量4MiB。app超ota_0硬停。既有ota_1不足必须明确记为双槽OTA不可用；若官方build仍成功且ota_0符合，可提交application-only构建证据，不改分区或OTA策略。不得将旧余量当新候选余量。

## 4. CP3：链接、来源与既有缺陷审计

| 模块 | 本阶段验收 |
| --- | --- |
| SyncExecutor / SingleFlight / BackendSession | 实际设备Runtime/HTTP路径已引用；Compiled、Referenced和最终链接证据均需要，提供map/object/符号或优化后的等效调用证据 |
| TimeAuthority / InteractionArbiter / ReminderCore | 必须纳入真实目标编译、对象/归档可定位；当前没有设备接线，允许被最终ELF裁剪，标记COMPILED_NOT_WIRED或GC_DISCARDED，不强行保留 |
| ReminderWakePort / lease / snapshot publisher | 头文件在真实目标TU中类型编译；模板/纯接口不要求独立nm符号；wake设备实现留C03/E01 |

每行分别记录Compiled、Archive、ELF retained/discarded、Referenced、Runtime-wired，不把archive符号当最终ELF符号。若后续要求休眠模块实际保留到产品镜像，另立接线任务，不能在此隐式扩大范围。

D5核查repo learning_screen的tick deadline与不再重复删除timer；其代码hash独立于vendor patch。audio yield核查现有patch、锁作用域和实际 `CONFIG_FREERTOS_HZ=1000` 下 `pdMS_TO_TICKS(1)` 的结果；不得沿用历史“10ms”的描述或在此擅改yield实现。静态存在不等于20轮真机稳定。

重新运行完整Host Gate；若只变CMake/文档且同SHA有效门禁已有，可引用本轮CP0准确证据，产品源码有任何经准许的编译修订必须重跑。TCP loopback、网络总deadline、UI延迟、真NVS均单列，不以Host/Build替代。

## 5. CP4：冻结候选与交付

唯一候选名 `claw4-v53-m0-a05-<source-shortsha>`。分清source SHA、构建输入manifest SHA、报告提交SHA；文档后续提交不改变已冻结代码，不对最终产物来源循环引用。

交付 `WB_A05_BUILD_001_REPORT.md`：

- accepted parent、source/branch、upstream、补丁链、全量受控源码hash、dirty=false、SDKCONFIG原始/生成hash、分区CSV/bin hash、dependencies/components版本/hash、工具链和命令。
- app bin/ELF/map、bootloader、partition-table以及实际生成资源的size/SHA256；app的ota_0绝对余量与百分比。大产物留本地不可变候选目录/既有批准存储，不提交Git或公开发布。
- compile_commands、map、link/object/module矩阵；应用路径与未接线模块分别说明。
- clean build成功、Host证据、失败尝试记录。记录可重跑流程；不得无依据宣称跨机器字节级可复现。
- 独立列出application-only候选app与offset；bootloader/partition/resource标记EVIDENCE_ONLY_NOT_FOR_FLASH，完整flash_args不作为交付执行入口。
- HARDWARE_VERIFY_REQUIRED：启动/触控P95、DNS/connect/headers/body/close耗时及未解决总deadline、20轮语音、真NVS/掉电；Reminder声音/Overlay/设备校时/LightSleep属于后续功能，不作为本包未实现缺陷偷偷补入。

每checkpoint独立提交并普通push，不rebase/squash已审历史、不force-push、不推官方origin。最后工作树clean、报告与manifest完整，提交 **A05-BUILD REVIEW_READY** 后停止。由Codex审查，WorkBuddy不得自行ACCEPTED或开始真机。

## 6. 下一路线（不是本包执行授权）

A05-BUILD认可后另拟A05-DEVICE，仅同一候选、合成测试身份/任务、开发后台，先启动/学习页/离线操作/网络挂起/重启恢复，再语音稳定性；Flash需用户新授权。

后续主线C01 TodayPlan/时间元数据 → C03-HOST合成接线 → C03-BUILD设备时间/Reminder薄适配 → C03-DEVICE主动提醒 → C04习惯反馈。C01/C03-HOST不必技术上阻塞于设备连接，但必须另下任务包；PWA排后，AI、NAS、LightSleep不进入当前流。

## 给WorkBuddy的启动文本

请领取本修订版WB-A05-BUILD-001，只先执行CP0，记录19fd979代码基线、Host运行阻断证据和已知C5配置/依赖清单。不要从E:/c的H2配置直接build。CP0提交后等Codex确认前序验收及输入清单；放行后按CP1到CP4完成隔离总装和application-only候选，交REVIEW_READY停止，不Flash、不接提醒硬件、不扩展业务。Codex负责审查，具体实现与补测由WorkBuddy完成。
