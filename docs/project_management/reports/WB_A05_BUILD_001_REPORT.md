# WB-A05-BUILD-001 实施报告（CP0：前置验收证据与构建输入盘点）

| 项 | 值 |
| --- | --- |
| 任务 | WB-A05-BUILD-001（审查修订版） |
| 本轮范围 | **仅 CP0**（含 §12 规则澄清、§13 复审 2 修正）。CP1～CP4 未开始 |
| 报告状态 | ✅ **CP0 ACCEPTED**（Codex 复核 2026-09-15，rev `6d49c72`）。**CP1 按 Codex 原裁定为 QUEUED**；§14 的 CP1 是在**用户明确授权覆盖该门禁**后执行的（§14.0 如实记录，Codex 原裁定未改），其中 §16 的 CP1-REVIEW-FIX-001 已 **ACCEPTED**。✅ **ENV-DIAG-001 ACCEPTED**（Ninja 失败根因 **CONFIRMED**）。⛔ **CP2 = BLOCKED**，`Reason = HOST ENVIRONMENT PATH DESYNCHRONIZATION`，`ARTIFACTS: NONE`，**CP3 NOT AUTHORIZED**。逐项裁定见 §18。Codex 对 `4f754c0` 复核：P0 PATH Guard / ENV-DIAG / CP2 状态命名 / 禁 Flash·禁 CP3 = ✅，**Post-build verifier = CHANGES_REQUIRED（三处）已按 §19 修复（`021fd62`），复核结论待补**。**§20：宿主 PATH 失同步已由方案 1 解决（`PATH_GUARD: OK`）、Ninja 错误未复现，configure PASS、编译 2,494/2,643 后因上游源码缺陷（`learning_screen.cc` 悬空符号）停止 → CP2 仍 BLOCKED，原因已换为 `REPO SOURCE DEFECT`**。**§21：CP2-SOURCE-FIX-001 的 A–D 已完成 —— 源码修复 `2c8f58f`、verifier 精确路径 `exclude_files`、干净重放新树 `src-cp2-003`、manifest 重冻结（`--check` 已 5/5 PASS）、指纹 `eec141fd…`；**E（cold build）待用户在普通宿主终端执行**。**§22：E 首次执行（16:52）构建了错误的树 —— 用户命令漏传 `-Src`、而 runner 的 `-Src` 默认值仍指向旧树 `src`（输入校验 5/5 通过却编译了另一棵树，属真实 harness 缺口）→ 该次运行判定为 **INVALID RUN**；已修复：`-Src` 默认值改为 `src-cp2-003` + 新增 **fail-closed 源树绑定守卫**（实测 `rc=3` 拒建旧树），**待重跑**。**§23：E 第二次执行（17:09）用的树正确（`src-cp2-003`、绑定守卫 OK、输入校验 5/5）且 §20 的编译错误已消失、推进到 2,439/2,643 —— 但撞上 Windows `CreateProcess` 32,767 字符命令行上限（失败命令 33,505）。用真实 `compile_commands.json` 证明：旧命名 `src` 最长 31,759（余量 1,008、超限 0/2,422），我选的 `src-cp2-003` 最长 33,463（**超限 195/2,422**）⇒ 是树目录名太长所致；缩短到 `E:\a05c\s` 可获 4,255 余量。**方案待裁定（改动冻结输入路径，属 CP 级）**。**§24：方案 A 已执行 —— 短路径重放到 `E:\a05c\s`（重放 8 步断言全过、managed_components tree match、A05 `14ffc5c3…`）；权威 `--write` 复测 `isolated_src = 36bc3d4d…`、1353 文件 / 72,457,109 B **与冻结值逐字相同 ⇒ 内容中性**；命令长度投影 33,463 → **28,512（余量 4,255、超限 0/2,422）**；绑定守卫与输入校验（5/5）均 PASS。**§25（17:55）：✅ **CP2 = REVIEW_READY** —— `exit=0`、`Project build complete`、`POST-BUILD VERIFICATION: PASS`、**5/5 构件**、sdkconfig `a901f204…` pre==post、真实分区表 **13 行全 OK**、app **9,271,760 B** / `ota_0` 余量 **165,424 B (1.75%)**、`ota_1` 不 fit（已显式记录）、**NO FLASH**；§9 十七项**全部产出**；另以真实 ELF 关闭"符号保留"未完成项。**CP3（2026-09-16）：✅ 审计完成，待复核 —— 模块矩阵逐行五列（SyncExecutor / SingleFlightHttpTransport / BackendSession = **ELF RETAINED**；TimeAuthority / InteractionArbiter / ReminderCore = **GC_DISCARDED + COMPILED_NOT_WIRED**；ReminderWakePort / SessionLease / SessionSnapshotPublisher = 类型编译，lease 有 ELF 符号、snapshot 全内联）；D5：tick=`kRefreshMs 1000`/窗口 `15000` 且 timer 恰好 2 处删除无重复、`s_selftest` 0 处、文件哈希 `a41aeed7…` 独立于 vendor patch；audio yield 锁收在内层作用域、`pdMS_TO_TICKS(1)` 经**真实编译标志 static_assert** 实测 = **1 tick = 1 ms**（历史"10 ms"描述**作废**）；**Host Gate 重跑 29/29 PASS**（§21/§25 的 28/29 遗留同时闭合）。详见 §26** |
| 工作区 | `E:/claw4-a05-build-m0`（独立克隆，**不在** `E:/workbuddy` 之下，理由见 §1.3） |
| 本地分支 | `workbuddy-a05-build-m0`（**无斜杠**，环境强制；偏差说明见 §1.3） |
| 远端分支 | `workbuddy/a05-build-m0`（与任务书要求**完全一致**） |
| Base（交接 HEAD，含本任务书） | `82337fbf0654b78501241323d6ac4d0f1d7deaeb` |
| 上游代码 SHA | `19fd979d4222093ff4ce7464e5b58407586594a2` |
| vendor pin | `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950` |
| 日期 | 2026-09-14（首版 CP0）；2026-09-15（§12 复审 1 落实；§13 复审 2 修正 = REVISION 2；**同日 CP0 ACCEPTED**） |

**本轮未做**：未改任何代码 / CMake / 配置；未运行 IDF `configure`/`build`；未 flash / erase / monitor；未升级或重下任何组件；未关系统应用控制、未重命名或改写二进制以绕过阻断。

---

## 1. 基线与分支

### 1.1 交接 HEAD 与代码 SHA 的差异核对（任务书 §0 要求）

`82337fb`（`codex/a05-build-task-review`，含本任务书）相对 `19fd979` 的差异**仅文档类**，无源码/配置变化：

```
M  .workbuddy/INSTRUCTIONS.md
M  AGENTS.md
M  README.md
M  docs/project_management/TASK_BOARD.md
A  docs/project_management/references/WB-A05-BUILD-001_ORIGINAL.md
A  docs/project_management/reports/CODEX_A05_PLAN_REVIEW_2026-09-14.md
A  docs/project_management/tasks/WB-A05-BUILD-001.md
```

`19fd979` 是 `82337fb` 的父提交；WorkBuddy 源分支 `workbuddy/v53-next-001-reliability` 本轮**未再推进**（远端仍为 `19fd979`，已复核），因此无需重定基线。

### 1.2 前序提交链（本流）

```
82337fb docs(A05): review local C5 build prerequisites and sequence WorkBuddy checkpoints   <- 交接 HEAD
19fd979 docs(WB-V53-NEXT-001): record final concurrency cleanup evidence
6736fda test(A03): cover final runtime concurrency cleanup
86db779 fix(A03): keep backend counter reads inside state transaction
7b080e0 fix(A03): make diagnostics publish atomic with session ownership
7646c65 docs(WB-V53-NEXT-001): record REVIEW-FIX-002 remediation and final status
7729dbe test(CP5): exercise lock-injected backend production path
88205a9 fix(B03): align reminder wake with calendar and quiet policy
99ea51d fix(A03): close today apply and reauth generation race windows
118f862 docs(WB-V53-NEXT-001): record REVIEW-FIX-001 remediation and corrected conclusions
```

工作树 clean（`git status --short` 空）。CP0 唯一新增文件即本报告。

### 1.3 分支创建：本机 Git 缺陷的完整证据与处置

任务书要求用正常 Git ref 建立 `workbuddy/a05-build-m0`，并授权"若实施环境仍失败，保存错误，不能编辑 refs 文件/强推/改写历史来规避"。实际执行如下。

**第一步（正常命令，在控制仓 `E:/workbuddy/claw4-v53-control-20260913` 内直接尝试）**

```text
$ git worktree add -b workbuddy/a05-build-m0 "E:/workbuddy/claw4-a05-build-m0" 82337fb
Preparing worktree (new branch 'workbuddy/a05-build-m0')
fatal: invalid reference: workbuddy/a05-build-m0        (rc=128)
```

**第二步（`E:/workbuddy/claw4-a05-build-m0`，同一目录，改 Windows 路径）**

```text
$ git branch workbuddy/a05-build-m0 82337fb              # rc=0，无任何输出
$ git rev-parse workbuddy/a05-build-m0
fatal: ambiguous argument 'workbuddy/a05-build-m0': unknown revision   (rc=128)
$ Get-ChildItem .git\refs\heads -Force
codex, main                                              # 没有 workbuddy/ 目录
```

**第三步（穷尽其它正常 git 途径，全部失败）**

| # | 命令 | 结果 |
| --- | --- | --- |
| 1 | `git worktree add -b <slash> <dir> <sha>` | rc=128 `fatal: invalid reference` |
| 2 | `git branch <slash> <sha>` | rc=0，**无引用生成** |
| 3 | `git update-ref refs/heads/<slash> <sha>` | rc=0，无引用生成 |
| 4 | `mkdir .git/refs/heads/workbuddy` 后执行 #2 | rc=0，**刚创建的目录被删除**，无引用生成 |
| 5 | `git fetch <url> <remote>:refs/heads/<slash>` | 打印 `* [new branch] ... -> zzprobe/x`，**实际未写引用** |
| 6 | `git clone --branch <slash> <url> <dir>` | rc=0 + 警告后**目录未生成**；换 Windows 路径则为"Switched to a new branch"但引用缺失、HEAD 悬空 |

**第四步（定位范围）**：在 `E:/` 根目录新建临时仓库并执行同类命令：

```text
$ git init -q ; git commit -qm init
$ git branch sub/test
$ git for-each-ref --format='%(refname)'
refs/heads/master
refs/heads/sub/test            <- 成功创建松散引用
$ ls .git/refs/heads
master  sub\test
```

**第五步（同一命令、换位置，成功）**

```text
$ cd E:\ && git clone --no-hardlinks <url> E:\claw4-a05-build-m0
$ cd E:\claw4-a05-build-m0 && git checkout -b workbuddy/a05-build-m0 82337fb
Switched to a new branch 'workbuddy/a05-build-m0'
$ git for-each-ref --format='%(refname) %(objectname:short)' refs/heads
refs/heads/main 03383db
refs/heads/workbuddy/a05-build-m0 82337fb      <- 创建当时存在（随后被移除，见第六步）
$ Get-Content .git\HEAD
ref: refs/heads/workbuddy/a05-build-m0
```

**第六步（关键补充：在 `E:\` 根创建成功后，斜杠本地引用又被移除）**

第五步的 `refs/heads/workbuddy/a05-build-m0` 曾**确实存在**（当时 `for-each-ref` 列出、`HEAD` 指向它、`git status` clean）。约 10 分钟后复查：

```text
$ cat .git/HEAD
ref: refs/heads/workbuddy/a05-build-m0          <- HEAD 仍指向它
$ ls .git/refs/heads/
main                                            <- workbuddy/ 目录已消失
$ git rev-parse --abbrev-ref HEAD
fatal: your current branch 'workbuddy/a05-build-m0' does not have any commits yet
```

即：**嵌套引用即使创建成功，也会在稍后被移除**（本轮未执行任何删除命令；同一工作区内的普通引用 `main` 与全部工作树文件均完好）。这与第三步第 4 条"预建父目录被删除"是同一现象的不同表现。

**最终处置（前置条件已满足）**

1. 本地分支改用 **无斜杠名** `workbuddy-a05-build-m0`（普通顶层引用，不受该缺陷影响），指向交接 HEAD `82337fb`；恢复过程中先 `git reset` 清空索引、再 `git checkout -f -b workbuddy-a05-build-m0 82337fb`，工作树与 `82337fb` 一致、`git status` 仅剩本报告一个未跟踪文件。
2. 远端分支保持任务书要求的 **`workbuddy/a05-build-m0`**，指向 `82337fb`（普通 push 建立，`git ls-remote` 已复核）。
3. **本地分支名与任务书字面要求不一致**（`workbuddy-a05-build-m0` vs `workbuddy/a05-build-m0`）——这是**环境强制偏差**，与上一流 `WB-V53-NEXT-001` 采用的做法一致（当时 Codex 已按远端分支名完成审查）。**远端名字与任务书完全一致**，Codex 可直接按 `workbuddy/a05-build-m0` 复审。
4. **未使用**被禁止的规避手段：没有编辑任何 refs 文件、没有 `git update-ref`/手工写引用、没有 `--force`/强推、没有 rebase/reset/squash 已审历史、没有改写任何提交。历史链保持只追加（本报告为 `82337fb` 之后的单个提交）。
5. 若 Codex 要求本地也必须使用斜杠名，需要在环境侧解决该缺陷（当前证据表明它作用于 `E:\workbuddy\**` 且对 `E:\` 根也表现为延迟移除），请指定处置口径。

**副产物清理（如实说明）**：定界试验过程产生两处临时物，均已删除——① `E:\e\workbuddy\claw4-a05-build-m0`：由 `/e/...` 形式参数被 Windows 解释为**盘符相对路径**（`E:\e\...`）而产生的残缺克隆骨架（`.git` 骨架、无工作树）；② `E:\tmp\reftest-a05`：第四步的临时试验仓库。

---

## 2. Host 运行证据（同 SHA）

### 2.1 复现命令与结果

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/dev/verify-host-cpp-tests.ps1 `
  -CompilerPath E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe `
  -CrossCompilerPath E:/workbuddy/claw4-idf-tools/tools/riscv32-esp-elf/esp-14.2.0_20260121/riscv32-esp-elf/bin/riscv32-esp-elf-g++.exe `
  -OutputDir out/a05-cp0-host
```

```text
common implementation objects: 18 (compiled once)
unit summary : 29 / 29 PASS
interface: exit=0 (see above; 0 == PASS)
RESULT: NATIVE CPP TEST GATE PASS
```

日志：`E:/claw4-a05-build-m0/out/a05-cp0-host/host_result.txt`（含逐套件输出）。

**与 Codex 独立复跑（24/29）的差异是真实的，且不是代码缺陷**：本轮**单次运行 29/29，未出现任何 `LAUNCH FAIL`**；Codex 那一轮 5 项被 Windows `An Application Control policy has blocked this file` 拦在**进程启动**阶段（不是断言失败）。两侧代码 SHA 相同（`19fd979`），因此该差异只能来自执行环境/策略状态。

### 2.2 针对 Codex 所述 5 个启动阻断项的逐项证据

同 SHA、同编译参数、**各只运行一次（未重试、未制造偶然通过）**：

| 套件 | exe SHA256 | 本轮运行 rc | 输出 |
| --- | --- | --- | --- |
| `outbox_codec_tests` | `747938b567cb07780b6fb111c6a8a357f47f593eae104bebcf889eebcaa5d3ae` | 0 | `outbox_codec_tests: all PASS` |
| `restart_recovery_tests` | `51fdc6a5ebe09de23d6775aa40078f3e9bb89aade4e4e332572c23dd87d6169e` | 0 | `restart_recovery_tests: all PASS (restart->Pause/Resume/Complete OK on host -> defect NOT reproduced at domain level)` |
| `backend_client_tests` | `fa31171b241859ac4d52d9c62d2c6855dbfb0baf4c7bce426c37a30358f8d63e` | 0 | `backend_client_tests: all PASS` |
| `learning_backend_session_tests` | `d101078555989bd6975925f735cb8b0d31a5d343eeeab95ef2a5d1fd2fa33481` | 0 | `learning_backend_session_tests: all PASS` |
| `sync_diagnostics_tests` | `bbde053cdb7cadde6c2d4029cc2e0d528bd159610c195a9605c2d0f622f0f957` | 0 | `sync_diagnostics_tests: all PASS` |

**编译与链接参数（门禁实际用法，逐字摘自 `tools/dev/verify-host-cpp-tests.ps1`）**

```text
实现对象（全门禁只编译一次）：
  <cc> -std=c++17 -Wall -Wextra -Werror -I <repo>/firmware/main -I <repo>/firmware/tests -I <repo>/integration -c <impl.cpp> -o common-<N>.o
单元测试（每套件独立编译+链接）：
  <cc> -std=c++17 -Wall -Wextra -Werror -I <repo>/firmware/main -I <repo>/firmware/tests -I <repo>/integration <test.cpp> <common-*.o> -o <name>.exe
本地编译器：E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe
```

**环境解释（供 Codex 交叉比对）**

1. 该阻断发生在**进程创建**阶段，与测试内容无关；同一 exe 的 SHA256 已给出，若两侧 hash 相同而结果不同，则确定是环境差异（应用控制/EDR/白名单策略状态），而非构建差异；若 hash 不同，则需要先对齐工具链再加比对。
2. 该现象在本项目实施过程中**反复出现且间歇**（本轮之前亦多次观察到"新链接出的 exe 首次执行被拒、稍后或在完整门禁内正常"），与 Codex 观察一致。
3. 我**没有**关闭应用控制策略、**没有**重命名 exe、**没有**改写二进制、**没有**用旧退出码或缓存结果冒充通过；也没有反复重跑以求偶发通过（5 项各只运行一次，全部 rc=0）。
4. 若 Codex 认为需要"同机重复抽样"来定论，请指定抽样口径（次数/时间窗/是否接受一次通过即视为可用），我按口径执行并如实记录失败样本。

**`ENV_VERIFY_REQUIRED`（本轮不改变）**

- **TCP loopback**：本机无法执行任何创建网络 socket 的可执行文件；Host 门禁中的 socket 层为桩，**桩不等于 loopback**。仅列状态，不主张已通过。
- 网络总 deadline、UI 延迟、真 NVS / 真掉电同属设备侧，见 §10。

---

## 3. 套件、门禁覆盖与 CI

### 3.1 套件清单（30 个可执行文件对应 29 个计分套件，`smoke_test` 不参与计数）

```
backend_client_tests            backend_provisioning_tests      backend_session_gate_tests
backend_sync_transport_tests    coordinator_tests               dispatcher_tests
domain_reducer_tests            final_concurrency_cleanup_tests host_funnel_tests
interaction_arbiter_tests       joint_loop_tests                learning_app_glue_tests
learning_backend_session_tests  learning_boot_policy_tests      learning_mcp_host_tests
outbox_codec_tests              outbox_core_tests               ports_contract_tests
presenter_tests                 production_path_gate_tests      reminder_core_tests
restart_recovery_tests          scheduled_http_transport_tests  session_lease_tests
single_flight_http_transport_tests  sync_diagnostics_tests      sync_executor_tests
time_authority_tests            wire_codec_tests                (smoke_test)
```

先前的 `unit summary : 27`（REVIEW-FIX-001）→ `28`（REVIEW-FIX-002 新增 `production_path_gate_tests`）→ **`29`**（FINAL-CONCURRENCY-CLEANUP 新增 `final_concurrency_cleanup_tests`）。

新增/变更测试的实际用例数（本轮实测）：`final_concurrency_cleanup_tests` 6、`production_path_gate_tests` 8、`backend_session_gate_tests` 8、`reminder_core_tests` 21、`coordinator_tests` 46。

### 3.2 交叉编译语法门禁

```text
== 1) learning_domain dependency scan ==   PASS: no forbidden hardware/OS includes (8 headers)
== 2) -fsyntax-only over all interface headers ==   headers  : 49 / 49 PASS
== 2.5) -fsyntax-only over host implementation sources (interaction/mcp) ==  implsrcs : 5 / 5 PASS
== 3) -fsyntax-only over contract tests ==  contract : 1/1 PASS
RESULT: ALL INTERFACE CONTRACT CHECKS PASS
```

覆盖范围说明（与 Codex 一致，本轮复核）：`49` = `firmware/main/**` 全部头文件（含本轮新增的 `sync/session_snapshot_publisher.h`，实测 `find firmware/main -name "*.h" | wc -l` = 49）；`5` = `interaction/mcp` 实现源；`1` = 契约测试。**该脚本不覆盖 `sync` / `reminder` / `time` 的全部实现源，不能替代完整 IDF 编译。**

### 3.3 CI

`CI: NOT PRESENT` —— 仓库不存在 `.github/workflows`，也没有外部 commit status 门禁。本轮一切结论均为 `Local Host Gate`，不写 `CI PASS`。

---

## 4. C5 配置来源与关键值

| 项 | 值 |
| --- | --- |
| **采用候选**（唯一） | `E:/workbuddy/claw4-idf-cold-c5-20260906-frozen-20260912/sdkconfig` |
| SHA256 | `a901f20491671a9cbca8cbe60c4ac4b26d91ac1916d36530b9475b6704fabc26`（**与审查报告期望值一致**） |
| 大小 / mtime | 127,103 B / 2026-08-31 |
| 同 hash 副本 | `E:/workbuddy/claw4-idf-cold-c5-20260906/sdkconfig`（旧 build 目录内，hash 相同） |
| **禁止使用** | `E:/c/sdkconfig` SHA256 `436050e9b95265fa9ad6f806b5006fb52d7cce9d3f8bfef0864b4bfa031fbdec`（H2 配置） |

关键项（从候选配置读取；凭据类键（PASS/PSK/SSID/TOKEN/SECRET/KEY/PASSWORD/USER/API/URL/HOST）**已过滤，未出现在本报告**）：

| 类别 | 值 |
| --- | --- |
| 目标 | `CONFIG_IDF_TARGET="esp32p4"`，`CONFIG_IDF_TARGET_ESP32P4=y` |
| 板型 | `CONFIG_BOARD_TYPE_METALIO_CLAW_4=y` |
| 协处理器 | `CONFIG_ESP_HOSTED_CP_TARGET_ESP32C5=y`；`CONFIG_SLAVE_IDF_TARGET_ESP32C5=y`（H2/H4/C6 等均未选中） |
| Flash | `CONFIG_ESPTOOLPY_FLASHSIZE="32MB"`；`FLASHMODE="dio"`（QIO 使能）；`FLASHFREQ="40m"` |
| PSRAM | `CONFIG_SPIRAM=y`、`MODE_HEX=y`、`SPEED_200M=y`、`XIP_FROM_PSRAM=y`、`FLASH_LOAD_TO_PSRAM=y`、`ALLOW_STACK_EXTERNAL_MEMORY=y` |
| 内核 tick | `CONFIG_FREERTOS_HZ=1000` |
| C++ 运行时 | `CONFIG_COMPILER_CXX_EXCEPTIONS=y`（EMG pool 1024）、`CONFIG_COMPILER_CXX_RTTI=y` |
| 优化 | `CONFIG_COMPILER_OPTIMIZATION_PERF=y`（`-O2` 档），assertion level 2 |
| 分区 | `CONFIG_PARTITION_TABLE_CUSTOM=y`，`FILENAME="partitions/v1/32m_dual.csv"`，`OFFSET=0x9000`，MD5 校验开 |
| Bootloader | `OFFSET_IN_FLASH=0x2000`、`APP_ROLLBACK_ENABLE=y`、`WDT_TIME_MS=9000`、`FLASH_32BIT_ADDR=y` |
| 控制台 | `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`（UART_NUM=-1，secondary 关闭） |
| 崩溃 | `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT=y` |
| 唤醒词模型 | `CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS=y`（其余 WN9 变体未选中） |

**分区表** `E:/c/partitions/v1/32m_dual.csv`，SHA256 `3522639424052778951248d32b02cff0690ddb86d84968b357d43fffc79f2ff6`（**与审查一致**）：

```text
nvsfactory data nvs      0xA000    200K
nvs        data nvs                840K
otadata    data ota                  8K
phy_init   data phy                  4K
model      data spiffs             956K
ota_0      app  ota_0    0x200000     9M      <- 候选写入槽（9 MiB = 9,437,184 B）
ota_1      app  ota_1                4M      <- 既有，双槽 OTA 能力不足，单独记录
resources  data spiffs               4M
factory_test data spiffs           600K
emote      data spiffs               4M
system     data fat                  1M
storage    data fat                  7M
coredump   data coredump            64K
```

CP1/CP2 约束（照此执行，不自行放宽）：ota_0 超 9 MiB **硬停**；ova_1 仅 4 MiB 不足以承载候选 → 明确记为"双槽 OTA 不可用"，不改分区/OTA 策略；只有官方 `idf.py build` 本身成功且 ota_0 符合时，才形成 **application-only** 候选证据。

---

## 5. vendor pin、A01 补丁与本地额外差异

### 5.1 A01 补丁身份（仓库内，未改动）

| 文件 | SHA256 |
| --- | --- |
| `integration/metalio_claw4/patches/project-ca3aa3fa.json` | `93170a69a71dcea40a1f3bde78540c506cdd388a8e092d0e5077a643c6f98790` |
| `integration/metalio_claw4/patches/project-ca3aa3fa.patch` | `58bfbe5266a9fa00980be5e5078b570b9915165e199981e6dd54dcb21aa81937`（与 JSON 内记录一致） |

JSON 记录的 4 文件三态哈希（`upstream_sha256` / `mirror_raw_sha256` / `patched_lf_sha256`）与 pin `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950` 均已在仓库中。

### 5.2 实测：pin 补丁在 `E:/c` 确实生效

对 `E:/c` 当前 4 个文件计算 (raw sha256, LF 归一 sha256)：

| 文件 | raw sha256（实测） | lf sha256（实测） | = `patched_lf_sha256`？ | = `upstream_sha256`(LF)？ |
| --- | --- | --- | --- | --- |
| `main/display/screen/home_screen/home_screen.cc` | `1bb0eef2e03467f2dcf50c648c65193dc5a167913d4658de723680090eba30c8` | `0c99f5ed52d8ff394707b00a5bd7b2987f09dbcc678a0db1abfccacc7fa36f8d` | **YES** | NO |
| `main/CMakeLists.txt` | `9f7098e02694397984e688a8416042c26efdeb9357cbbbba1ecf340d58d35a92` | `9f7098e02694397984e688a8416042c26efdeb9357cbbbba1ecf340d58d35a92` | **YES** | NO |
| `main/display/lv_adapter_display.cc` | `c4aab117ee03f75183a50a9bfbd33843ce7d26db74aa46dcda16f3431408521b` | `e3a66e7613a1f9b3a1a9ac669c5bbe491a801a3968f9aeb449622b38f0f5b26b` | **YES** | NO |
| `main/audio/audio_service.cc` | `3bbea83d4477ad783ae4f064101a6a39d91987d7906ecc34f93be07141696bed` | `f406e12a2fde1209e6e103727d2d0e107b0c41a2ffa8d22346a69d6c66cc58a2` | **YES** | NO |

（`raw` = 磁盘原始字节；`lf` = 将 CRLF 归一为 LF 后；两列均与 A01 JSON 的 `mirror_raw_sha256` / `patched_lf_sha256` **逐字命中**，`upstream_sha256` 均不命中——即补丁确已应用。）

### 5.3 `E:/c` 相对**真实 pin** 的完整差异账目（REVISION 2 更正）

> **更正说明（首版结论有误）**：首版据 `E:/workbuddy/MetalioClaw4-ascii` 与 `E:/c` 的比对，判定"本机没有 pin 的等价副本、无法证明字节级差异"。**该比对本应用 LF 归一化哈希，首版用的是 raw（CRLF）哈希**，而 A01 JSON 里的 `upstream_sha256` 本身就是 **LF 归一化**口径 —— 口径不一致使 4/4 被误判为"不同"。按 LF 口径复算，参考树 4 文件全部命中。更关键的是：**pin 的权威来源本来就在本机**。

| 项 | 实测 |
| --- | --- |
| pin 源路径 | `E:/workbuddy/学习习惯培育AI/vendor/MetalioClaw4` |
| 是否 git 仓库 | **是**（`.git` 存在；git 2.53.0.windows.1） |
| HEAD | `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950` = **pin 本身** |
| 分支 / 工作树 | `main` / **clean**（无未提交改动） |
| commit 标题 | `Merge pull request #22 from vaemc/main` |
| remote | `https://github.com/CloudZao/MetalioClaw4.git` |
| 四文件 LF 哈希 | **4/4 == A01 `upstream_sha256`** ✅ |

**补丁完整重放（本轮独立重做，不引用他人的结论）**

| 步骤 | 结果 |
| --- | --- |
| `project-ca3aa3fa.patch` SHA256 | `58bfbe5266a9fa00980be5e5078b570b9915165e199981e6dd54dcb21aa81937` = A01 记录 ✅ |
| `git apply --check -p1` | rc=0 |
| `git apply` → 4 文件 LF 哈希 | **4/4 == `patched_lf_sha256`** ✅ |
| `git apply -R` → 4 文件 LF 哈希 | **4/4 == `upstream_sha256`**（完全恢复）✅ |

**`E:/c` vs 真实 pin 的逐文件账目**（排除 `.git` / `build` / `managed_components` / 二进制类扩展名）

```text
pin 文件数            : 477
E:/c 文件数           : 552
identical             : 472
different             : 5    -> main/CMakeLists.txt
                               main/audio/audio_service.cc
                               main/display/lv_adapter_display.cc
                               main/display/screen/home_screen/home_screen.cc
                               sdkconfig
only in E:/c          : 75   -> main/learning/**                        70
                               main/display/screen/learning_screen/**    3
                               main/assets/lang_config.h                 1
                               main/mmap_generate_resources.h            1
only in pin           : 0    <- 关键：没有任何 pin 文件在镜像中缺失
```

- `different` 的 5 个文件 = **A01 补丁的 4 个目标文件 + `sdkconfig`**，与 "pin + 补丁重放" 完全吻合（§5.2 已逐字节核对 `patched_lf_sha256`）。
- `only in E:/c` 的 75 个文件**全部已定性**：**73 个 learning 特性文件** + **2 个构建生成头文件**
  - `main/assets/lang_config.h` —— 由 `scripts/gen_lang.py` 生成（脚本内注释自述"output_path 通常是 main/assets/lang_config.h"）
  - `main/mmap_generate_resources.h` —— 由 `scripts/build_default_assets.py:405` / `scripts/spiffs_assets/spiffs_assets_gen.py` 生成 `mmap_generate_<asset>.h`；被 `main/display/lv_adapter_display.cc:14` include
- **`only in pin = 0`** → 镜像未丢失任何上游文件，**无未知漂移**。

→ 结论（更正版）：**"完整镜像相对 pin 的差异"在本机已完全解释，零未知漂移**。CP1 的重放源即 `E:/workbuddy/学习习惯培育AI/vendor/MetalioClaw4` @ `ca3aa3fa`，**不需要 Codex 另行提供来源**。CP1 仍按任务书 §2 的既定路径推进：pin + 已认可 A01 补丁重放 → 再同步 `19fd979` 的镜像与 learning 特性，**不从 `E:/c` 整树继承**。

---

## 6. IDF 与工具链

| 项 | 实测 |
| --- | --- |
| IDF 根 | `E:/workbuddy/esp-idf-5.5.4-ascii` |
| 版本标识 | `tools/cmake/version.cmake`（**不在 IDF 根**）：`IDF_VERSION_MAJOR 5` / `MINOR 5` / `PATCH 4`；文件 SHA256 `ce4b5269c971811c1ace487b053eea4ad1ef1f86e52f4bae1b6e967970643fd2` |
| Git 元数据 | **不存在**（`esp-idf-5.5.4-ascii/.git` 不存在）→ 按审查要求**不伪造 Git SHA**，以路径名（发行包形式，含 `-ascii` 后缀）+ `version.cmake` + 关键工具版本识别 |
| 工具根 | `E:/workbuddy/claw4-idf-tools` |
| 工具族 | `ccache, cmake, esp-rom-elfs, idf-exe, ninja, openocd-esp32, riscv32-esp-elf, riscv32-esp-elf-gdb` |
| CMake | `tools/cmake/3.30.2/bin/cmake.exe` → **3.30.2** |
| Ninja | `tools/ninja/1.12.1/ninja.exe` → **1.12.1** |
| ccache | `tools/ccache/4.12.1` → **4.12.1** |
| 交叉编译器 | `tools/riscv32-esp-elf/esp-14.2.0_20260121/riscv32-esp-elf/bin/riscv32-esp-elf-g++.exe` → `crosstool-NG esp-14.2.0_20260121` / gcc **14.2.0** |
| Python | `python_env/idf5.5_py3.12_env` → **Python 3.12.14** |
| 旧 build cache | `E:/workbuddy/claw4-idf-cold-c5-20260906/CMakeCache.txt`：`CMAKE_HOME_DIRECTORY=E:/c`，SDKCONFIG 指向该 build 目录内文件。**CP1/CP2 不复用该 cache/object/ELF** |

> 注：IDF 版本文件的实测内容为
> ```
> set(IDF_VERSION_MAJOR 5)
> set(IDF_VERSION_MINOR 5)
> set(IDF_VERSION_PATCH 4)
> ```
> 由于该 IDF 树没有 Git 元数据，"5.5.4" 只是 `version.cmake` 的自声明，**不作为提交身份**。

---

## 7. 依赖与组件

### 7.1 `dependencies.lock`

| 项 | 值 |
| --- | --- |
| 路径 | `E:/c/dependencies.lock` |
| SHA256 | `90af1addf9daf8ae1ec3094a798d7ab56cd12a4fef5702696d08a524458f91e1`（**与审查一致**） |
| 大小 / 条目 | 39,731 B / **83 条目**（82 个组件 + `idf`） |

### 7.2 锁文件 ↔ 本地组件的一致性（REVISION 2 已升级为内容校验）

| 检查 | 结果 | 强度 |
| --- | --- | --- |
| 版本一致性（82 个组件 vs `E:/c/managed_components` 各目录 `idf_component.yml`） | **82/82 匹配，0 版本差异** | 记录级 |
| 目录多余性 | **0 个磁盘目录不在锁文件中** | 记录级 |
| 锁文件 `component_hash` vs 磁盘 `<dir>/.component_hash` | **82/82 一致，0 失配** | ⚠️ **仅记录一致** |
| **组件目录内容哈希重算**（IDF 管理器自己的 `hash_dir` + `validate_hash_eq_hashdir`） | **82/82 MATCH，0 MISMATCH，0 ERROR** | ✅ **内容级**（§13.2） |
| 负向对照：翻转 1 bit 后是否被检出 | **被检出**（MISMATCH） | ✅ 证明检查可失败 |
| `idf` 条目 | 无对应 managed_components 目录（外部依赖，正常） | — |

> ⚠️ **口径更正（REVISION 2）**：首版把"锁文件 `component_hash` ↔ 磁盘 `.component_hash` 一致"当作内容证据，**这是错的** —— 两者都是**记录**，一致只能说明记录未被改，**不能证明组件文件未被修改**。本轮补上了真正的**内容级**校验（重算目录内容哈希 + 负向对照），结论由 82/82 内容 MATCH 支撑。详见 §13.2。

→ 审查报告 §6/§2 中"尚未证明 `dependencies.lock` 与历史 C5 `managed_components` 完全匹配"：**在当前"版本 + 内容哈希"两个维度上已证明自洽**。仍未证明的是"这份 `E:/c` 组件集合与历史 C5 构建当时所用的组件集合逐字节相同"——历史构建目录内没有独立的组件清单快照可比对，**仍列为待 Codex 裁决项**（见 §10 第 5 项）。

### 7.3 C5 协处理器组件

| 项 | 值 |
| --- | --- |
| 组件 | `espressif/esp_hosted` |
| 版本 | **2.12.12** |
| 锁内 component_hash | `8ca10092db3e8c8122540d907ab705805a269377d31192f087ba1def2f5f4f91` |
| 磁盘 `.component_hash` | 同上（一致） |
| 目录 | `E:/c/managed_components/espressif__esp_hosted`（含 `idf_component.yml` sha256 `db8b54cb8357bd49b5ff137dd2d1a643b184875ca271a6efe956ee2ccfa1eff9`） |

### 7.4 资源 / 模型 / 自定义唤醒词

| 项 | 值 |
| --- | --- |
| 唤醒词模型 | `E:/c/wakeword/srmodels.bin`（292,606 B）；配置选中 `CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS=y` |
| 预置二进制 | `E:/c/esp_claw_bin/`：`edge_agent_0xb00000.bin`(3,697,408) / `emote_assets_0x1396000.bin`(2,484,870) / `storage_0x1896000.bin`(7,340,032) / `system_0x1796000.bin`(1,048,576) / `README.md` |
| 资源 | `E:/c/sd_images/` 36 文件（`.eaf` 表情等）；`E:/c/secondary_screen/` 2 文件；`E:/c/images/` 3 文件 |
| 来源与升级 | 全部为本地既有内容，**未重下、未升级**；如需逐文件 hash 清单请指示口径（CP4 交付时会给出受控源码/资源 hash 全表） |

---

## 8. 模块矩阵（CP1 登记用）

| 模块 | 源（仓库 `firmware/main/…`） | 当前设备调用点 | 需要的 CMake 项 | 后续接线边界 |
| --- | --- | --- | --- | --- |
| SyncExecutor | `sync/sync_executor.{h,cpp}` | **已接线（链路逐跳可查）**：`learning_runtime.h:95-96` 持有 `claw4::sync::StateLockFn/StateUnlockFn`（类型来自 `sync_executor.h`）→ `learning_runtime.cpp:35-36` 绑到 `state_mutex_` → `learning_runtime.cpp:112` `backend->runOnlineCycle(state_lock_, state_unlock_)` → `learning_backend_session.h:88` → `learning_backend_session.cpp:283` **构造 `SyncExecutor executor(app_, sync_transport_, lock, unlock, …)`** | SOURCES 增加 `learning/sync/sync_executor.cpp`；头文件已在 `learning/` include 路径下 | **必须**给出 Compiled + Referenced + 最终 ELF 保留 证据 |
| SingleFlightHttpTransport | `sync/single_flight_http_transport.{h,cpp}` | **已接线（REVISION 2 更正）**：`integration/metalio_claw4/device/ports/metalio_http_transport.cpp:9` include 其头；**`:52` 直接 `std::make_unique<claw4::sync::SingleFlightHttpTransport>(PerformRequest)`**；该传输经 `CreateMetalioHttpTransport()` → `learning_runtime.cpp:65` 注入 `LearningBackendSession`，由 `BackendClient` 承载每一次 HTTP 调用 | SOURCES 增加该 cpp | **与 SyncExecutor 同列**：必须证明 Compiled + Referenced + 最终 ELF 保留。**不允许**以 COMPILED_NOT_WIRED / GC_DISCARDED 通过验收 |
| LearningBackendSession / BackendSession 编排 | `sync/learning_backend_session.{h,cpp}`、`sync/backend_client.*`、`sync/outbox_core.*`、`sync/wire_codec.*` | `learning_runtime.{h,cpp}`（`backend_holder_`）、`metalio_http_transport` | **已在**上游 SOURCES | 已接线 |
| SessionLeaseHolder | `sync/session_lease.h` | `learning_runtime.h:89`（`SessionLeaseHolder<LearningBackendSession>`） | 头文件（模板实例化在设备 TU 内） | 头文件型，类型可用即达 |
| SessionSnapshotPublisher | `sync/session_snapshot_publisher.h` | `learning_runtime.cpp:19/120`（`publishSessionSnapshot`） | 头文件 | 同上（纯模板） |
| TimeAuthority | `time/time_authority.{h,cpp}` + `ports/clock_port.h` | **无设备调用点**（`grep` 在 `integration/` 零命中） | SOURCES 增加 `learning/time/time_authority.cpp`；**镜像目录 `learning/time/` 尚不存在，需新建** | 预期 COMPILED_NOT_WIRED，允许被 GC 裁剪 |
| InteractionArbiter | `interaction/interaction_arbiter.{h,cpp}` | **无设备调用点** | SOURCES 增加 `learning/interaction/interaction_arbiter.cpp` | 同上 |
| ReminderCore | `reminder/reminder_core.{h,cpp}` | **无设备调用点** | SOURCES 增加 `learning/reminder/reminder_core.cpp`；**镜像目录 `learning/reminder/` 尚不存在，需新建** | 同上 |
| ReminderWakePort | `ports/reminder_wake_port.h` | 无设备实现（按任务书，设备实现留 C03/E01） | 头文件 | 纯接口，不要求独立符号 |

**REVISION 2 复查说明**：首版对 SingleFlight 的判定是错的，根因是 CP0 时的 `grep … | head -20` **被截断**，`integration/` 的命中没进输出。本轮对本表全部模块做了**无截断全量复查**（逐模块 `grep -rl` 覆盖 `integration/**` 全部 `.h/.cpp`）：

| 模块 | `integration/` 命中文件数 | 判定 |
| --- | --- | --- |
| `single_flight_http_transport`（含类型名） | 1（`device/ports/metalio_http_transport.cpp`） | **已接线**，须证明 ELF 保留 |
| `sync_executor.h`（StateLockFn/UnlockFn） | 1（`device/app/learning_runtime.h`） | 已接线 |
| `session_lease.h` / `SessionLeaseHolder` | 1（`device/app/learning_runtime.h:90`） | 已接线 |
| `session_snapshot_publisher.h` / `publishSessionSnapshot` | 1（`device/app/learning_runtime.cpp:120`） | 已接线 |
| `time_authority` / `TimeAuthority` | **0** | COMPILED_NOT_WIRED 成立 |
| `interaction_arbiter` / `InteractionArbiter` | **0** | COMPILED_NOT_WIRED 成立 |
| `reminder_core` / `ReminderCore` | **0** | COMPILED_NOT_WIRED 成立 |
| `reminder_wake_port` / `ReminderWakePort` | **0** | 纯接口，不要求符号 |

上游 CMake 现状（`E:/c/main/CMakeLists.txt`，552 行，A01 补丁后）：

- `set(SOURCES …)` 中已有 `learning/{learning_domain,sync,application,interaction,mcp,ui,metalio_claw4/device/…}` 条目（10 条 learning 源）。A05 需**追加 5 条**：`sync_executor.cpp`、`single_flight_http_transport.cpp`、`time/time_authority.cpp`、`interaction/interaction_arbiter.cpp`、`reminder/reminder_core.cpp`。
- include：第 147 行 `list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/learning)` → 头文件按 `learning/<子目录>/` 镜像即可被 `#include "sync/…"` 解析；`learning/time/`、`learning/reminder/` 需随源一起镜像。
- 镜像现状：`E:/c/main/learning/**` 共 70 文件，**无 `time/`、无 `reminder/`**。
- 不采用 dummy 调用 / whole-archive / 强制保留符号 / 关闭节裁剪。

---

## 9. 拟修改文件、隔离目录与磁盘

### 9.1 拟修改文件（严格按任务书 §2 白名单，CP1 起生效）

```text
integration/metalio_claw4/patches/           新增 A05 补丁 + 元数据（记录 base / 应用顺序 / 前后 hash / 回滚）
integration/metalio_claw4/integration_manifest.md
tools/dev/                                   本任务的构建、清单与验证脚本
docs/project_management/reports/WB_A05_BUILD_001_REPORT.md
（隔离 src 副本内）main/CMakeLists.txt       仅追加源/必要 include 与已存在依赖登记，由 A05 补丁表达
```

**不改**：官方只读 vendor、`E:/c`、旧 build、历史冻结目录、`partition`/bootloader/OTA/eFuse/BSP/sdkconfig 策略。

### 9.2 隔离目录（短 ASCII，均在 `E:\` 根，避开 §1.3 的引用写入缺陷与 `E:/c` 禁区）

| 用途 | 路径 | 当前状态 |
| --- | --- | --- |
| **A05 Git 工作区**（本轮已建） | `E:/claw4-a05-build-m0` | 已创建，本地分支 `workbuddy-a05-build-m0` @ `82337fb`，工作树 clean |
| **CP1/CP2 IDF 隔离根（建议）** | `E:/claw4-a05-19fd979/`（下含 `src`、`build`） | **空闲未占用** |
| 备选 | `E:/claw4-a05-82337fb/` | 空闲未占用 |

命名取 `19fd979`（受控**代码** SHA）以免与文档提交 `82337fb` 混淆；若 Codex 希望改用分支短 SHA，请在放行时明确。

### 9.3 磁盘与复制方式

- 磁盘：`E:` 985 GB 总 / 已用 387 GB / **可用 598 GB（40% 使用）**；当前 IDF 隔离根与 `build` 尚未占用。
- 复制方式（CP1 起）：**不从 `E:/c` 整树继承**。按 `pin + 已认可 A01 补丁重放` → 同步 `19fd979` 的 `firmware/main` 镜像（含本轮新增 `sync/`、`time/`、`reminder/` 目录）→ 复制候选 C5 配置到**独立 SDKCONFIG 副本**并显式传入 → 不复用旧 CMakeCache/object/ELF。
- 复制工具：`robocopy /E /COPY:DAT`（不做跨树硬链接）；源/目的均不含凭据写回，不触碰 `E:/c` 原文件（只读读取）。

---

## 10. 未解决项与需 Codex 决定（不自行跨越）

| # | 事项 | 现状 | 需要的决定 |
| --- | --- | --- | --- |
| 1 | **前序 WB-V53-NEXT-001 的最终验收** | 看板仍为 `REVIEW_READY`；本轮 CP0 补齐了 Host 证据与 5 项阻断的同 SHA 运行证据 | 是否记录 `ACCEPTED` |
| 2 | **5 项启动阻断的判定口径** | 本轮 29/29、5 项各单次运行均 rc=0；Codex 那轮 24/29 | 是否需要"同机重复抽样"及抽样口径；是否接受"环境间歇阻断 + 同 SHA 可运行证据"作为收口 |
| 3 | **pin 的可获取来源** → **已解决（REVISION 2）** | pin 源就在本机：`E:/workbuddy/学习习惯培育AI/vendor/MetalioClaw4`，HEAD == `ca3aa3fa`、工作树 clean、remote `CloudZao/MetalioClaw4.git`；补丁前向/反向重放 4/4 命中；`E:/c` 相对 pin **零未知漂移**（§5.3）。首版"本机无 pin 元数据"是 raw/LF 哈希口径混用造成的误判 | 不再阻塞。仅需确认接受该本地路径作为 CP1 的重放源 |
| 4 | **本地斜杠引用缺陷的处置** | 6 条正常 git 途径均无法落地斜杠本地引用；`E:\workbuddy/**` 下静默失败，`E:\` 根下**创建成功但稍后被移除**（§1.3 第六步）。故本地用无斜杠名、远端用要求名 | 是否接受该偏差（远端名与任务书一致）；若要求本地也必须斜杠名，需指定环境处置口径 |
| 5 | **组件内容一致性** → **已补证（REVISION 2）** | 首版只用「lock 的 `component_hash` ↔ 磁盘 `.component_hash`」比对，那只是**记录一致**、不能证明文件未被改。本轮改用 **IDF 组件管理器自己的算法重算目录内容哈希**（`idf_component_tools.hash_tools.hash_dir` + `validate_hash_eq_hashdir`）：**82/82 MATCH、0 MISMATCH、0 ERROR**，并有**负向对照**证明该检查能抓到单比特篡改（§13.2） | 仅剩「与历史 C5 构建当时所用集合是否逐字节相同」无独立快照可比（内容完整性本身已证） |
| 6 | **C5 配置是否随候选冻结为副本** | 候选配置已定位并核 hash；本轮 Codex 已确认「C5 配置冻结」安排合理 | CP1 使用的 SDKCONFIG 副本命名与是否纳入 manifest hash（细节） |

---

## 11. 状态声明

- 本轮**只完成 CP0**（前置验收证据与构建输入盘点）。**未运行任何 IDF configure/build**，未产出任何候选产物，**未 Flash**，未接提醒硬件，未扩展业务。
- **验收与复审记录（准确口径）**
  - **复审 1**（2026-09-15）：Codex 认为**任务书这一版总体可执行、CP0 可以启动**，并确认「C5 配置冻结 / 未接线模块允许裁剪 / 禁止 Flash / WorkBuddy 实施而 Codex 审查」这些安排合理；同时要求放行 CP1 前补清 3 处执行规则（已落成 §12）。
  - **复审 2**（2026-09-15）：Codex 未作出验收结论，并指出本报告 4 处需修正（①验收状态表述错误 ②pin 上游来源并非缺失 ③组件内容一致性证据不足 ④SingleFlight 验收被错误放宽）。本轮已逐条修正（§13）。
  - ✅ **CP0 验收通过**（2026-09-15，Codex 复核提交 `6d49c72`）：上轮 4 项均已修正；Codex **独立重算 82 个组件的实际内容哈希，82/82 通过**；上游来源与补丁证据明确；SingleFlight「必须实际链接」的要求已修正。**CP0 至此 ACCEPTED。**
  - ⚠️ **CP1 仍未放行**：剩余前置是 **前序 `WB-V53-NEXT-001` 的整体代码验收**尚未完成。Codex 明确「无需重复整改 CP0，暂不开始构建」。
- **CP1～CP4 未开始**，等待放行。原「pin 的可重放来源」阻塞已消除（§5.3、§13.1）。
- 状态：**`CP0 ACCEPTED` / `CP1 BLOCKED`**（唯一阻塞项：前序流整体代码验收）。放行条件未满足前我停下 —— **不自行跨门禁、不开始构建**。

**前序流验收所需材料索引（供 Codex 复核；本轮不新增任何整改）**

| 项 | 位置 |
| --- | --- |
| 分支 / tip | `workbuddy/v53-next-001-reliability` @ `19fd979`（远端 `github.com/revercgy-hub/claw4-learning-habit-ai.git`） |
| 主报告 | `docs/project_management/reports/WB_V53_NEXT_001_REPORT.md`（含 `## REVIEW-FIX-001`、`## REVIEW-FIX-002`、`## FINAL-CONCURRENCY-CLEANUP`） |
| 末轮 Host Gate 证据 | 结果：**29/29 PASS + `interface: exit=0` + `RESULT: NATIVE CPP TEST GATE PASS`**。⚠️ **原始日志未入库**（`/out/` 在 `.gitignore`，`git ls-files out` = **0** 条），仅本机留档：`E:/workbuddy/claw4-wb-v53-next-001/out/fcc-final/host_result.txt`（前序流工作区）与 `E:/claw4-a05-build-m0/out/a05-cp0-host/host_result.txt`（本工作区，同一代码 SHA `19fd979`）。复现方式：在任一工作区运行 `tools/dev/verify-host-cpp-tests.ps1` |
| 关键新增测试 | `firmware/tests/unit/v53/{production_path_gate_tests,final_concurrency_cleanup_tests,backend_session_gate_tests}.cpp` |
| 未决/限制 | 同报告 §15 末段：镜像 CMake 未登记、TCP loopback `ENV_VERIFY_REQUIRED`、总 deadline 未解决、触控 P95 等 `HARDWARE_VERIFY_REQUIRED` |

**继续保持的状态标记**

- `TCP loopback: ENV_VERIFY_REQUIRED`
- `CI: NOT PRESENT`
- `Local Host Gate: PASS（29/29）`
- Device / Hardware 项（见下）一律 `HARDWARE_VERIFY_REQUIRED`

---

## 12. 复审意见落实：3 处执行规则的精确判据（CP1 放行前置）

Codex 复审 1（2026-09-15）：**任务书这一版总体可执行、CP0 可以启动**（⚠️ 该表述**不构成验收结论**）；并确认「C5 配置冻结、未接线模块允许裁剪、禁止 Flash、WorkBuddy 实施 Codex 审查」这些安排合理。放行 CP1 前需补清 3 处执行规则 —— 下面把 3 条落成**可检查判据**，并给出**可直接采纳的替换措辞**。复审 2 另指出 4 处报告缺陷，已在本报告 §13 修正。

> 合规说明：本轮仍处 CP0 约束内——**只新增/补充本报告文件**，未改代码/配置/CMake，未运行 IDF configure/build，未 Flash。任务书本体在 `codex/a05-build-task-review` 分支，**我不单方面改写它**；§12.1–§12.3 末尾的引用块即逐字可用的替换文本，交由 Codex 决定采纳。

### 12.1 规则一：界定「所有输入已提交」的范围（任务书 L46）

**问题**：SDKCONFIG、IDF 树、`managed_components/`、隔离源码副本都在仓库外，要求「全部入 Git」既不现实也不该做。原措辞会同时导致两种误判——把仓库外输入当成"未提交"而卡住，或把仓库外输入当成"不需要冻结"而放松。

**判据（两类输入，两套口径）**

| 类别 | 具体输入 | 要求 | 判定命令 |
| --- | --- | --- | --- |
| **A. 仓库内输入** | `firmware/**`、`integration/**`（含 `patches/`、`integration_manifest.md`）、`tools/dev/**`、`docs/**` | **必须已提交且 clean** | `git status --porcelain` 输出为空 |
| **B. 仓库外输入** | IDF 树；工具链根；`managed_components/`；隔离源码副本（pin+patch 重放结果）；SDKCONFIG 副本；资源/模型/自定义唤醒词；Python 环境 | **不要求入 Git**，但必须由**提交进仓库的 manifest** 按「来源 / 版本 / 哈希」冻结，构建前逐项复验 | manifest 复验脚本，全部命中才继续 |

**关键约束（B 类不得被当成"无需证明"）**

- B 类每一项必须在 manifest 里落 **来源路径或 URL + 版本 + 哈希**；哈希口径二选一并写明：文件级 SHA256，或目录级 tree-hash（排序后「相对路径 + 文件 SHA256」清单再 SHA256）。
- 构建前必须**重新计算**并与 manifest 比对；任一不一致 → **停止并报告**，不自动修正、不自动重新下载。
- manifest 自身提交进仓库，其 SHA256 即 CP4 交付项里的「构建输入 manifest SHA」。
- B 类里的 **SDKCONFIG 必须显式传入**（不从默认位置隐式读取），configure 前后各记一次哈希（任务书 L48 已要求）。
- A 类里 **官方只读 vendor 树不改**；隔离源码副本属 B 类，其 `main/CMakeLists.txt` 的追加由新增 patch 表达（任务书 L30/L32 已要求）。

**拟采纳的替换措辞（任务书 L46 首句）**

> 构建输入分两类，判定口径如下：
> (a) **仓库内输入**（`firmware/**`、`integration/**`、`tools/dev/**`、`docs/**` 及 A05 补丁层）——必须已提交且 `git status --porcelain` 为空（dirty=false），再构建。
> (b) **仓库外输入**（IDF 树、工具链、`managed_components/`、隔离源码副本、SDKCONFIG 副本、资源/模型/唤醒词、Python 环境）——**不要求入 Git**，但必须由提交进仓库的 `build_input_manifest` 按「来源/版本/哈希」冻结，并在构建前逐项复验通过；任一不一致即停止并报告，不自动修正或重下载。

> 本轮已完成的等价盘点见 §4（SDKCONFIG 三候选与关键值）、§6（IDF 与工具链）、§7（依赖/组件/资源）、§5（pin 与补丁身份）。这些内容即是 A05 的 manifest 初稿来源。

### 12.2 规则二：Host 证据复用改按「指纹」判定，而非「同 SHA」（任务书 L64）

**问题**：CP1 会改 CMake 与文档，提交 SHA 必然变化，「同一 SHA」字面不可能满足；而 SHA 相同也不等于 Host 门禁输入相同（工具链可能换）。两个方向都不严谨。

**判据：Host 复用指纹（Host Reuse Fingerprint）**

定义（**合成方式已定死为规范化 JSON，避免"等价实现算出不同值"**）：

```python
# 1) 文件树哈希
rows = [(相对路径(用 / ), sha256(文件)) for 文件 in firmware/**, integration/**, tools/dev/**
        排除 out/、*.o、*.exe、__pycache__、.git]
rows.sort(key=相对路径)
trees_hash = sha256("\n".join(f"{h}  {relpath}" for relpath, h in rows))   # 两个空格分隔

# 2) 参数哈希（固定字面量，见下行 ARGS）
args_hash = sha256(json.dumps(ARGS, sort_keys=True, separators=(",", ":")))

# 3) 合成指纹
canonical_json = json.dumps(
    {"args": args_hash,
     "cc": {"native": sha256(原生 g++ 二进制), "cross": sha256(交叉 g++ 二进制)},
     "trees": trees_hash},
    sort_keys=True, separators=(",", ":"))
FINGERPRINT = sha256(canonical_json)
```

```python
ARGS = {"compile": "-std=c++17 -Wall -Wextra -Werror -I firmware/main -I firmware/tests -I integration",
        "link":    "-std=c++17 -Wall -Wextra -Werror -I firmware/main -I firmware/tests -I integration <objs...>",
        "gate_script":      "tools/dev/verify-host-cpp-tests.ps1",
        "interface_script": "tools/dev/verify-interface-contracts.ps1"}
```

要点：`sort_keys=True` + `separators=(",", ":")`（无空格）保证同一个字典在任何实现下产生**字节相同**的 `canonical_json`。键名固定为 `args` / `cc.cross` / `cc.native` / `trees`。

文件集取 `firmware/** + integration/** + tools/dev/**` 是**门禁实际编译范围的超集**（门禁编 `firmware/main/{learning_domain,sync,application,ui,interaction,mcp,ports,time,reminder}` + `firmware/tests` + `integration/metalio_claw4/{host_glue,device/core}` 与两个 `tools/dev/*.ps1`）。取超集的好处：只可能更严格，不会漏判。

**本轮实测基线（同一代码 SHA `19fd979` 对应的 CP0 提交）**

| 项 | 值 |
| --- | --- |
| **HOST_REUSE_FINGERPRINT** | `133ffedba3f9195cc1d16732e42fc582adc2c7593f8be8f766da2f74308c70b4` |
| 文件数 | 156 |
| `trees_hash` | `b88d50440a30d3ffbb4e88031be9980f7bcc1b9cddbf4e5fc700573f368edce1` |
| `args_hash` | `69018d2e4a47283164cde8a3540e9f3992c0af40f053f43471b36dae4a066c1f` |
| `canonical_json` | `{"args":"69018d2e…","cc":{"cross":"8dc9eeb8…","native":"4f40f17d…"},"trees":"b88d5044…"}`（完整值见 `host-fingerprint.json`） |
| 幂等性验证 | 同一规范连算两次 → 字节一致 `True`；与声明基线比对 → `True` |
| 原生 g++ | `E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe` sha256 `4f40f17d8a5a1052b83de568fe2e951fa37a3a21c38676e5b1da5707029cce61`；`g++.exe (GCC) 16.2.0` |
| 交叉 g++ | `E:/workbuddy/claw4-idf-tools/tools/riscv32-esp-elf/esp-14.2.0_20260121/riscv32-esp-elf/bin/riscv32-esp-elf-g++.exe` sha256 `8dc9eeb8a7f3908baf0e7a7b2374dae7fe830dc79d005454136c3f8689905687`；`crosstool-NG esp-14.2.0_20260121) 14.2.0` |

> **本轮自查抓到一个真实的规范缺陷（已修正，如实记录）**：第一版我写的是「`SHA256(trees_hash ‖ 编译器哈希集合 ‖ args_hash)`」，**没有规定序列化方式与键名**。用一个"等价实现"复算时（键名写成 `native`/`cross` 而非 `native_gpp`/`cross_gpp`，且未固定 JSON 空白）算出了**不同的指纹** `42f3c9ccfb0ba820e30bd1c6fe3fde05ffd0803199746398b0b734ba3c126691`，而 `trees_hash` 两次**完全相同**——说明差异纯粹来自合成方式未定死。这正是规则二本身要防的失效模式：**判据若不精确到可字节复现，它自己就会成为新的争议源**。现已改为上面的规范化 JSON 定义，并完成幂等性验证（连算两次一致、与声明基线一致）。

**复用条件（全部满足才可复用本轮 CP0 门禁日志）**

1. 指纹逐字段相同（`trees_hash`、两个编译器 SHA256、`args_hash`）；
2. 报告中记录两个提交之间的 `git diff --name-status <CP0-SHA> <CP1-SHA>`，且**差异文件全部落在指纹集合之外**（即只动 `docs/**`）；
3. 指纹集合内**任一文件**变化 → 必须重跑全量 Host Gate，不可复用。

**本轮实操示例（规则二在引入它的这条链上就跑通了）**

- 两个提交：`96fc0d6`（CP0 首版）→ 本规则的澄清提交
- `git diff --name-status 96fc0d6 HEAD` = `M docs/project_management/reports/WB_A05_BUILD_001_REPORT.md`
- 差异文件**落在指纹集合之外** → 指纹未变（`trees_hash` 在加入 §12 前后两次重算均为 `b88d5044…`，`args_hash` 均为 `69018d2e…`）
- 判定：**CP0 的 Host 门禁日志在本提交下仍可复用**，无需重跑
- 同时验证了反向：`git diff` 内不含 `vendor/`、`sdkconfig`、`CMakeLists`、`partition`、`bootloader`、`.bin` 的任何改动

即：纯文档提交不会使既有 Host 证据失效 —— 这正是要解决的原歧义。

**刻意排除在指纹之外的东西（这是设计意图，不是遗漏）**

- `docs/**` —— 文档提交不应使既有门禁证据失效；
- 镜像的 `E:/c/main/CMakeLists.txt` 等**仓库外** CMake —— Host 门禁根本不编译镜像，改它不影响 Host 结果（其影响由 CP2/CP3 的构建证据承担）；
- `out/` 构建产物 —— 非输入。

**拟采纳的替换措辞（任务书 L64 后半句）**

> Host 证据能否复用，**不看提交 SHA，看 Host 复用指纹**：指纹按本节上方的规范化 JSON 定义计算（文件集 `firmware/** ∪ integration/** ∪ tools/dev/**` + 原生与交叉编译器二进制哈希 + 编译链接参数），**须能字节级复现**。指纹相同即可复用本轮 CP0 的门禁日志；指纹不同必须重跑。复用与重跑均须在报告中记录：两提交间的 `git diff --name-status`、指纹比对结果、以及差异文件是否全部落在指纹集合之外。产品源码有任何改动 → 指纹必变 → 必须重跑。

> 指纹基线现由一次性等价计算取得（`E:/workbuddy/claw4-a05-cp0-recon/host-fingerprint.json`）。CP1 获准写 `tools/dev/` 后，我会把它固化为 `tools/dev/host-reuse-fingerprint.ps1`，使后续复算可一键重放。

### 12.3 规则三：分区表必须从**生成物**反解核验 + 余量只能来自本候选（任务书 L50）

**问题**：原文只约束输入 CSV 与标称容量。但 CSV 里有**空白偏移**（`ota_1` 的 offset 就是空的，靠自动计算），"布局与批准输入一致"若不从生成的 bin 反解就只是假设。

**判据：解析生成物，逐行比对，再算余量**

工具与命令（用 IDF 自带、树内工具即可，已实测往返可用）：

```
<python> esp-idf-5.5.4-ascii/components/partition_table/gen_esp32part.py \
         --flash-size 32MB  <build>/partition_table/partition-table.bin  actual.csv
```

- 工具身份：`gen_esp32part.py` sha256 `8f0f2b4eeb42254d5ce3490260e43832f951d3c9930935f035a4adf831ad5be3`（IDF 5.5.4 发行包内，`--verify` 默认开启，`--flash-size` 做容量适配检查）。
- 同样方式解析**已批准输入**，取得期望布局，使两者可比（输入 CSV 的空白偏移会被解析为确定值）。

**已批准输入 `E:/c/partitions/v1/32m_dual.csv` 的期望解析结果（本轮实测固化）**

- CSV 文件哈希：`3522639424052778951248d32b02cff0690ddb86d84968b357d43fffc79f2ff6`
- 归一化布局表哈希：`00c12fe55d034a06b70d28b770d682e3e580ecd7a42fc2267972a1dac755929f`
  （归一化 = 按行 `name|type|subtype|0x偏移|字节数|flags` 拼接后 SHA256）

| name | type | subtype | offset | size (bytes) |
| --- | --- | --- | --- | --- |
| nvsfactory | data | nvs | 0x0000a000 | 204,800 |
| nvs | data | nvs | 0x0003c000 | 860,160 |
| otadata | data | ota | 0x0010e000 | 8,192 |
| phy_init | data | phy | 0x00110000 | 4,096 |
| model | data | spiffs | 0x00111000 | 978,944 |
| **ota_0** | **app** | **ota_0** | **0x00200000** | **9,437,184** |
| **ota_1** | **app** | **ota_1** | **0x00b00000** | **4,194,304** |
| resources | data | spiffs | 0x00f00000 | 4,194,304 |
| factory_test | data | spiffs | 0x01300000 | 614,400 |
| emote | data | spiffs | 0x01396000 | 4,194,304 |
| system | data | fat | 0x01796000 | 1,048,576 |
| storage | data | fat | 0x01896000 | 7,340,032 |
| coredump | data | coredump | 0x01f96000 | 65,536 |

- 末段结束 = `0x01fa6000`（33,185,792 B）；32MiB = `0x02000000`（33,554,432 B）→ **尾部未分配 0x5a000 = 368,640 B（360 KiB）**。这个数只能由解析结果算出，不能凭 CSV 目测。

**必须的断言（任一不通过即硬停）**

1. `ota_0.offset == 0x00200000`，`ota_0.size == 9437184`（9MiB）
2. `ota_1.offset == 0x00b00000`，`ota_1.size == 4194304`（4MiB）
3. 生成物解析出的**完整布局**逐行等于上表（含顺序、类型、子类型、flags），且归一化哈希 == `00c12fe5…`
4. `--flash-size 32MB` 适配检查通过；无重叠、无越界
5. 生成 `partition-table.bin` 的 SHA256 与输入 CSV 的 SHA256 均入报告

**候选余量的唯一定义（禁止引用历史余量）**

```
ota_0_余量_字节 = 9437184 − <本候选 app 镜像字节数>        （app 镜像取构建报告的实际 size，含对齐/校验）
ota_0_余量_百分比 = ota_0_余量_字节 / 9437184
```

**双槽 OTA 可用性（由数据判定，不由假设）**

`ota_1` 仅 4,194,304 B，**小于** `ota_0` 的 9,437,184 B。因此：

- 若本候选 app 镜像 > 4,194,304 B → **必须显式记录「双槽 OTA 不可用」**，只提交 application-only 构建证据，不改分区表、不改 OTA 策略；
- 若 app 镜像 ≤ 4,194,304 B → 双槽可用性成立，仍需记录实测数值。
- app 镜像 > 9,437,184 B → **超过 ota_0 硬停**，不得跳过尺寸检查、不得手改分区。

**拟采纳的替换措辞（任务书 L50 首句前插入）**

> 分区表必须**从生成的 `partition-table.bin` 反解核验**，不能只核输入 CSV：用 IDF 自带 `gen_esp32part.py --flash-size 32MB <生成BIN> <输出CSV>` 解析实际生成结果，逐行比对（name/type/subtype/offset/size/flags）与已批准输入的解析结果一致，并显式断言 `ota_0.offset==0x200000`、`ota_0.size==9437184`、`ota_1.offset==0xb00000`、`ota_1.size==4194304`。**候选余量只能由本次候选的 app 镜像实际大小计算**（ota_0 余量 = 9437184 − 本候选 app 镜像字节数），不得引用任何历史余量；app 超 ota_0 立即硬停。因 `ota_1`(4MiB) < `ota_0`(9MiB)，若本候选 app 镜像 > 4194304 B 必须显式记录「双槽 OTA 不可用」，只提交 application-only 证据，不改分区或 OTA 策略。

**顺带发现（供 Codex 决定，不自行处理）**：输入 CSV 里 `ota_1` 的 offset 为**空白**、由 `gen_esp32part.py` 自动接续得到 `0x00b00000`。这与任务书写的 `ota_1 offset0xb00000` 一致，但**该值来自工具自动计算而非显式声明**。是否要求在 A05 的隔离源码副本中把该 offset 显式写死，请裁定；我倾向**保持与原 CSV 逐字节一致**（改 CSV 等于改已批准输入，超出本包范围）。

### 12.4 三条规则与本轮状态

- 本轮**未修改任何代码、配置、CMake、分区表或脚本**；只补充本报告文件（CP0 白名单）。§12.1–§12.3 的替换措辞未应用到任务书，等 Codex 采纳。
- 三条规则所需的前置数据**本轮已全部实测取得**：输入分类清单（§4–§7）、Host 指纹基线（§12.2）、期望分区布局与其归一化哈希（§12.3）。
- **规则二已在本轮自我验证**：补充 §12 属纯文档改动，改后重算指纹 —— `trees_hash` 与 `args_hash` **均不变**，即"文档提交不使既有门禁证据失效"这一设计意图成立，规则二可直接用于本包后续文档提交。
- 因此 **CP1 不需要额外等待新的取证**；只等规则采纳与下列放行项。

### 12.5 CP1 放行的剩余依赖（除上述 3 条规则外）

| # | 依赖 | 状态 |
| --- | --- | --- |
| 1 | 前序 WB-V53-NEXT-001 记 `ACCEPTED` | 仍待；看板现状 `REVIEW_READY`（§10 第 1 项） |
| 2 | ~~pin 的可重放来源~~ | ✅ **已消除**（复审 2 指出：来源本来就在本机）—— `E:/workbuddy/学习习惯培育AI/vendor/MetalioClaw4` @ `ca3aa3fa`，重放已验证（§5.3、§13.1） |
| 3 | 3 条规则的采纳形态 | 待定：直接改任务书，或我在 CP1 报告中以「执行规则声明」逐条落地并引用 §12 |
| 4 | 复审 2 的 4 项修正 | ✅ 本轮已完成（§13），等待复核 |

→ 现在**不存在任何取证型阻塞**。CP1 只等放行。

---

## 13. 复审 2 修正（REVISION 2）

Codex 复审 2 指出本报告 4 处问题。**4 处全部成立**，逐条修正如下。本轮仍处 CP0 约束内：**只改本报告文件**，未动代码/配置/CMake，未运行 IDF configure/build，未 Flash。

### 13.1 修正一：pin 上游来源**并非缺失**（首版结论错误）

首版说"本机无上游 git 元数据、无法证明相对 pin 的差异"。**错误。**

| 项 | 实测 |
| --- | --- |
| 路径 | `E:/workbuddy/学习习惯培育AI/vendor/MetalioClaw4` |
| `.git` | 存在；git 2.53.0.windows.1 |
| HEAD | `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950`（= pin） |
| 工作树 | clean；分支 `main` |
| remote | `https://github.com/CloudZao/MetalioClaw4.git` |

**独立重放验证（我自己做，未引用他人结论）**

| 步骤 | 结果 |
| --- | --- |
| `project-ca3aa3fa.patch` SHA256 | `58bfbe5266a9fa00980be5e5078b570b9915165e199981e6dd54dcb21aa81937` = A01 记录 ✅ |
| pin 检出 4 文件 LF 哈希 vs `upstream_sha256` | **4/4 命中** ✅ |
| `git apply --check -p1` | rc=0 ✅ |
| `git apply` → 4 文件 LF 哈希 vs `patched_lf_sha256` | **4/4 命中** ✅ |
| `git apply -R` → 4 文件 LF 哈希 vs `upstream_sha256` | **4/4 命中**（完全恢复）✅ |

**`E:/c` vs 真实 pin**：identical 472 / different 5（= 4 补丁目标 + `sdkconfig`）/ only-in-E:/c 75（73 learning + 2 生成头文件）/ **only-in-pin 0**。→ **零未知漂移**，详见 §5.3。

**根因**：A01 JSON 的 `upstream_sha256` 是 **LF 归一化**口径，首版却拿 **raw（CRLF）**哈希去比，导致 4/4 误判为不同，进而错误推论"参考树不是 pin 的等价副本 ⇒ 本机没有可重放来源"。这是**哈希口径混用**造成的错误，不是缺证据。

### 13.2 修正二：组件内容一致性（首版证据不足）

**首版的错**：只比对了「`dependencies.lock` 里的 `component_hash`」与「磁盘上的 `.component_hash` 文件」。这两者都是**记录**，两者相同只能说明记录没被改，**不能证明组件文件本身未被修改**。

**本轮的修法**：改用 **ESP-IDF 组件管理器自己的算法重算目录内容哈希**：

```
hash_dir(root) = sha256( 对排序后的 (相对路径, sha256(文件内容)) 逐项 update )
                 过滤规则与组件管理器一致：
                 组件的 idf_component.yml 的 include/exclude + use_gitignore，
                 再排除 .component_hash 与 checksums 文件

校验函数：idf_component_tools.hash_tools.validate_hash_eq_hashdir(root, expected)
         —— 即组件管理器在 STRICT_CHECKSUM 模式下自己执行的那一个
工具版本：idf-component-manager 2.5.0（E:/workbuddy/claw4-idf-tools/python_env/idf5.5_py3.12_env）
```

**结果**

| 指标 | 值 |
| --- | --- |
| lock 条目总数 | 83 |
| **内容哈希 MATCH** | **82** |
| **MISMATCH** | **0** |
| 磁盘无对应目录 | 1（`idf` 伪条目，无组件目录） |
| 无 lock 哈希 | 0 |
| 异常 | 0 |
| `espressif/esp_hosted`（C5 协处理器） | `8ca10092db3e8c8122540d907ab705805a269377d31192f087ba1def2f5f4f91` **MATCH** |

**负向对照（证明这个检查真的能抓到篡改）**

只报"82/82 通过"是没意义的 —— 一个不会失败的检查证明不了任何事。所以做了对照实验：

```text
component      : 78/esp-ml307（220,981 bytes）
1) 逐字节复制的目录            -> d4de9738baaaa5e6…  == lock 值 ✅ MATCH
2) 翻转单个文件里的 1 个 bit    -> ccc111fe092115db…  != lock 值 ✅ MISMATCH（被检出）
3) 还原该 bit                  -> d4de9738baaaa5e6…  == lock 值 ✅ MATCH
判定：该检查能检出单比特改动 = True
```

→ **组件内容完整性现已由内容哈希证明**，不再是"记录一致"。
→ 仍**不能**证明的：这套组件与"历史 C5 构建当时所用集合"是否逐字节相同（无独立快照可比）—— 如实保留该限制（§10 第 5 项）。

**合规说明**：校验脚本目前放在**本机取证目录** `E:/workbuddy/claw4-a05-cp0-recon/verify_component_content.py` 与 `negative_control.py`，**没有**放进仓库 `tools/dev/`——因为 CP0 白名单只允许改本报告文件。若 Codex 要求把它固化为仓库内可重跑工具，请在放行 CP1（`tools/dev/` 进入白名单）时一并说明。

### 13.3 修正三：SingleFlight 验收口径（首版错误放宽）

**首版的错**：把 SingleFlightHttpTransport 放进"可被裁剪"的行，写"无直接设备 include"、"若最终未被引用须记 COMPILED_NOT_WIRED 或 GC_DISCARDED"。**错误 —— 设备 HTTP 适配器明确引用并构造了它。**

实测（`E:/claw4-a05-build-m0`）：

```text
integration/metalio_claw4/device/ports/metalio_http_transport.cpp:9   #include "sync/single_flight_http_transport.h"
integration/metalio_claw4/device/ports/metalio_http_transport.cpp:52  return std::make_unique<claw4::sync::SingleFlightHttpTransport>(PerformRequest);
```

完整设备链路：`CreateMetalioHttpTransport()` → `learning_runtime.cpp:65` 注入 `LearningBackendSession` → `BackendClient` 承载每一次 HTTP 调用。

**修正后的验收口径**：SingleFlight 与 SyncExecutor **同类** —— 必须证明 **Compiled + Referenced + 最终 ELF 保留**；**不允许**以 `COMPILED_NOT_WIRED` / `GC_DISCARDED` 通过。§8 模块矩阵已同步更正。

**根因**：CP0 阶段的 `grep … | head -20` **被截断**，`integration/` 的命中没进输出，我却据此下了"无设备 include"的结论。本轮已对 §8 全表做**无截断全量复查**（见 §8 的复查表）：`time_authority` / `interaction_arbiter` / `reminder_core` / `reminder_wake_port` 在 `integration/` 确实 0 命中，那三项的 COMPILED_NOT_WIRED 判定**成立**。

### 13.4 修正四：验收状态表述（首版措辞错误）

首版把 Codex 的「**CP0 可以启动**」写成「**CP0 已获 Codex 验收/复审通过**」。**Codex 此前并未作出该验收结论。**

已更正处：本报告首部状态表、§11 状态声明、§12 引言。
**后续更新**：Codex 已于 2026-09-15 对提交 `6d49c72` 正式**通过 CP0 验收**。复审 2 时点所做的「本包不存在任何 `ACCEPTED`」声明在当时是准确的，现由 §11 的验收记录取代 —— 该句**不再作为当前状态**。

### 13.5 本轮暴露的方法论缺陷（记下来，避免复发）

| 缺陷 | 后果 | 已采取的防线 |
| --- | --- | --- |
| **哈希口径混用**（raw/CRLF vs LF 归一化） | 误判 pin 等价性 → 错误结论"无可重放来源" | 凡与 A01/上游哈希比对，**先确认口径**；本报告所有四文件比对均标注 LF |
| **`grep \| head -N` 截断** | 漏看 `integration/` 命中 → 错误放行 SingleFlight | 判定"有/无引用"时**禁止加 head**；§8 复查表逐模块列命中文件数 |
| **只比记录不比内容** | 组件校验变成同义反复 | 用产生该记录的**同一算法重算内容**，并配**负向对照**证明检查可失败 |
| **把"可以启动"当"已验收"** | 越权宣称验收状态 | 状态表述一律引用原文措辞，不升格；结论性词汇（ACCEPTED/放行）只能由 Codex 给出 |

### 13.6 本轮新增证据文件（均在本机，非交付物）

| 路径 | 内容 |
| --- | --- |
| `E:/workbuddy/claw4-a05-cp0-recon/pin-source.txt` | pin 仓库身份：HEAD/分支/clean/remote/对象类型 |
| `E:/workbuddy/claw4-a05-cp0-recon/pin-replay.txt` | 补丁 SHA、4 文件 LF 哈希、apply --check / apply / reverse 全流程结果 |
| `E:/workbuddy/claw4-a05-cp0-recon/pin-vs-mirror.txt` | `E:/c` vs 真实 pin 的逐文件差异账目 + ascii 树 LF 口径复核 |
| `E:/workbuddy/claw4-a05-cp0-recon/component-content-verify.{txt,json}` | 82 组件内容哈希逐项结果（含 esp_hosted） |
| `E:/workbuddy/claw4-a05-cp0-recon/component-content-negative-control.txt` | 单比特篡改被检出的负向对照 |
| `E:/workbuddy/claw4-a05-cp0-recon/verify_component_content.py`、`negative_control.py` | 校验与对照脚本（待 CP1 决定是否入库 `tools/dev/`） |

---

## 14. CP1：可重放的最小构建登记

### 14.0 门禁状态与越门禁授权（必读，先看这段）

**CP1 是在门禁未放行的情况下、经用户明确授权后执行的。** 事实如下，不做粉饰：

| 事实 | 依据 |
| --- | --- |
| Codex 于 2026-09-15 正式裁定 **A05 CP0 ACCEPTED（前置取证范围）**，同时明确 **CP1 仍为 QUEUED** | `docs/project_management/reports/CODEX_A05_CP0_VERIFICATION_2026-09-15.md`（分支 `codex/a05-cp0-verification` @ `70738dd1`） |
| 该裁定要求：等待 Codex 完成前序 `WB-V53-NEXT-001` 整流验收；**WorkBuddy 保持等待，不开始 CP1/CP2** | 同上；并见 `TASK_BOARD.md` 顶部「2026-09-15 CP0正式复核」段与 `.workbuddy/INSTRUCTIONS.md` 覆盖指令 |
| 任务书 §0 原文亦写明「**CP1～CP4 QUEUED，未经下述放行不得开始构建**」 | `WB-A05-BUILD-001.md` §0 |
| **用户随后明确指示：开始 CP1，并选择「授权我越门禁开工」** | 本轮对话，用户答复 `你授权我越门禁开工` |

**因此：本节的产出是在用户授权覆盖 Codex 门禁的前提下完成的。** 保留此记录以便追溯；Codex 的原始裁定**未被修改**，任务书/看板/INSTRUCTIONS 也**未被改写**。若要回到原门禁状态，请以 Codex 的前序整流验收结论为准。

**本轮仍未做**（与本包一贯纪律一致）：未运行 IDF `configure`/`build`，未产出候选产物，**未 Flash / erase / monitor**，未改 vendor / BSP / sdkconfig 内容 / 分区 CSV / bootloader / ota_1 / eFuse，未加 dummy 调用 / whole-archive / 强制保留符号 / 关闭节裁剪。

### 14.1 CP1-A 隔离重放树（pin → A01 → 19fd979 → 冻结配置）

工作区根 `E:/claw4-a05-19fd979/`，重放源码树 `src/`。

| 步骤 | 命令/依据 | 结果 |
| --- | --- | --- |
| 1. 取 pin | `git archive ca3aa3fa`（源 `E:/workbuddy/学习习惯培育AI/vendor/MetalioClaw4`） | 1264 文件 / 72 MB，**无 `.git`、无 `managed_components`、无 `build`** |
| 2. 归一到 canonical LF | 见下「行尾口径」说明 | 434 个文本文件 CRLF→LF；0 个二进制被误改 |
| 3. 校验 pin | 4 个 A01 目标文件 vs `upstream_sha256` | **4/4 MATCH** |
| 4. 应用 A01 补丁 | `git apply -p1 --whitespace=nowarn`（`--check` rc=0，apply rc=0） | 结果 canonical LF **4/4 == `patched_lf_sha256`** |
| 5. 同步 19fd979 学习源码 | 从 `82337fb` 的 **blob** 写入（精确同路径映射） | **90 文件写入**（learning 87 + learning_screen 3） |
| 6. 安装冻结 C5 配置 | `claw4-idf-cold-c5-20260906-frozen-20260912/sdkconfig` → `src/sdkconfig` | 源/目标 sha256 均 `a901f20491671a9cbca8cbe60c4ac4b26d91ac1916d36530b9475b6704fabc26` **与批准输入一致** |
| 7. 应用 A05 补丁 | §14.2 | `main/CMakeLists.txt` → `14ffc5c3…` |

**最终重放树**：`E:/claw4-a05-19fd979/src`，1354 文件。

**行尾口径（本轮踩到的真实坑，已固化）**：本仓库 `core.autocrlf=true` 且 `.gitattributes` 声明 `* text=auto`，因此**工作树是 CRLF、而提交的 blob 是 LF**。于是 (a) `git archive` 直接产出 CRLF；(b) `git apply` 会把打补丁后的结果**再转成 CRLF**。A01 记录里的 `upstream_sha256` / `patched_lf_sha256` 都是 **LF 口径**。若拿 raw 或工作树字节去比，会得出"4/4 不匹配"的**假失败**（本次首轮即如此）。因此重放树以 **canonical LF** 为准，每步之后重新归一。

### 14.2 CP1-B A05 补丁层

| 项 | 值 |
| --- | --- |
| 补丁 | `integration/metalio_claw4/patches/a05-0001-cmake-source-registration.patch` |
| 补丁 sha256 | `51e2db0411917176c0cbdaafa927d579f7c3547b60e16f838acb753ba463ec5e` |
| 元数据 | `.../a05-0001-cmake-source-registration.json`（base / 应用顺序 / 前后 hash / 回滚 / 约束） |
| 目标 | **仅** `main/CMakeLists.txt` |
| base | A01 之后（`9f7098e0…`，与 `E:/c` 当前该文件**逐字节相同**） |
| 结果（canonical LF） | `14ffc5c3466849179915813c413950b16b2f037a683477c9bf1c7881f0964b6d` |
| 追加的 5 行 | `learning/sync/sync_executor.cpp`、`learning/sync/single_flight_http_transport.cpp`、`learning/time/time_authority.cpp`、`learning/interaction/interaction_arbiter.cpp`、`learning/reminder/reminder_core.cpp` |
| 应用顺序 | `project-ca3aa3fa.patch` → `a05-0001-...patch`（不可颠倒） |
| 回滚 | `git apply -R -p1 --whitespace=nowarn <patch>`，恢复 `9f7098e0…` |

**未做的事**（避免"伪装成链接实现"）：头文件只靠既有 `list(APPEND INCLUDE_DIRS .../learning)` 解析（第 152 行），**未**新增 include 路径；**未**加 dummy 调用、whole-archive、强制符号保留、关节裁剪。

**登记一致性核对**：`main/CMakeLists.txt` 中 `"learning/...` 条目共 **24** 条，逐个核对文件存在性 → **缺失 0**。

### 14.3 CP1-C 工具三件套（`tools/dev/`）

| 工具 | 作用 | 退出码 |
| --- | --- | --- |
| `verify-component-content.py` | 用 **IDF 自己的算法**（`hash_dir` + `validate_hash_eq_hashdir`）重算 managed_components 的**内容**哈希并与锁文件比对；附 `--negative-control` | **任何失败类别均非零**：锁文件缺失 / 组件目录缺失 / 解析到 0 个组件 / 磁盘缺目录 / 缺 lock hash / 内容不匹配 / 单组件异常。仅允许跳过 `idf` 伪条目 |
| `host-reuse-fingerprint.py` | Host 证据复用判定：文件集（`firmware/** ∪ integration/** ∪ tools/dev/**`）+ **工具链身份** + 编译链接参数 → 合成指纹；`--write-baseline` / `--check` | 0 相同、1 不同或基线不可读、2 用法错误 |
| `verify-partition-table.py` | 反解**生成的** `partition-table.bin`，与批准 CSV 的解析结果逐行比对；重叠/越界检查；`ota_0`/`ota_1` 显式断言；余量**只由本候选 app 镜像**计算 | 任一断言失败即非零 |

**实测结果**

```text
verify-component-content.py --negative-control
  lock entries 83 / content verified OK 82 / failures 0 / rc=0
  negative control: detects a single-bit change = True
  失败类别实测：lock missing rc=1；components dir missing rc=1；
                lock 解析 0 组件 rc=1（空文件与垃圾文件各一次）

host-reuse-fingerprint.py（工具链身份 34 项：原生/交叉 程序 19、编译器内部 6、运行库/归档 9；缺失 0）
  文件数 161 / trees_hash 3a238c26… / args_hash 69018d2e… / toolchain_hash 80c46973…
  HOST_REUSE_FINGERPRINT = 63f106e5f80000333580d2051e2380e975e04c2ee962c1fbacb2ee4aade7018c
  --check → SAME（rc=0）

verify-partition-table.py（自测：CSV→BIN→比对，app 用 8 MB 占位，非候选）
  13 行逐行比对全部 OK；last end 0x01fa6000；尾部未分配 368640
  ota_0 余量 1437184 (15.23%)；app > ota_1 → 正确输出「双槽 OTA 不可用」提示
  RESULT: PASS / rc=0
```

**按裁定 #2 重建基线**：新增 `host-reuse-fingerprint.py` 本身进入了输入集，所以**基线已重建**（不是沿用旧值），旧基线 `133ffedb…` 作废、新基线 `63f106e5…`。**按裁定 #5**：组件校验工具已按"空输入/缺目录/缺锁 hash/异常/不匹配必须非零退出"实现并逐类实测。

**⚠️ 指纹工具的一个已知局限（如实标注）**：指纹哈希的是**工作树字节**，而本仓库 `core.autocrlf=true`，新加入的 `.py` 提交时 git 已警告 `LF will be replaced by CRLF the next time Git touches it`。也就是说：**内容未变、仅行尾被重新检出**时，`trees_hash` 会变，`--check` 会报 `DIFFERENT` —— 这是**假阳性**（只会多跑一次门禁，不会漏放行）。可选修正：改为哈希 **git blob 内容**（而非工作树字节），或在工具内先做 LF 归一。倾向后者，等 Codex 裁定后再改，本轮不擅自扩大范围。

**工具里发现并修正的两个真实缺陷**

1. **`gen_esp32part.py` 的输出格式跟随"输入"格式，而不是输出文件名后缀**：CSV 进 → **二进制**出；BIN 进 → CSV 出。所以"CSV→CSV"这种模式**不存在**，请求它会静默得到二进制（首轮自测即因此报 `utf-8 codec can't decode byte 0xaa`，0xAA 正是 ESP 分区表魔数）。已改为**两步**：CSV→BIN→CSV 才能拿到解析后的布局。
2. **w64devkit 是静态链接发行版，树内没有任何 DLL**（`g++ -print-file-name=libstdc++-6.dll` 只回显裸名）。因此"原生运行库身份"**不能**用 DLL 表示。已改为：原生/交叉的**编译器内部程序**（`cc1`、`cc1plus`、`collect2`）+ **静态归档**（`libstdc++.a`/`libgcc.a`/`libsupc++.a`/`libwinpthread.a`、交叉 `libgcc.a`/`libstdc++.a`/`libm.a`/`libc.a`）。

### 14.4 CP1-D 同步账目与 fixture 验证

**同步 Check（镜像 `E:/c` vs 19fd979 的 canonical blob）** —— 基准必须是 **blob 字节（LF）**，用工作树或把行尾差异算进"内容差异"都会误报：

| 类别 | 数量 |
| --- | --- |
| IDENTICAL（与 blob 逐字节相同） | 16 |
| EOL ONLY（内容相同，镜像为 CRLF） | 44 |
| **REAL CONTENT DIFFERS** | **13** |
| mirror-only（无 repo 来源 = 未解释漂移） | **0** |
| repo-only（镜像缺失） | **17** |

- 对账：16+44+13 = 73 = 镜像文件数；+17 = 90 = repo 候选文件数 → **完全闭合**。
- 13 个真实差异（`learning_screen.cc`、`application/coordinator.{h,cpp}`、`device/app/learning_runtime.{h,cpp}`、`device/ports/{metalio_http_transport,nvs_backend_provisioning,nvs_outbox_storage}.{h,cpp}`、`sync/learning_backend_session.{h,cpp}`）**正是 V5.3 NEXT-001 改动过的文件** → 全部可解释。
- 17 个缺失**正是本包新增模块**（含 A05 要登记的那 5 个 cpp）→ 全部可解释。
- **结论：无未解释差异**（`mirror-only = 0` 是关键判据）。

**fixture 验证**（`E:/claw4-a05-19fd979/fixture/`）

| 测试 | 结果 |
| --- | --- |
| A05 补丁 `apply --check` / `apply` / `apply -R` | 全部 **rc=0** |
| 应用后 canonical LF 哈希 == 元数据声明值 | **True** |
| 反向恢复后 == base | **True** |
| A01 在重放树上的前向应用 | rc=0，4/4 命中 `patched_lf_sha256` |

**任务书 §2 的四条 CP1 验证项**：补丁应用/反向/hash **通过**；同步 Check **无未解释差异**；配置关键项——冻结配置副本哈希与批准输入**一致**（逐键枚举沿用 CP0 §4 证据，真正"configure 前后各记一次哈希"属于 CP2）；提交后工作树保持 clean（见提交记录）。

### 14.5 CP1 明确**未**完成的部分

- **未做任何 IDF configure/build**（CP2 范围）。因此：`compile_commands.json`、对象清单、`partition-table.bin` 的真实反解、app 镜像真实字节数与余量、ELF 链接证据——**全部尚不存在**。§14.3 的分区工具只是**自测**（用 CSV 往返生成的 bin + 8 MB 占位 app），**不是候选验证**。
- 三个工具已入库但**尚未接入 Host 门禁**（`tools/dev/verify-host-cpp-tests.ps1` 未改动）。是否纳入需 Codex 裁定。
- **`time` / `reminder` / `interaction_arbiter` 三个模块在设备侧仍无调用点**（§8 复查：`integration/` 内 0 命中）→ 预期 `COMPILED_NOT_WIRED`；`single_flight` 与 `sync_executor` 则**必须**给出最终 ELF 保留证据。
- 本机 `E:\workbuddy\**` 斜杠引用缺陷依旧（§1.3）；本地分支名仍为无斜杠。
- `TCP loopback: ENV_VERIFY_REQUIRED`、`CI: NOT PRESENT` 不变；Device/Hardware 项一律 `HARDWARE_VERIFY_REQUIRED`。

### 14.6 CP1 新增/修改的仓库文件

| 路径 | 类型 |
| --- | --- |
| `integration/metalio_claw4/patches/a05-0001-cmake-source-registration.patch` | 新增（补丁） |
| `integration/metalio_claw4/patches/a05-0001-cmake-source-registration.json` | 新增（元数据） |
| `integration/metalio_claw4/integration_manifest.md` | 追加 A05 补丁层与同步缺口登记 |
| `tools/dev/verify-component-content.py` | 新增 |
| `tools/dev/host-reuse-fingerprint.py` | 新增 |
| `tools/dev/verify-partition-table.py` | 新增 |
| `docs/project_management/reports/WB_A05_BUILD_001_REPORT.md` | 本节 |

隔离工作产物（**不入库**）：`E:/claw4-a05-19fd979/{src,fixture}`、`E:/workbuddy/claw4-a05-cp0-recon/{cp1-*,host-fingerprint-v2.json,pt-*}`。

---

## 15. CP2：隔离根完整构建（**BLOCKED**，含根因定位）

### 15.0 结论先行

**CP2 未完成：`idf.py build` 在 configure 阶段失败，原因是工具链环境缺失，不是源码问题。** 已按任务书「环境问题由 WorkBuddy 整理证据、Codex 定位并给处理意见，**不私自绕过策略**」停下。**未使用任何绕过手段**：没有安装/伪造工具、没有改 `idf_tools` 元数据、没有传 `-DCMAKE_MAKE_PROGRAM`、没有照抄旧 ninja 命令、没有改项目 sdkconfig/CMake/分区。

越门禁授权与 §14.0 同源（用户授权覆盖 Codex 门禁），此处不重复；Codex 原裁定未改。

### 15.1 执行记录（已按任务书要求全部留档）

| 项 | 值 |
| --- | --- |
| 构建根 | `E:/claw4-a05-19fd979/build`（**新建，无任何旧 CMakeCache/object/ELF**） |
| 源码 | `E:/claw4-a05-19fd979/src`（CP1 重放树） |
| SDKCONFIG | `E:/claw4-a05-19fd979/src/sdkconfig`，**显式 `-D SDKCONFIG=<abs>` 传入** |
| 命令 | `idf.py -C <src> -B <build> -D SDKCONFIG=<abs> build`（**不是** ninja 直调） |
| 运行器 | `tools/dev/run-a05-cp2-build.ps1`（已提交 `4b4bc71`） |
| 日志 | `E:/claw4-a05-19fd979/logs/cp2-build-20260915-115131.log` |
| 结果 | **exit=2，elapsed 00:00:11.9**；configure 失败，**未生成任何构件**（app/bootloader/partition-table 全部 ABSENT） |

**环境（与文档记载的 recipe 一致，逐条落实并记录）**：`IDF_PATH`、`IDF_TOOLS_PATH`、`IDF_PYTHON_ENV_PATH` 均设；`ESP_IDF_VERSION=5.5`（**不是 5.5.4** —— 5.5.4 会静默丢 `esp_wifi_remote` 的 Kconfig 变体从而丢失 Wi-Fi 符号）；`IDF_VERSION=5.5.4`；剥离 `PYTHONPATH` 与 9 个 `CODEBUDDY_SAFE_DELETE_*`；PATH 前置 venv/cmake/ninja/riscv32。

**构建前复验**：`verify-build-inputs.py --check` → **5/5 PASS，0 failures**（见 §15.3）。仓库工作树在构建前 **clean**（提交 `4b4bc71`）。

### 15.2 失败点与根因（本轮新证据，比既往记录更精确）

> ⚠️ **本节第「第二步」的结论已在 §17（ENV-DIAG-001）中被实测推翻，请注意更正。** `idf_tools.py export` rc=1 是**真实存在的独立问题，但不是本次冷构建失败的成因**：`tools/idf_py_actions/tools.py` 构造给 cmake 的环境是 `env_copy = dict(os.environ)` + 一个极小的补充字典（第 338–339 行），调用点是第 685 行 `RunTool('cmake', cmake_args, cwd=args.build_dir, env=env, ...)`，**并不消费 `export` 的输出**；`export` 只被 `tools/export_utils/activate_venv.py` 用于 Python 版本加载器。真正的成因见 **§17.4**。

失败输出：

```text
-- IDF_TARGET is not set, guessed 'esp32p4' from sdkconfig 'E:/claw4-a05-19fd979/src/sdkconfig'   <-- 目标识别正确
CMake Error: CMake was unable to find a build program corresponding to "Ninja".
             CMAKE_MAKE_PROGRAM is not set.
-- Configuring incomplete, errors occurred!
cmake failed with exit code 1
```

**第一步：确认不是"机器上没有 ninja"**

| 检查 | 结果 |
| --- | --- |
| `ninja.exe` 存在性 | `E:/workbuddy/claw4-idf-tools/tools/ninja/1.12.1/ninja.exe` ✅ |
| `ninja --version` | `1.12.1` ✅ |
| `Get-Command ninja`（用我搭的环境） | 解析到上述路径 ✅ |
| **`cmake -G Ninja <tiny project>`（同一环境）** | **cmake 完全没抱怨 Ninja，直接进入编译器探测** ✅ |

⇒ **cmake 用我的环境能找到 ninja。** 所以失败不是"环境变量没配好"。

**第二步：定位真正的失败源 —— `idf_tools.py export` 失败**

```text
$ python tools/idf_tools.py export
rc=1
ERROR: tool xtensa-esp-elf-gdb has no installed versions. ...
ERROR: tool xtensa-esp-elf     has no installed versions. ...
ERROR: tool esp32ulp-elf       has no installed versions. ...
ERROR: tool dfu-util           has no installed versions. ...
```

四个工具在 `IDF_TOOLS_PATH` 下的目录**确实不存在**：

| 工具 | 目录 | 对 esp32p4 的相关性 |
| --- | --- | --- |
| `xtensa-esp-elf` | ABSENT | Xtensa 目标工具链，**与 esp32p4（RISC-V）无关** |
| `xtensa-esp-elf-gdb` | ABSENT | 同上，且是**调试器** |
| `esp32ulp-elf` | ABSENT | ULP 协处理器，esp32p4 不用 |
| `dfu-util` | ABSENT | USB DFU，属 S2/S3 系 |

⇒ `idf_tools.py export` **整体 rc=1**（它不区分目标），因此 `idf.py` 构造给 cmake 的子环境**不含** ninja/cmake/交叉编译器条目 → cmake 找不到 Ninja。**这是我搭的环境与 `idf.py` 内部重建环境之间的差异，不是配置缺失。**

**第三步：这条路以前为什么"能跑"？—— 查到了原因**

文档里反复提到的「已固化脚本」`C:/Users/rever/AppData/Local/Temp/claw4_rebuild.sh` **仍在**，其内容显示：该脚本**直接调用 `ninja -j 8`**（对已存在的 build 目录做增量），并自行 `export` 环境变量，**完全绕过 `idf.py`**。

⇒ 这与既往记录一致且互为印证：**历史成功构建是 warm/incremental 的 ninja 直调；而任务书要求的 cold `idf.py build` 从未真正跑通**（`CODEX_APP_FIRST_AF4_HOST_2026-09-05.md:69` 早已记录该阻塞，并明确「不得通过修改项目配置绕过，应修复工具链环境后再跑一次 cold build」）。

**另有一条线索**：`E:/workbuddy/claw4-idf-tools/idf-env.json` 记录了另一个 **esp32p4 专用** IDF 安装（`E:/workbuddy/学习习惯培育AI/toolchains/esp-idf-v5.5.4`，`targets: ["esp32p4"]`），其 `.espressif/tools` 同样**不含**上述 4 个工具 → 换用该环境**不能**解决 export 失败。

### 15.3 构建输入 manifest（本轮已交付）

`integration/metalio_claw4/a05_build_input_manifest.json`（已提交），工具 `tools/dev/verify-build-inputs.py`。

| 输入 | 用途 | 身份（本轮实测） |
| --- | --- | --- |
| IDF 树 | `IDF_PATH` | 树哈希 `9979c6c36539dd7c7021a881…` |
| 工具链根 | `IDF_TOOLS_PATH` | 树哈希 `78ad1256ef07b11b92e90392…` |
| `managed_components`（已装入隔离树） | 组件 | 源/目标树哈希一致 `83c759c8306b11193feebd06…`（82 目录 / 14656 文件 / 666,122,132 B） |
| 冻结 C5 `sdkconfig` | 显式 SDKCONFIG | `a901f20491671a9cbca8cbe60c4ac4b26d91ac1916d36530b9475b6704fabc26` **== 批准值** ✅ |
| 隔离源码树 | `-C` | 树哈希 `f72e868b21680e38c3e817fd…` |

- **`dependencies.lock` 无需决策**：pin 自带的那份与 `E:/c` 的**逐字节相同**（`90af1addf9daf8ae1ec3094a798d7ab56cd12a4fef5702696d08a524458f91e1`），因此组件集合只有一个权威版本。
- 工具对**不可读/符号链接**条目（如 `claw4-idf-tools/msys64/etc/mtab`）输出确定性标记并在 manifest 中列出，而不是崩溃或静默跳过。
- 检出 `git archive`/`git apply` 带来的 CRLF 干扰：manifest 树哈希按**原始字节**计算（这些树不受 git 管理，字节即事实）。

### 15.4 CP2 需要的决策（不自行选择）

| 方案 | 内容 | 代价 / 风险 |
| --- | --- | --- |
| **A（请假裁定）** | 安装 `xtensa-esp-elf` / `xtensa-esp-elf-gdb` / `esp32ulp-elf` / `dfu-util`，使 `export` 可成功 —— 即文档所说"修复工具链环境" | 需网络；**会改动被冻结的工具链目录**（manifest 中 `idf_tools` 树哈希随之变化，须重新冻结）；4 个工具对 esp32p4 构建在功能上**无用**，纯为满足 export 的严格性 |
| **B** | 改由 Codex 定位是否有受支持的"仅 esp32p4 工具集"导出路径（IDF 5.5.4 的 `export` **不支持 `--targets`**，已实测报 `unrecognized arguments`） | 属 Codex 的定位工作；我未找到官方开关 |
| **C** | 显式传 `-D CMAKE_MAKE_PROGRAM=<ninja>` 等，绕过 export | **任务书明确禁止绕过**（「不得通过修改项目配置绕过」）；且会改变 configure 命令，影响可复现声明 —— **我未执行** |
| **D** | 接受"cold build 在本机不可得"，按位置证据降级（例如复现 warm 构建并如实标注为**非 cold**） | 与任务书 §3「只在新隔离根执行完整 idf.py build」的要求不符，需 Codex 明确降级授权 |

**我的建议**：先走 **B**（让 Codex 判是否有受支持的 target 限定导出），若确无，则走 **A** 但在安装**前**先固定工具链目录的可回滚快照并重冻结 manifest。**不建议 C**。

### 15.5 CP2 明确未完成的部分

- **未生成任何构件**：app bin / ELF / map、bootloader、`partition-table.bin` **全部不存在**。因此：
  - `verify-partition-table.py` **没有真实输入可跑**（CP1 里那次是 CSV 往返自测，已在 §14.3 标注）；
  - **本候选 app 的真实字节数与 ota_0 余量未知**；
  - `compile_commands.json`、对象/归档清单、最终 ELF 的符号保留证据 —— 全部留待 CP3。
- 「configure 前后各记一次 sdkconfig 哈希」只完成了**前**（`a901f204…`）；**后**无法取得（configure 未完成）。
- 未做「IDF/components/dependencies.lock 前后核对」中的**后**（构建未发生）。
- 未 Flash / erase / monitor / 执行 flasher_args（这一条是**遵守**，不是未完成）。

---

## 16. CP1-REVIEW-FIX-001：row-diff 格式缺陷修复 + "真实行不匹配"负向测试

### 16.0 范围与结论

| 项 | 值 |
| --- | --- |
| 授权范围（严格白名单） | `tools/dev/verify-partition-table.py`、`docs/project_management/reports/WB_A05_BUILD_001_REPORT.md`（本文件）。**未动任何第三个文件** |
| 本轮任务 | ① 修复 row-diff format 参数错误 ② 增加"真实 row mismatch"负向测试 |
| 结论 | 两条**均已完成**。4 例自测 4/4 符合预期；证伪检验成立（把 row-diff 段还原成修复前写法 → 同一套自测立刻 FAIL 并复现 TypeError） |
| 未做 | 未重开 CP1 已通过项、未顺手重构其它分支；未跑 IDF configure/build；未 Flash/erase/monitor；未改分区 CSV / CMake / sdkconfig；未把工具接入 Host 门禁 |

> 说明：本报告首部「本轮范围 = 仅 CP0」是 CP0 期冻结的陈述，属已被 Codex 验收过的原文，本轮**不修改**；CP1 内容见 §14，CP2 见 §15，本轮的整改见本节。

### 16.1 缺陷定位：row-diff 分支的参数表错位，真实不匹配**必然崩溃**

**修复前源码**（`git show HEAD:tools/dev/verify-partition-table.py`，第 187–189 行）：

```python
failures.append("row differs for %s: expected %s/%s/%s/0x%x/%d flags=%r vs actual %s/%s/%s/0x%x/%d flags=%r"
                % (e["name"], e["type"], e["subtype"], e["offset"], e["size"], e["flags"],
                   a["type"], a["subtype"], a["offset"], a["size"], a["flags"]))
```

**逐位核算（可复核）**：格式串含 **13 个占位符**，实参只有 **11 个**；且第 6 个占位符是 `%d`，却落到了字符串 `e["flags"]`：

| 位 | 占位符 | 实参 | 结果 |
| --- | --- | --- | --- |
| 1–5 | `%s %s %s %s %x` | `e.name / e.type / e.subtype / e.offset / e.size` | 勉强能打印（`offset` 被 `%s` 打成十进制串） |
| **6** | **`%d`** | **`e["flags"]` = `""`** | **`TypeError: %d format: a real number is required, not str`** |
| 7–11 | `%r %s %s %s %x` | `a.type / a.subtype / a.offset / a.size / a.flags` | 根本走不到（整体错位） |

Python 是**边格式化边消耗实参**，所以类型错误先于"实参不足"暴露 —— 这正是实测到的报错形态。根因是**把"expected / actual 哪一侧"的信息寄存进了一个长格式串的顺序**；一旦两侧字段数不等（这里 actual 组按格式串要 3 个 `%s`，实参只给了 `type/subtype`）就整体崩。

**为什么上一轮没发现**：§14.3 的首次自测**只跑了 PASS 路径**（CSV 往返生成的 bin），row-diff 分支**从未被执行**过。缺陷由此漏过 —— 这是本轮最有价值的发现，也说明"只测正例"在该工具上的确不可接受。

### 16.2 修复方式（不靠"数占位符"）

改为**逐字段比较 + 每个差异单独成项**，彻底取消长位置式格式串（新增 `ROW_DIFF_FIELDS` / `format_field_value()` / `describe_row_diff()`）：

```python
diffs = describe_row_diff(e, a)
if diffs:
    failures.append("row differs for %s: %s" % (e["name"], "; ".join(diffs)))
```

- 逐字段（name/type/subtype/offset/size/flags）比对，**每个差异各成一项**，明确写 `expected` 与 `actual` 两侧；
- `offset`/`size` 同时给**十六进制与十进制**（`0x10000 (65536)`），避免再出现"0x/十进制分不清"的误读；
- 字符串字段用 `%r`，空串显式可见为 `''`；
- 占位符数量不再依赖人工核对：字段集合由 `ROW_DIFF_FIELDS` 单一来源驱动。

同时把行数不匹配的诊断改为携带具体信息（原来只报 `row count mismatch` 一词）：

```python
failures.append("row count mismatch: expected %d rows, actual %d (first unmatched: %s)"
                % (len(expected), len(actual), label))
```

### 16.3 负向测试：`--self-test`（真实不匹配，不是桩）

白名单只允许改这一个 `.py`，所以测试**内嵌**在该工具的 `--self-test` 模式下（新增测试文件会越出白名单）。它用**冻结的 `gen_esp32part`** 从**篡改过的批准 CSV 副本**生成**真实二进制**，再跑本脚本本身（子进程，退出码即真实退出码），逐例断言**退出码 + 诊断文本 + 无 traceback**：

| 例 | 构造方式 | 期望 |
| --- | --- | --- |
| positive | 批准 CSV 原样 → bin | **rc=0**，`RESULT: PASS`，无 `DIFF` |
| negative-1 | `coredump` 尺寸 64K→128K（只动最后一行） | **rc=1**，且出现 `row differs for coredump: size: expected 0x10000 (65536) vs actual 0x20000 (131072)` |
| negative-2 | 删掉 `coredump` 行（13→12 行） | **rc=1**，出现 `ROW COUNT MISMATCH` 与 `row count mismatch: expected 13 rows, actual 12` |
| negative-3 | `ota_0` 尺寸 9M→8M（下游 offset 连锁位移） | **rc=1**，`ota_0` 与 `ota_1`…逐行报差异，offset 变化同样以 `expected ... vs actual ...` 给全 |

**断言口径的一处必要澄清**：`gen_esp32part` 把 `Parsing CSV input... / Verifying table...` 进度写到 **stderr**，所以"无 traceback"不能等价于"stderr 为空"，而应断言 `Traceback` / `TypeError` **不出现**。测试已按此实现。

**测试是否有判别力的旁证**：negative-3 的期望值我第一版写错了（误算成 `0x900000`，正确是 `ota_0` 起点 `0x200000` + 8M = `0xa00000`），`--self-test` **当场把它判为 PROBLEM 并打印了完整 stdout** —— 说明断言不是走过场的橡皮图章（该处随后已修正为正确期望值）。

### 16.4 实测证据

```text
# A. 修复前（仓库 HEAD 版本）直跑真实不匹配 —— 崩溃，不是走判定路径
$ python HEAD_verify-partition-table.py --csv <approved.csv> --bin <mutated.bin> --app <app>
  ... row-by-row ... coredump ... OK   <- 打印到第 12 行就断
  TypeError: %d format: a real number is required, not str
  rc=1（来自 traceback，不是来自 RESULT: FAIL）
  Traceback 出现 1 次

# B. 修复后同一输入 —— 干净判定
$ python tools/dev/verify-partition-table.py --csv <approved.csv> --bin <mutated.bin> --app <app>
  ... row-by-row ... coredump ... DIFF
  RESULT: FAIL (1)
    - row differs for coredump: size: expected 0x10000 (65536) vs actual 0x20000 (131072)
  rc=1 ；Traceback 出现 0 次

# C. 修复后正常分区 —— rc=0
$ python tools/dev/verify-partition-table.py --csv <approved.csv> --bin <bin-from-approved.csv> --app <app>
  RESULT: PASS -- generated partition table matches the approved input, no overlap, in range
  rc=0

# D. 内嵌负向自测
$ python tools/dev/verify-partition-table.py --csv <approved.csv> --self-test
  positive   : generated BIN == approved CSV                      rc=0 OK
  negative-1 : one row's size differs (coredump 64K -> 128K)      rc=1 OK
  negative-2 : one partition removed (row count 13 -> 12)         rc=1 OK
  negative-3 : ota_0 size differs -> cascading offsets downstream rc=1 OK
  SELF-TEST: PASS -- 4/4 cases behaved as specified
  rc=0

# E. 证伪检验：把 row-diff 段还原成修复前写法（保留同一套自测）后重跑
  negative-1 ... rc=1 PROBLEM   - stdout missing 'RESULT: FAIL' / - python traceback present / - TypeError present
  negative-3 ... rc=1 PROBLEM   - python traceback present / - TypeError present
  SELF-TEST: FAIL (2 problem(s))  rc=1
  # 修复 → 自测 PASS；还原缺陷 → 自测 FAIL。测试确实咬住了该缺陷。

# F. 旁路行为未回归
  缺 --bin（无 --self-test）           -> rc=2（用法错误，与 host-reuse-fingerprint.py 的口径一致）
  --csv 指向不存在的文件               -> rc=1（FAIL csv not found: ...）
```

留存日志：`E:/claw4-a05-cp1-fix-recon/logs/{prefix-realmismatch,postfix-realmismatch,postfix-positive,selftest-postfix,selftest-falsified,fingerprint-check}.log`。

### 16.5 本轮的诚实边界（不粉饰）

1. **C 例的"正常分区"是"由批准 CSV 自造的 bin"，不是候选构件。** 验证的是**比较器本身**正确，而**不是**候选分区表正确。§15.5「`verify-partition-table.py` 没有真实输入可跑」**本轮依然成立**：CP2 未产出 `partition-table.bin`，本节**不提供任何候选证据**。
2. **`app` 用的是合成的 1 MiB 占位文件**（自测专用，非候选镜像），仅用于满足 `--app` 存在性；`ota_0` 余量数字因此**无候选意义**。
3. **Host 门禁未重跑**：本文件**不被任何脚本引用**（全仓 grep 除自身外 0 命中），且不在 Host 门禁输入路径中（门禁只编译 `firmware/main/**` 与 `integration/.../{host_glue,device/core}`）。任务书本轮也只允许改 2 个文件。如需重跑请裁定。
4. **副作用（必须记录）：`host-reuse-fingerprint.py` 的文件集含 `tools/dev/**`，所以本轮改动使 CP1 基线 `63f106e5…` 失效** —— `--check` 现返回 `DIFFERENT`（rc=1，`files 164`）。但**这不全是本轮造成的**：CP2 已先新增 3 个文件（`tools/dev/verify-build-inputs.py`、`tools/dev/run-a05-cp2-build.ps1`、`integration/metalio_claw4/a05_build_input_manifest.json`），文件数已由 161 变 164。按该工具自身设计，`tools/dev/**` 变化后基线**必须重取而非沿用**（只会多跑一次门禁，不会误放行）。**重取基线属 CP 级决策、也不在白名单内，本轮未擅自重写**。
5. **工具仍未接入 Host 门禁**（§14.5 的表述不变）。

### 16.6 修改产物

| 路径 | 变更 | 身份（canonical LF） |
| --- | --- | --- |
| `tools/dev/verify-partition-table.py` | 修复 row-diff 诊断 + 新增 `--self-test` + 行数不匹配诊断增强；退出码契约明确为 0/1/2 | `95b932f13cc8755548f5ec8088518f998e0ddb7f749ee98c8e6577ce20e5d079`（20130 B；git blob `144ca4401`） |
| `docs/project_management/reports/WB_A05_BUILD_001_REPORT.md` | 新增本节 | 本节 |

变更规模：`1 file changed, 229 insertions(+), 13 deletions(-)`（仅统计代码文件；只追加普通提交，无 rebase/reset/squash/force）。

---

## 17. ENV-DIAG-001：同进程路径取证 → **根因定位（已由最小复现证明）**

### 17.0 结论先行

| 项 | 结果 |
| --- | --- |
| 授权边界 | **不安装** Xtensa/ULP/DFU 工具；**不进 CP3**；不改 CMake / sdkconfig / partition；不做任何绕过 |
| 冷构建结果 | **同一错误复现**：`exit=2`、12.08 s、**零构件**（新根 `build-envdiag-001` 内无 `CMakeCache.txt`、无 bin/ELF） |
| 是否按要求停下 | **是**。已保存「idf.py 实际 cmake 命令」与「完整 configure 日志」后停止，未做第二次尝试 |
| 根因 | **已定位并已用最小复现证明**：`idf.py` 所在 Python 进程里，**`os.environ['PATH']` 与真实 Win32 进程 PATH 不一致** —— 四个 IDF 工具目录**只在真实块里**，而 `subprocess` 传给 cmake 的正是 `dict(os.environ)`。详见 §17.4 |
| 旧结论 | **§15.2「第二步」的 `idf_tools.py export` 归因被推翻**（export rc=1 仍属实，但**非成因**） |

### 17.1 四项要求逐条落地

| 要求 | 落地 |
| --- | --- |
| 在同一 build runner 进程中记录 cmake/ninja/riscv32 的 PowerShell 与 Python 实际解析路径 | ✅ runner 内置 `ENV-DIAG-001` 段，**在同一进程内**做两套取证（PowerShell `Get-Command`/`where.exe`/PATH 逐条；IDF venv Python `shutil.which` + 真实执行），并落盘 `envdiag-<stamp>.txt` |
| 把 `verify-build-inputs --check` 变成 build 的**硬前置** | ✅ 内嵌进 runner，**fail-closed**：非零即 `exit 3`，**不构建**。本轮实测 `rc=0`（5/5 PASS）后才继续 |
| 用**全新 build root** 再执行一次 cold `idf.py build` | ✅ `E:\claw4-a05-19fd979\build-envdiag-001`（新建、无旧 cache），命令与上一轮**完全一致**（未加 `-v`，保持可比） |
| 若仍报 CMAKE_MAKE_PROGRAM → 保存 cmake 命令 + 完整 configure 日志后停止 | ✅ 见 §17.3；**额外**做了根因定位（只读取证，未改动任何工程文件） |

### 17.2 同一进程内的实测解析路径（关键：两套结果**不一致**）

**PowerShell 侧（runner 进程自身）** —— 一切正常：

```text
prepend 候选 4 个：Test-Path 全为 True → kept 4 of 4
PATH 长度 989，24 条，前四条正是 4 个 IDF 工具目录
Get-Command cmake            -> E:\workbuddy\claw4-idf-tools\tools\cmake\3.30.2\bin\cmake.exe
Get-Command ninja            -> E:\workbuddy\claw4-idf-tools\tools\ninja\1.12.1\ninja.exe
Get-Command riscv32-esp-elf-g++ -> ...\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-g++.exe
where.exe ninja / cmake     -> 均命中 IDF 工具目录
```

**IDF venv Python 侧（runner 的子解释器，即 idf.py 的实际运行环境）** —— **全部落空**：

```text
len(os.environ['PATH'])               = 833      <- 21 条，且不含任何 IDF 工具目录
shutil.which('cmake')                = None
shutil.which('ninja')                = None
shutil.which('riscv32-esp-elf-g++')  = None
but:  subprocess.run(['ninja','--version'])  rc=0 -> '1.12.1'
      subprocess.run(['cmake','--version'])  rc=0 -> 'cmake version 3.30.2'
```

**同一个 Python 进程内的自我矛盾**（`kernel32!GetEnvironmentVariableW` vs `os.environ`）：

```text
len(os.environ['PATH'])              = 833
len(GetEnvironmentVariableW('PATH')) = 989        <-- 真实 Win32 进程环境块
IDENTICAL                            = False

只在「真实块」而 os.environ 里没有的：
  + E:\workbuddy\claw4-idf-tools\python_env\idf5.5_py3.12_env\Scripts
  + E:\workbuddy\claw4-idf-tools\tools\cmake\3.30.2\bin
  + E:\workbuddy\claw4-idf-tools\tools\ninja\1.12.1
  + E:\workbuddy\claw4-idf-tools\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin
只在 os.environ 而真实块里没有的：
  - C:\Users\rever\AppData\Local\Programs\WorkBuddy\resources\app.asar.unpacked\cli\vendor\shim\safe-bin
```

- **对照组（不做前置）**：`os.environ=833`、真实块 `732` —— **分歧本来就存在**（os.environ = 真实块 + `shim\safe-bin`），**与我们的前置无关**。
- **`-S` / `-E -S` / 删除 `PYTHONPATH` 三组变体**：全部**不变**（`shim on sys.path` 已为 `False` 时分歧依旧）→ **不是 Python 的 `site`/环境变量级钩子**；分歧在解释器启动**之前**就由宿主进程的启动环境决定。
- `idf_tools.py export`：`rc=1`、**stdout 0 行**、stderr 为 4 个缺失工具（与 §15.2 一致）—— **已留档，但非成因**。

### 17.3 冷构建结果与留档

```text
build root : E:\claw4-a05-19fd979\build-envdiag-001   （新建，-Fresh）
前置校验   : verify-build-inputs.py --check -> entries 5 checked / failures 0 / PASS
invoking   : idf.py -C <src> -B <build> -D SDKCONFIG=<abs> build
exit=2   elapsed=00:00:12.08   零构件（app/bootloader/partition-table 全部 ABSENT）

CMake Error: CMake was unable to find a build program corresponding to "Ninja".
             CMAKE_MAKE_PROGRAM is not set.
-- Configuring incomplete, errors occurred!
```

**idf.py 实际执行的 cmake 命令（已单独落盘）**：

```text
Executing "cmake -G Ninja -DPYTHON_DEPS_CHECKED=1
  -DPYTHON=E:\workbuddy\claw4-idf-tools\python_env\idf5.5_py3.12_env\Scripts\python.exe
  -DESP_PLATFORM=1 -DSDKCONFIG=E:\claw4-a05-19fd979\src\sdkconfig -DCCACHE_ENABLE=0
  E:\claw4-a05-19fd979\src"
```

留档清单：

| 产物 | 内容 |
| --- | --- |
| `E:/claw4-a05-19fd979/logs/cp2-build-20260915-125426.log` | 完整运行日志（含 ENV-DIAG 段与全部 configure 输出） |
| `.../logs/envdiag-20260915-125426.txt` | ENV-DIAG-001 专段（双份解析路径 + 逐条 PATH） |
| `.../logs/envdiag-cmake-command-20260915-125426.txt` | **idf.py 实际 cmake 命令** |
| `.../logs/envdiag-artifacts-20260915-125426/` | `idf_py_stdout_output_36216`、`idf_py_stderr_output_36216`、`CMakeConfigureLog.yaml` |
| `.../logs/envdiag-idf-tools-export.{out,err}.txt` | `export` 的 rc / stdout / stderr |
| `E:/claw4-a05-cp1-fix-recon/logs/probe-*.txt` | 判别探针：`probe-realenv-{PREPEND,NOPREPEND}`、`probe-site-variants`、`probe-minimal-repro`、`probe-path-dump*` |

### 17.4 根因（最小复现证明，不依赖任何工程改动）

**机制**：`idf.py` 把 cmake 作为子进程启动时，显式传入 `env = dict(os.environ)`（`tools/idf_py_actions/tools.py:338-339`，调用点 `:685`）。因此 **cmake 进程的 PATH 就是 `os.environ['PATH']`，而不是本机真实的 Win32 PATH**。而在这台机器上：

- `os.environ['PATH']` **不含**四个 IDF 工具目录；
- 「cmake 可执行文件本身」却仍能被启动 —— 因为可执行文件的解析走的是**父进程真实 PATH**；
- 于是出现了本包反复看到的**非对称现象**：`cmake` 起来了，但 CMake 用自己的 `FindProgram` 在**子进程 PATH** 里找不到 `ninja` → `CMake was unable to find a build program corresponding to "Ninja"`。

这也同时解释了 §15.2「第一步」的另一半疑点：`cmake -G Ninja` 在**独立 PowerShell 会话**里能跑通（PowerShell 不显式传 env，Windows 继承真实块），而在 `idf.py` 里必失败。**§15.2 之前的「同一环境可行」结论正是这条差异造成的误判。**

**最小复现（同进程、同 cmake、同 tiny 工程，只改 env 传递方式）**：

```text
len(os.environ['PATH']) = 732     len(real PATH) = 1090     shutil.which ninja=None cmake=None

--- env=dict(os.environ)   <-- idf.py 交给 cmake 的方式
    rc = 1
      CMake Error: CMake was unable to find a build program corresponding to "Ninja".
                   CMAKE_MAKE_PROGRAM is not set.
--- env=None                <-- 继承真实 Win32 块
    rc = 0
      -- Configuring done / -- Generating done / Build files have been written to: ...\bB

where.exe ninja with env=dict(os.environ)  rc=1 out=''
where.exe ninja with env=None              rc=0 out='E:\workbuddy\claw4-idf-tools\tools\ninja\1.12.1\ninja.exe'
```

⇒ **一句话根因：本机宿主环境（WorkBuddy 沙箱/shim 的启动环境注入）使 Python 进程内 `os.environ['PATH']` 成为一份不含 IDF 工具目录的副本；凡是用 `os.environ` 显式传 env 的工具链（ESP-IDF 的 `RunTools` 正是如此）都看不到工具链。这是环境/宿主问题，不是 IDF、项目 CMake、sdkconfig、分区或 runner 脚本的问题。**

### 17.5 因此对既有结论的两处更正

| 位置 | 原结论 | 更正 |
| --- | --- | --- |
| §15.2「第二步」 | 失败源于 `idf_tools.py export` rc=1 剥离了 idf.py 的子环境 | **不成立**（`export` 输出不参与 cmake 的 env 构造）。export rc=1 只是**并存的独立问题** |
| §15.4 方案 A | 「安装 4 个缺失工具」= 修复工具链环境 | **与本失败无因果关系** —— 装了也仍然失败。用户已明确**不得安装**，本轮未安装 |

**方案 B/C/D 的评价不变**（B 仍属 Codex 定位；C 任务书禁止且本轮未用；D 需显式降级授权）。**新增待裁定项**：如何在「`os.environ` 与真实 PATH 不一致」的宿主环境下获得受支持的构建路径 —— 这是**环境启动方式**层面的问题，需 Codex 裁定，我未擅自改动任何构建配置。

### 17.6 本轮明确未做（边界）

- **未安装**任何工具；**未**改 CMake / sdkconfig / 分区 / 项目源码；**未**用 `-DCMAKE_MAKE_PROGRAM` 或任何绕过；**未**做第二次构建尝试（按要求停止）；**未**进入 CP3。
- 仍**零构件** → `verify-partition-table.py` 依旧没有候选分区表可验；候选 app 字节数/余量、`compile_commands.json`、对象与符号保留证据一律不存在。
- `RISC-V 交叉编译器` 是否真能在该 PATH 传递方式下被 CMake 找到，**未验证**（Ninja 都没过）。

### 17.7 本轮修改产物

| 路径 | 变更 |
| --- | --- |
| `tools/dev/run-a05-cp2-build.ps1` | 新增 `ENV-DIAG-001` 段（同进程 PowerShell/Python 双份取证、`idf_tools.py export` 留档）；`verify-build-inputs.py --check` 变为**硬前置**（`exit 3` fail-closed）；新增**失败证据自动收割**（cmake 命令、configure 日志、`CMakeConfigureLog.yaml`）；显式记录退出码契约 0/2/3/n |
| `docs/project_management/reports/WB_A05_BUILD_001_REPORT.md` | 本节；§15.2 顶部加更正指引；附录 A 增列本轮取证目录 |

---

## 18. Codex CP2 完成情况校验（2026-09-15）的落实

审查输入：`CODEX_A05_CP2_COMPLETION_REVIEW_2026-09-15.md`，审查对象 `workbuddy/a05-build-m0` @ `7fdf5fa`。

### 18.0 逐项裁定（照抄 Codex，不改口径）

| 项 | Codex 裁定 |
| --- | --- |
| CP1-REVIEW-FIX-001 | ✅ **ACCEPTED**（不再阻塞 CP2） |
| CP2 构建输入冻结 | ✅ ACCEPT |
| `verify-build-inputs` 硬前置 | ✅ ACCEPT（fail-closed 设计符合原则） |
| 全新隔离 build root | ✅ ACCEPT |
| 正式 `idf.py build` 口径 | ✅ ACCEPT（无 ninja 直调/warm/set-target/menuconfig/`CMAKE_MAKE_PROGRAM`） |
| 失败边界处理 | ✅ ACCEPT（无 Flash/erase/monitor/flasher_args；未装额外工具；未进 CP3） |
| 前一轮 export 归因已撤回 | ✅ **修正正确** —— 不批准为当前问题安装无关 Xtensa/ULP/DFU 工具，不把 export rc=1 当作本次 cold build 根因 |
| Ninja 失败根因 | ✅ **CONFIRMED**（`ENV-DIAG-001 ROOT CAUSE CONFIRMED`：Python 显式传递的 `os.environ` 与真实 Win32 进程环境不一致） |
| 最小复现 | ✅ 认为**已把变量缩到**该分歧，具判别力 |
| **CP2 最终判定** | ⛔ **BLOCKED / NOT ACCEPTED** |
| **CP3** | ⛔ **NOT AUTHORIZED**（须待 CP2 ACCEPTED） |
| `CI` | 仍须写 `CI: NOT PRESENT`（GitHub 无 commit status / CI Gate），**不得写 `CI PASS`** |

### 18.1 按 Codex §7 统一的状态命名

```text
A05 CP2
BUILD GATE: BLOCKED
ROOT CAUSE: CONFIRMED
ARTIFACTS: NONE
CP3: NOT AUTHORIZED

CP2 = BLOCKED
Reason = HOST ENVIRONMENT PATH DESYNCHRONIZATION
ENV-DIAG-001 = ACCEPTED
```

并明确：**不是** Claw4 业务代码失败、**不是** A05 CMake 补丁被证明错误、**不是** partition/sdkconfig 问题、**不是**"缺 Xtensa/ULP/DFU 工具"导致的已证实问题。**在完成一次真正成功的 cold `idf.py build` 前：不得进入 CP3、不得冻结 M0 Candidate、不得 Flash。**

### 18.2 本轮已实施（只做被授权的部分）

**（a）P0 守卫：把"环境是否已修好"变成构建前的硬闸门**（对应 Codex §8 方案 1 要求的"启动后先确认"）

新增 `P0 guard`：在**要跑 idf.py 的那个 Python 进程**里检查

```text
os.environ['PATH'] == kernel32!GetEnvironmentVariableW('PATH')
4 个工具目录都在 os.environ['PATH'] 里
shutil.which('cmake' / 'ninja' / 'riscv32-esp-elf-g++') 均非 None
```

任一不满足 → **`exit 4`，不构建**。于是"环境未修好就白跑一次构建"在结构上不可能发生。

**实测（本轮，在我这个仍被宿主注入的 shell 里）**：

```text
== P0 guard: os.environ['PATH'] vs real Win32 process PATH ==
PATH_GUARD: DESYNC -- this process cannot see the toolchain; refusing to build
  - os.environ['PATH'] != Win32 GetEnvironmentVariableW('PATH')  (len 732 vs 1090)
  - tool dir absent from os.environ['PATH']: ...\python_env\idf5.5_py3.12_env\Scripts
  - tool dir absent from os.environ['PATH']: ...\tools\cmake\3.30.2\bin
  - tool dir absent from os.environ['PATH']: ...\tools\ninja\1.12.1
  - tool dir absent from os.environ['PATH']: ...\tools\riscv32-esp-elf\...\bin
  - shutil.which('cmake') is None   / ('ninja') is None   / ('riscv32-esp-elf-g++') is None
HARD STOP: host PATH desynchronisation is STILL PRESENT -> no build attempted.
path guard rc = 1        exit 4
```

同一次运行中 `grep -c "invoking: idf.py" = 0` —— **确认未发起任何构建**。

**（b）构建后验证（Codex §9 第 13–16 项）内嵌进 runner，fail-closed（`exit 5`）**：

- **[13]** `sdkconfig` pre/post 哈希比对，漂移即失败（`pre == post` 才通过）；
- **[14/15]** 调用 `verify-partition-table.py` 反解**真实生成**的 `partition_table/partition-table.bin` 并与批准 CSV 逐行比对，app 尺寸/`ota_0` 余量由**本候选镜像**算出；
- **[16]** 构建**后**重跑 `verify-build-inputs.py --check`，输入漂移即失败；
- **[17]** 始终打印 `NO flash / erase / monitor / flasher_args was executed`。

**（c）退出码契约**：`0` 成功且构建后验证全过｜`2` 安装性致命｜`3` 输入校验失败（不构建）｜**`4` 宿主 PATH 失同步（不构建）**｜**`5` 构建成功但构建后验证失败**｜`n` 其余为 idf.py 的退出码。

**（d）明确未实施**：**方案 2（build-environment launcher：把真实 Win32 PATH 同步进 `os.environ` 后再跑原生 `idf.py build`）未实施** —— Codex 写明"不应无授权直接实施"，故**等明确批准**。本轮同样**未**安装任何工具、**未**改 CMake/sdkconfig/partition/源码、**未**使用 `-DCMAKE_MAKE_PROGRAM`、**未**做 warm ninja、**未**再发起一次注定失败的构建。

### 18.3 Codex §9「下一次重跑的最低验收证据」→ runner 落地映射

| §9 项 | 由谁产生 | 现状 |
| --- | --- | --- |
| 1 `verify-build-inputs` 5/5 PASS | runner 硬前置 | ✅ 已实现 |
| 2 新 build root | `-Build` + `-Fresh` | ✅ 已实现 |
| 3 `idf.py build` exit=0 | idf.py | ⛔ 待环境修复后重跑 |
| 4 configure PASS ／ 5 compile PASS ／ 6 link PASS | idf.py | ⛔ 未开始 |
| 7–11 `xiaozhi.bin`/`.elf`/`.map`、`bootloader.bin`、`partition-table.bin` | idf.py | ⛔ 不存在 |
| 12 每个 artifact 的 size + SHA256 | runner 已逐项打印 | ✅ 已实现 |
| 13 sdkconfig pre/post + equality | runner §18.2(b) | ✅ 本轮新增 |
| 14 真实分区表反解 + 逐行比对 + ota_0/ota_1 + 无重叠/在界内 | runner 调 `verify-partition-table.py` | ✅ 本轮新增 |
| 15 app 真实字节数 + `ota_0` 余量 + `ota_1` fits | 同上（工具输出） | ✅ 本轮新增 |
| 16 构建后外部输入复核 | runner 重跑 `--check` | ✅ 本轮新增 |
| 17 明确 `NO FLASH` | runner 始终打印 | ✅ 已实现 |

即：**一次性重跑即可自动产出 §9 全部 17 项证据**，不需要再补一轮人工取证。

### 18.4 环境修复的两条路径（Codex §8）与已授权边界

| 路径 | 内容 | 状态 |
| --- | --- | --- |
| **方案 1（优先）** | 从**不经过 WorkBuddy shim / sandbox 环境注入的普通宿主终端**启动同一个 `run-a05-cp2-build.ps1`；启动后 P0 守卫会**自动确认** `os.environ['PATH'] == Win32 PATH` 且 `shutil.which(cmake/ninja/riscv32-esp-elf-g++)` 非空，再执行全新 cold build | **已就绪**（需在普通宿主终端执行；本会话的 shell 被注入，实测仍 DESYNC） |
| **方案 2（需明确批准）** | 增加 build-environment launcher：仅把被宿主破坏的 PATH 同步回 `os.environ['PATH']`，不碰 IDF 源码/项目 CMake/sdkconfig，不指定 `CMAKE_MAKE_PROGRAM`，完整记录同步前后 PATH 与哈希，之后仍执行**原生** `idf.py build` | **未实施**（Codex：不应无授权直接实施） |

### 18.5 本轮修改产物

| 路径 | 变更 |
| --- | --- |
| `tools/dev/run-a05-cp2-build.ps1` | 新增 `P0 guard`（`exit 4`，fail-closed）；新增构建后验证 `[13]/[14/15]/[16]`（`exit 5`）；退出码契约扩为 0/2/3/4/5/n；头部注明方案 1 的调用方式与仍被禁止的手段 |
| `docs/project_management/reports/WB_A05_BUILD_001_REPORT.md` | 本节；首部「报告状态」按 Codex §7 统一口径更新 |

---

## 19. CP2-RUNNER-FIX-001：Codex 对 `4f754c0` 的 CHANGES_REQUIRED 落实

审查输入：Codex 对 `4f754c0` 的正式判断。**只允许改** `tools/dev/run-a05-cp2-build.ps1` 与 `docs/project_management/reports/WB_A05_BUILD_001_REPORT.md`（本节）。按要求**不再跑 cold build**，只做静态 + 小型 self-check。

### 19.0 Codex 对 `4f754c0` 的裁定

| 项 | 裁定 |
| --- | --- |
| P0 PATH Guard | ✅ **ACCEPTED** |
| ENV-DIAG | ✅ **ACCEPTED** |
| CP2 状态命名 | ✅ **正确** |
| 禁止 Flash / 禁止 CP3 | ✅ **正确** |
| **Post-build verifier** | ⚠️ **CHANGES_REQUIRED** —— 只三处 |

### 19.1 三处修复（逐条）

| # | 要求 | 修复前 | 修复后 |
| --- | --- | --- | --- |
| 1 | sdkconfig post hash 改为**重新 hash `$Src\sdkconfig`**，不要找 `build/config/sdkconfig` | `$sdkAfter = Join-Path $Build "config\sdkconfig"`；`$postHash = ... else "ABSENT"`，且与"事后重算的 pre"比 → 实际上是**同一次重算的自我比较**（近乎恒真） | 顶部在 configure 前记录 `$preHash`（第 159 行）；构建后**对同一个被 `-D SDKCONFIG=` 传入的文件重新 hash** 得 `$postHash`（第 394 行）；`$preHash -ne $postHash` 即判为漂移。**不再读 `build\config\sdkconfig`**（全文件 0 处引用，仅注释里说明为何不读） |
| 2 | 5 个必需 artifact **任一缺失都必须 `exit 5`** | 逐项打印，缺了只显示 `ABSENT`，不影响退出码 | 抽出 `$RequiredArtifacts`（5 项，第 76 行）；缺失项收集进 `$missingArtifacts`，并作为 `[7-11]` 计入 `$postFail` → 与 `[13]/[14-15]/[16]` 一起触发 **`exit 5`**；另打印 `required artifacts present : n / 5` |
| 3 | `PartitionCsv` 改用 **`$Src\partitions\v1\32m_dual.csv`**，不要默认引用 `E:\c` | `[string]$PartitionCsv = "E:\c\partitions\v1\32m_dual.csv"` | 默认改为空串，参数块之后解析为 `Join-Path $Src "partitions\v1\32m_dual.csv"`（第 72 行）；**全文件 0 处 `E:\c`/`E:/c` 残留**（已 grep 核验） |

**修复 3 附带的一处小改动（明示，供复核否决）**：既然该 CSV 已成为本候选**树内**输入，就把它做成**前置检查**——文件不存在则 `exit 3`、不构建（第 311-313 行），并记录其 **raw sha256 与 LF 归一化 sha256**（第 316-322 行）。理由是：否则一次 30 分钟量级的构建可能在**没有预期输入**的情况下跑到最后才失败。若不接受，删掉这 6 行即可，不影响三处修复本身。

### 19.2 实施中发现并必须记录的输入事实（口径差异，不是内容差异）

| 文件 | 字节 | 行尾 | raw sha256 |
| --- | --- | --- | --- |
| `E:\claw4-a05-19fd979\src\partitions\v1\32m_dual.csv`（隔离树内，本次启用） | **810** | **LF**（CRLF=0，bareLF=15） | `c5277b4bf6348c676cdb1e02fcd4275d6b7a3630675f0ad1291f85fa8b01116b` |
| `E:\c\partitions\v1\32m_dual.csv`（旧默认，CP0/CP1/CP2 取证一直用的那份） | 825 | CRLF（CRLF=15） | `3522639424052778951248d32b02cff0690ddb86d84968b357d43fffc79f2ff6` |

- **LF 归一化后两者逐字节相同**（同一个 `c5277b4b…`），**13 行布局完全一致**（`ota_0` @0x200000/9M、`ota_1` 接续 0xb00000/4M …）。
- ⇒ 按修复 3 改用树内副本**不会改变期望布局**，只是去掉了对 `E:\c` 的依赖。
- ⚠️ **提醒复核者**：`verify-partition-table.py` 会打印被比对 CSV 的 sha256，**它现在会是 `c5277b4b…` 而不是历史记录里的 `3522639424…`**。这是**行尾口径**差异（825 CRLF vs 810 LF），**不是分区内容变更** —— 别把它当作"输入被改动"。

### 19.3 self-check 证据（静态 + 小型等价自检，未跑 cold build）

**(a) 静态**：语法 `SYNTAX OK`（467 行）；`grep` 核验 `E:\c` **0 处**、`build\config\sdkconfig` **仅注释 1 处**、`$RequiredArtifacts` 定义 1 处/使用 3 处、`$preHash`↔`$postHash` 比对 1 处、`$PartitionCsv` 传入分区校验器 1 处。

**(b) 小型等价自检**（同一表达式 + 合成输入；日志 `E:/claw4-a05-cp1-fix-recon/logs/selfcheck-runnerfix.txt`）：

```text
fix3 resolved        : E:\claw4-a05-19fd979\src\partitions\v1\32m_dual.csv     exists = True
fix3 raw sha256      : c5277b4bf6348c676cdb1e02fcd4275d6b7a3630675f0ad1291f85fa8b01116b
fix3 LF  sha256      : c5277b4bf6348c676cdb1e02fcd4275d6b7a3630675f0ad1291f85fa8b01116b  [CRLF=0 bareLF=15]
E:\c  LF  sha256     : c5277b4bf6348c676cdb1e02fcd4275d6b7a3630675f0ad1291f85fa8b01116b  LF-normalised identical = True

fix2 synthetic root  : <temp>   （5 项中故意只放 4 项）
fix2 missing         : xiaozhi.map
fix2 expect          : exactly 'xiaozhi.map'  -> match = True
fix2 verdict         : would exit 5                     ← 缺失即判定失败

fix1 pre  (before configure) : a901f20491671a9cbca8cbe60c4ac4b26d91ac1916d36530b9475b6704fabc26
fix1 post (re-hash same file): a901f20491671a9cbca8cbe60c4ac4b26d91ac1916d36530b9475b6704fabc26
fix1 no-drift verdict        : True
fix1 drift probe (on a copy) : pre != drifted = True    ← 证明该比对**可以失败**（不是恒真）
```

- `a901f204…` 正是冻结 C5 配置的批准值 ⇒ 修复 1 的 pre 侧与既有批准输入一致。
- 该 harness **明确不是端到端运行**：P0 守卫在本会话的 shell 里仍然拒绝构建。

**(c) 实跑（到守卫为止）**：`runner exit code = 4`；日志里 `invoking: idf.py` **0 次**（仍未发起任何构建）。

### 19.4 本轮明确未做

**未跑 cold build**（按要求只做静态/小型 self-check）｜未安装任何工具｜未改 CMake/sdkconfig/partition/源码｜未用 `CMAKE_MAKE_PROGRAM`｜未做 warm ninja｜未实施方案 2｜未进 CP3｜未 Flash。

### 19.5 本轮修改产物

| 路径 | 变更 |
| --- | --- |
| `tools/dev/run-a05-cp2-build.ps1` | 三处修复 + 修复 3 附带的前置检查；头部注明 `CP2-RUNNER-FIX-001` 三点与三条退出码语义不变 |
| `docs/project_management/reports/WB_A05_BUILD_001_REPORT.md` | 本节 |

---

## 20. CP2 第二次 cold build（宿主环境修复后）：**环境问题已解决，暴露出上游源码缺陷**

用户按 Codex §8 方案 1，在**从开始菜单直开的普通终端**执行了 `run-a05-cp2-build.ps1`（`-Build build-cp2-002 -Fresh`）。

### 20.0 结论先行

| 项 | 结果 |
| --- | --- |
| **宿主 PATH 失同步** | ✅ **已解决** —— `PATH_GUARD: OK -- os.environ['PATH'] == Win32 PATH (len 874)`；4 个工具目录齐全；`shutil.which(cmake/ninja/riscv32-esp-elf-g++)` 均可解析；`path guard rc = 0` |
| **Ninja / `CMAKE_MAKE_PROGRAM` 错误** | ✅ **未复现**（runner 的 DIAGNOSIS 行确认）—— §17.4 的根因与方案 1 的正确性**同时被验证** |
| configure | ✅ **PASS**（`cmake -G Ninja … -DSDKCONFIG=…` 实际执行，命令已留档） |
| 交叉编译器 | ✅ **实际被调用**（日志含完整 `riscv32-esp-elf-g++` 命令行与 RISC-V 目标参数） |
| 编译 | ⚠️ 2,643 个目标跑到 **2,494** 停下；**恰好 1 个 TU 失败** |
| 构件 | **2 / 5**：`bootloader.bin`、`partition-table.bin`；`xiaozhi.bin/.elf/.map` 缺失（链接从未开始） |
| sdkconfig | ✅ pre == post（`a901f204…`），**configure 未改写输入** |
| **CP2 判定** | ⛔ 仍 **BLOCKED**，但**原因已由"宿主环境"换成"上游源码缺陷"** |

**已被本次运行排除的原因**：宿主 PATH 失同步、CMake/生成器、工具链缺失、sdkconfig、分区表、`idf_tools.py export` rc=1。
**新的唯一阻塞原因**：仓库 revision `19fd979` 的 `learning_screen.cc` **不可编译**。

### 20.1 唯一失败点（精确到行）

```text
FAILED: esp-idf/main/CMakeFiles/__idf_main.dir/display/screen/learning_screen/learning_screen.cc.obj
E:/claw4-a05-19fd979/src/main/display/screen/learning_screen/learning_screen.cc:497:9:
    error: 's_selftest_timer' was not declared in this scope
  497 |     if (s_selftest_timer != nullptr) {
  498 |       lv_timer_del(s_selftest_timer);
  499 |       s_selftest_timer = nullptr;
ninja: build stopped: subcommand failed.
```

全文件 `grep` 结果：`s_selftest_timer` **仅这 3 处引用，零声明**。

### 20.2 根因链（逐层排除后定位到"不完整删除"）

1. **不是环境** —— 同上，`PATH_GUARD: OK`。
2. **不是 CMake / 生成器 / 工具链** —— configure PASS，交叉编译器被真正调用。
3. **不是 sdkconfig / 分区** —— pre==post；真实分区表逐行校验 PASS（§20.4）。
4. **是源码，且缺陷在仓库自身 revision** —— 隔离树内该文件与仓库 blob `19fd979:integration/metalio_claw4/device/learning_screen/learning_screen.cc` **LF 归一化后逐字节相同**（`34447f0b…`，27,469 B）⇒ 重放是忠实的，**不是同步走样**。
5. **具体形态：一次"不完整删除"**。仓库版 **700 行** vs `E:/c` 镜像版 **761 行**，diff 显示仓库版删掉了**整个一次性自检功能块**（`@@ -89,59 +89,7 @@`，净 -53 行）：`RefreshUi()` 前向声明、`s_selftest_step`、`s_selftest_ok[4]`、**`lv_timer_t* s_selftest_timer = nullptr;`**、`RunSelfTestStep()`、以及 pending-self-test 的创建点（镜像版 754-757 行）；**但漏删了 lifecycle unload 里那 3 行清理引用** → 悬空标识符。
6. 该符号声明位于**匿名 namespace 内**（文件局部），**不可能由任何头文件补足** ⇒ **该文件单独就不可编译**，与其它 2,642 个目标的编译结果无关。

### 20.3 与 CP1-D 对账结论的关系（不推翻，但需重新裁定同步方向）

`learning_screen.cc` 正是 CP1-D 记录的 **13 个「REAL CONTENT DIFFERS」之一**，当时记为"V5.3 NEXT-001 改动过的文件 → 可解释"，并按 §14.4 既定规则**同步为仓库 blob**。

> ⚠️ **"可解释"不等于"可编译"**：那次对账核的是**来源可追溯**，并未（也无法）核到**该 revision 是否可编译**。本条不构成对 CP1-D 结论的推翻，但说明**"13 个差异一律取仓库侧"这一同步方向需要重新裁定** —— 至少对 `learning_screen.cc`，镜像侧（761 行，含功能、历史上 warm build 过）与仓库侧（700 行，功能被删且删漏）**不是等价的两个修订**。

### 20.4 本次运行已取得的 §9 证据（构建未完成，逐项如实标注）

| §9 项 | 结果 |
| --- | --- |
| 1 输入 5/5 PASS | ✅ runner 硬前置通过 |
| 2 新 build root | ✅ `E:/claw4-a05-19fd979/build-cp2-002`（`-Fresh`） |
| 3 `idf.py build` exit=0 | ❌ exit=2（耗时 00:03:36） |
| 4 configure PASS | ✅ |
| 5 compile PASS | ❌ **1 / 2,643 TU 失败**（`learning_screen.cc`） |
| 6 link PASS | ⛔ 未开始 |
| 7–11 五个构件 | ⚠️ **2 / 5**：`bootloader/bootloader.bin` 20,416 B `f11b3e01…`；`partition_table/partition-table.bin` 3,072 B `ef0039b6…`；`xiaozhi.bin/.elf/.map` **ABSENT**（另生成 `factory_test.bin` 614,400 B、`ota_data_initial.bin` 8,192 B） |
| 12 size + SHA256 | ✅ 见上 |
| 13 sdkconfig pre/post | ✅ `a901f204…` == `a901f204…`，**无漂移** |
| 14 真实分区表反解 + 逐行比对 | ✅ **PASS** —— 见下 |
| 15 app 字节数 / `ota_0` 余量 | ⛔ **不适用**（app 不存在） |
| 16 构建后输入复核 | 见 §20.5 |
| 17 NO FLASH | ✅ runner 打印 |

**§9-14 详情（用已入库的 `verify-partition-table.py` 对真实生成物执行，rc=0）**：

```text
approved CSV      : E:/claw4-a05-19fd979/src/partitions/v1/32m_dual.csv
  sha256          : c5277b4bf6348c676cdb1e02fcd4275d6b7a3630675f0ad1291f85fa8b01116b
generated BIN     : E:/claw4-a05-19fd979/build-cp2-002/partition_table/partition-table.bin
  sha256          : ef0039b6366c57de098972c0f6e9fd991013b41968da9b866c4704cb68ef7e5f
13 行逐行比对     : 全部 OK（ota_0 0x00200000 / 9437184；ota_1 0x00b00000 / 4194304）
last end          : 0x01fa6000   尾部未分配 368640 B
RESULT: PASS -- generated partition table matches the approved input, no overlap, in range
app image         : **合成 1 MiB 占位（非候选）→ ota_0 余量/ota_1 结论无候选意义**
```

**交叉印证（强证据）**：真实生成的 `partition-table.bin` 的 sha256 `ef0039b6…` 与 §16 里**由批准 CSV 往返生成的 bin** 的 sha256 **完全相同** ⇒ 构建产物中的分区布局与批准输入解析后的布局**逐字节一致**。因此 §9-14 的结论对**真实构件**成立（app 相关部分除外）。

### 20.5 构建后输入复核（本次手工补跑）——**发现：构建会写入隔离源码树**

runner 的 `[16]` 只在 `rc == 0` 时才执行，本次构建未成功故未跑到该步。已**手工补跑** `verify-build-inputs.py --check`：

```text
entries : 5 checked      failures : 3
  FAIL isolated_src: origin_tree_sha256 changed  manifest=f72e868b21680e38c3e817fd…  now=3c086bf8d4253c5287bcf0f8…
  FAIL isolated_src: origin_file_count changed   manifest=1354                        now=1356
  FAIL isolated_src: origin_bytes changed        manifest=72529007                    now=72560858
RESULT: FAIL -- do NOT build; fix or re-freeze the inputs first
```

**定位：本次构建在隔离源码树内新写/改写了 3 个头文件**（`E:\claw4-a05-19fd979\src`，排除 `managed_components`/`build`，按 mtime 筛出）：

| 文件 | 大小 | 时间 | 性质 |
| --- | --- | --- | --- |
| `main/i18n/i18n_strings_gen.h` | 78,840 B | 14:46:56 | 已存在、被**改写**（文件数不变） |
| `main/assets/lang_config.h` | 10,639 B | 14:46:56 | **新增**（`gen_lang.py` 生成） |
| `main/mmap_generate_resources.h` | 18,246 B | 14:46:59 | **新增**（`build_default_assets.py` 生成） |

**必须记录的三个后果**：

1. **隔离树不是"构建只读"的** —— ESP-IDF 的资源/语言生成按设计把结果写回**项目源码目录**（这也正是 `E:/c` 镜像里存在这两个"生成物"的原因）。
2. **§9 第 16 项（构建后外部输入复核）在现有 manifest 定义下永远不可能 PASS** —— `isolated_src` 的树定义**包含了这些会被构建重写的路径**。要么把生成路径从 `isolated_src` 的定义里排除，要么明确"构建后复核对 `isolated_src` 只做'非生成文件'比对"，要么在首次构建后重冻结该条目。**这是定义问题，需要裁定，不是我该自行改的**（manifest 不在本轮白名单内）。
3. ⚠️ **下一次构建会被 runner 自己的硬前置拦下**：冻结值 `f72e868b…` 已被 `3c086bf8…` 取代，`--check` 现在会 `exit 3`、拒绝构建。因此**在下一次 cold build 之前必须先解决第 2 条**（重冻结 / 改排除定义 / 或把树恢复到冻结态 —— 但恢复到冻结态也只能让"构建前"通过，"构建后"仍会再次分歧）。

### 20.6 本轮明确未做

**未修改任何源码 / CMake / sdkconfig / 分区**（`learning_screen.cc` 不在任何白名单内）；**未修改冻结的隔离树**（改它会作废 manifest 的 `isolated_src` 树哈希，属 CP 级决策）；未 Flash / erase / monitor；未进 CP3；未安装任何工具。

### 20.7 待 Codex 裁定的两条路线（我不自行选择）

| 路线 | 内容 | 代价 / 风险 |
| --- | --- | --- |
| **A（推荐）** | 判"自检功能应保留"→ 在**仓库** `integration/metalio_claw4/device/learning_screen/learning_screen.cc` 上补回被漏删的 3 行引用（或补回整个自检块），形成新的仓库提交 → **重新同步隔离树** → **重冻结** manifest 的 `isolated_src` 哈希 → 再跑一次全新 cold build | 需授权动 `integration/**`（不在 A05 白名单）+ 重冻结哈希（CP 级决策）。与"镜像侧历史上能 warm build"一致 |
| **B** | 判"删除自检功能才是本意"→ 则改为**删掉那 3 行漏删的清理引用**；同样必须走上游仓库提交 + 重放 | 同上；但会主动移除一个 debug 能力，需业务侧确认 |

**共同前提**：无论 A/B，都**只能改上游仓库再重放**，不得直接改冻结的隔离树（否则 CP2 的"可复现"声明失效）。**请裁定后我再动手。**

**另需一并裁定（否则下一次构建跑不起来）**：§20.5 的 `isolated_src` 定义问题 —— 该条目当前把"构建会重写的 3 个生成头文件"也算进树哈希，导致

- **构建后**复核必然 FAIL（§9-16 永远不可能 PASS）；
- **构建前**复核现在也已 FAIL（`f72e868b…` → `3c086bf8…`）→ runner 会 `exit 3` 拒绝构建。

可选处置（都属 CP 级决策，我不自行选择）：① 把 3 个生成路径从 `isolated_src` 的树定义中排除；② 首次构建后重冻结该条目（并说明"生成物随后由构建再生"）；③ 保留严格口径，但把 `[16]` 明确限定为"非生成文件"的比对。

### 20.8 本次运行留档

| 产物 | 内容 |
| --- | --- |
| `logs/cp2-build-20260915-144326.log` | 完整运行日志（6,401 行 / 400 KB），含 guard、ENV-DIAG、输入校验、全部编译输出、唯一失败点 |
| `logs/envdiag-20260915-144326.txt` | ENV-DIAG 专段（`PATH_GUARD: OK`、双份路径解析） |
| `logs/envdiag-cmake-command-20260915-144326.txt` | idf.py 实际执行的 cmake 命令 |
| `logs/envdiag-artifacts-20260915-144326/` | `CMakeCache.txt`、`CMakeConfigureLog.yaml`、`idf_py_{stdout,stderr}_output_{13516,37008}` |
| `E:/claw4-a05-cp1-fix-recon/logs/real-partition-verify.log` | 真实 `partition-table.bin` 的逐行校验输出（rc=0） |
| `E:/claw4-a05-cp1-fix-recon/{repo_learning_screen.cc,diff-mirror-vs-repo.txt}` | 仓库 blob 副本与"镜像 vs 仓库"差异 |

---

## 21. CP2-SOURCE-FIX-001：授权整改（A–D 已完成；E 待用户在普通终端执行）

授权范围 A–F（用户指令原文）。**旧 `HOST PATH` blocker = CLOSED**（§20）。当前两个 blocker 的处理：

| Blocker | 处理 | 状态 |
| --- | --- | --- |
| ① upstream `learning_screen` compile defect | A：只改权威 Git 源，删除 stale cleanup block | ✅ 完成 `2c8f58f` |
| ② `isolated_src` generated-path policy | B：verifier 加精确路径 `exclude_files`；C：干净重放 + 重冻结 manifest | ✅ 完成 `1d064dc` |

### 21.1 A：源码修复（只改权威 Git 源）

**授权理由核实**：`6d84ea3` = `fix(A02): protect formal learning state from demo reset`，该 commit 对 `integration/metalio_claw4/device/learning_screen/learning_screen.cc` 的改动是 **66 行、绝大多数为删除**，即确实移除了**自动四步 SELFTEST 与 `ResetToSeed`**；被漏删的正是 lifecycle unload 里那 3 行引用。

**改动**：删除 4 行（`if (s_selftest_timer != nullptr) {` … `}`）——`git diff` 为**单文件、4 deletions**，无其它任何改动。

| 约束 | 遵守情况 |
| --- | --- |
| 不得恢复 SELFTEST | ✅ 未恢复 |
| 不得重新声明 `s_selftest_timer` | ✅ 未声明 |
| 不得恢复 `ResetToSeed` | ✅ 未恢复 |

**额外核验（D 的 rg 要求）**：`integration/metalio_claw4/device` + `firmware` 中 `s_selftest|SELFTEST|ResetToSeed|SelfTest` → **0 处**（`learning_screen/README.md` 亦为 0）。A02 报告原文声称的 *"no production references remain"* 至此**才真正成立**。新树 `src-cp2-003` 的生产路径同样为 **0 处**。

提交：**`2c8f58f53506402918284c695100f007798433d8`**（= 本轮后续使用的**新 authoritative source SHA**），已推送。

### 21.2 B：input verifier 的精确路径 `exclude_files`

新增 `exclude_files`（每条目可选），只接受**精确相对文件路径**，以下一律**硬报错**（`--write` 与 `--check` 双路径）：绝对路径 / 盘符、含 `..` 段、含通配符 `* ? [ ]`、以 `/` 结尾、空串；`measure()` 另加**文件系统级检查**：条目若指向**目录**即报错 ⇒ **目录级排除在结构上不可能**。

- 该列表写入 manifest 并**纳入 `--check` 比较** ⇒ 任何扩大都是显式、fail-closed 的 manifest 变更。
- 每个被排除路径的**存在性 / 大小 / sha256 会被记录并打印**（`report_excluded`），但**刻意不参与比较**（构建会重新生成它们）。
- **其余所有文件仍在摘要内** ⇒ 它们一旦变化仍是硬失败（"其它 isolated_src 文件 post-build 必须保持 immutable hash 完全一致"由此保证）。

**判别性自测**（`E:/claw4-a05-cp1-fix-recon/logs/selfcheck-exclude-files.py`，含反向用例，非正例走过场）：

```text
已跑 9 例：接受精确相对路径 / 接受反斜杠形式（归一化）；
           拒绝绝对路径、盘符路径、'..'、'*'、'?' 、目录后缀、空串
measure() 拒绝指向目录的条目：error = "probe: exclude_files names a directory (not allowed): 'main'"
反向对照：不加排除时摘要为 3c086bf8…（1356 文件）→ 与加排除的结果不同 ⇒ 排除确实生效
```

### 21.3 C：干净重放（**未**在旧树上重冻结）

**(a) 先证明"就地重冻结"是错的**：旧树 `E:\claw4-a05-19fd979\src` 已构建改写，且**无法调和** —— 用 4 种排除组合（排 3 个 / 只排 2 个新增 / 只排 i18n / 不排）**都无法复现其冻结摘要** `f72e868b…`，尽管：

- 文件数**完全对上**（冻结 1354 → 现值 1356，+2；且 3 个生成文件中 `main/i18n/i18n_strings_gen.h` 由 pin 预置（pin=YES）、另两个为新增（pin=NO），算术闭合）；
- 全部 **90 个同步文件仍与其 CP1 ledger 哈希逐一致**（0 不匹配）；
- 除那 3 个生成文件外，**没有任何文件的 mtime ≥ 12:50**。

⇒ 旧树存在**无法用授权排除集解释的额外漂移**（内容漂移或改名/符号链接标记变化，mtime 无法揭示）。**这就是必须干净重放、且不得就地重冻结的直接证据。**

**(b) 新树 `E:\claw4-a05-19fd979\src-cp2-003`（从原始来源重新生成，每步断言）**：

| 步 | 内容 | 结果 |
| --- | --- | --- |
| 1 | `git archive ca3aa3fa`（vendor pin） | 1264 文件 / 72,291,311 B |
| 2 | LF 归一化 | **434 文件**重写 |
| 3 | pin 身份 vs A01 `upstream_sha256` | **4/4 OK** |
| 4 | A01 `apply --check` / `apply` | rc=0 / rc=0；结果 **4/4 == `patched_lf_sha256`** |
| 5 | learning sync @ `2c8f58f` + 冻结 C5 sdkconfig | **90 文件**；sdkconfig `a901f204…` == 批准值；**`learning_screen.cc` == 修复后 blob；selftest 引用 0 处** |
| 6 | A05 补丁 | rc=0；`main/CMakeLists.txt` LF = **`14ffc5c3…` == 期望值** |
| 7 | `managed_components` 复制 | 14,656 文件 / 666,122,132 B；树 `83c759c8…` **与源一致** |
| 8 | 新树身份 | **1353 文件**（排除 3 生成路径）= `36bc3d4d…`；不排除时 1354 文件 = `b5174381…` |

**与旧同步集的差异：恰好 1 个文件** —— `main/display/screen/learning_screen/learning_screen.cc`，27,469 → **27,352 B**（就是 A 的修复）。路径集合完全相同（90/90），其余 89 个文件字节一致。

**登记完整性**：`main/CMakeLists.txt` 中 `learning/` 条目 **24 条**，5 个新源（`sync_executor.cpp`、`single_flight_http_transport.cpp`、`time_authority.cpp`、`interaction_arbiter.cpp`、`reminder_core.cpp`）均已登记，**缺失文件 0**。

### 21.4 manifest 重新生成 + 硬前置恢复可用

| 条目 | 值 | 与上一版 |
| --- | --- | --- |
| `idf` | `9979c6c36539dd7c7021a881…` | 不变 |
| `idf_tools` | `78ad1256ef07b11b92e90392…` | 不变 |
| `managed_components` | `83c759c8306b11193feebd06…`（14656 / 666,122,132 B） | 不变 |
| `sdkconfig` | `a901f20491671a9cbca8cbe6…` | 不变（== 批准值） |
| `isolated_src` | **`36bc3d4d9b847253b90068fd…`**（1353 文件，排除 3 生成路径） | 新树、新路径 |

**3 个生成路径的 pre 状态（已记录，不比较）**：

```text
main/assets/lang_config.h                 exists=False  (not present)
main/i18n/i18n_strings_gen.h              exists=True   bytes=75874  sha256=a19b324eec42abe0a1d8e456230353788a38c38f908c7762fd7f3f63ec34c419
main/mmap_generate_resources.h            exists=False  (not present)
```

**`verify-build-inputs.py --check` → `entries 5 checked / failures 0 / RESULT: PASS`（rc=0）** ⇒ runner 的硬前置恢复可用，**下一次 cold build 不会再被自己拦下**。

### 21.5 D：Host 指纹、完整门禁、rg

**指纹重算**（`tools/dev/**` 与 `integration/**` 均已改动）：files **164**；`trees_hash e0085d2c6eb6f653662942c082de7a7d3645b9a2c593889d7a12cec10ef40aeb`；`args_hash 69018d2e…`（未变）；`toolchain_hash 80c46973…`（未变）；**`HOST_REUSE_FINGERPRINT = eec141fdcee1494627e29aaa337eeeaede035de040e53e1e5be0fea8466a6597`**（新基线写入 `E:/workbuddy/claw4-a05-cp0-recon/host-fingerprint-cp2-sourcefix.json`）。旧 CP1 基线 `63f106e5…` 已作废（`--check` → `DIFFERENT`，`changed fields: trees_hash, HOST_REUSE_FINGERPRINT`）。

**完整 Host 门禁**：⚠️ **28 / 29 PASS（RESULT: FAIL）**，交叉编译步骤 `interface: exit=0`（P4 PASS）。**唯一失败不是测试失败**：

```text
LAUNCH FAIL: 使用"0"个参数调用"Start"时发生异常:"应用程序控制策略已阻止此文件。"
unit learning_mcp_host_tests : RUN FAIL (exit=-1)
```

**为什么可判定为环境伪失败（三重证据）**：

1. **本轮全部日志中 `failures=[1-9]` 出现 0 次** ⇒ 没有任何断言失败；该套件是**启动阶段**被 OS 拦下（`exit=-1`），测试代码一行都没跑。
2. **同一套件在 CP0 基线里是通过的**：`out/a05-cp0-host/host_result.txt` 里 `unit learning_mcp_host_tests : RUN PASS (exit=0)`、`unit summary : 29 / 29 PASS`。而本轮改动**完全没触及 MCP host 代码**（只改了 `learning_screen.cc`、输入校验器、manifest）。
3. **共 6 次重跑**（含**改名强制重链接**后再跑）仍被拦截；首轮 26/29 的另两个被拦套件在后续重跑中自行放行 ⇒ 属"新链接 exe 被应用控制策略瞬时/持续拦截"这一本机已知现象。第 6 次为最终值，结果与第 5 次逐字相同。

⇒ 因此**我不能声称本轮拿到 29/29**；如实记为 **28/29 + interface PASS**，并把该项列为**环境遗留项**（不阻塞 CP2 构建）。若 Codex 需要 29/29 原始证据，可在该 exe 被策略放行后重跑门禁（命令见 §21.8）。

### 21.6 E：随后由用户在普通宿主终端执行（我不能执行）

```powershell
powershell -ExecutionPolicy Bypass -File E:\claw4-a05-build-m0\tools\dev\run-a05-cp2-build.ps1 -Build E:\claw4-a05-19fd979\build-cp2-003 -Fresh
```

- 继续使用既有全部闸门：P0 PATH Guard → `verify-build-inputs --check` 硬前置（现已 PASS）→ 5 个必需 artifact → sdkconfig pre/post → 真实分区表反解 → 构建后 immutable-input 复核（现按精确路径排除 3 个生成路径）→ **NO FLASH**。
- **预期**：`PATH_GUARD: OK` → 输入 5/5 PASS → cold build（上一轮在 3:36 跑完 configure + 2494/2643，本轮应继续推进到链接）。

### 21.7 F：停止条件（照抄，承诺遵守）

- 若 cold build 再出现**新的** compile/link error：**立即停止**，保存完整日志，**不自行扩大源码修改**。
- 若 `exit=0` + 构建后验证 PASS：状态写 **`CP2 REVIEW_READY`**，停止。
- **不进入 CP3，不 Flash。**

### 21.8 本轮最终结论与提交

| 项 | 值 |
| --- | --- |
| A 源码修复 | ✅ `2c8f58f`（新 authoritative SHA） |
| B 校验器 | ✅ `1d064dc` |
| C 干净重放 + manifest 重冻结 | ✅ `1d064dc`（新树 `src-cp2-003`） |
| D 指纹 / 门禁 / rg | ✅ 指纹 `eec141fd…`；门禁 **28/29 + interface PASS**（1 项为本机策略拦截的环境伪失败，证据见 §21.5）；rg 生产路径 0 处 |
| E cold build | ⏳ **待用户执行**（我不具备普通宿主终端，见 §20/§21.6） |

未做：未安装任何工具；未改 CMake / sdkconfig / partition；除 A 授权的 4 行删除外**未扩大任何源码修改**；未动旧的 `E:\claw4-a05-19fd979\src`（仅只读取证）；未 Flash；未进 CP3。

---

## 22. E 首次执行（2026-09-15 16:52）：**构建了错误的树** —— harness 缺口，非源码问题

### 22.0 结论先行

用户按 §21.6 的命令执行，`PATH_GUARD: OK`、Ninja 错误未复现、configure PASS、sdkconfig 无漂移 —— **宿主环境侧仍然正常**。但构建**在同一个 `learning_screen.cc:497` 再次失败**，3 分 27 秒即停，2/5 构件。

**原因不是修复无效，而是这次构建根本没有编译修复后的树。** 我给出的命令漏了 `-Src`，而 runner 的 `-Src` 默认值仍指向**旧树**：

| 项 | 值 |
| --- | --- |
| 实际编译的源（构建日志第 9 行） | `source : E:\claw4-a05-19fd979\src` ← **旧树，未修** |
| manifest 描述的树 | `E:\claw4-a05-19fd979\src-cp2-003` ← 已修 |
| `-Src` 参数 | **未传** → 取默认值（旧树） |

⇒ **这是我的指令错误**（命令不完整），由此暴露 runner 的一个真实缺口。

### 22.1 两棵树的实测差异（同一文件）

| 树 | 字节 | sha256（前 16） | `s_selftest_timer` 出现次数 |
| --- | --- | --- | --- |
| 旧 `src`（本次被编译） | 27,469 | `34447f0b93942624` | **3**（即那 3 行悬空引用） |
| 新 `src-cp2-003` | 27,352 | `a41aeed7b10dfab5` | **0** |

旧树该处上下文实测（第 497 行即失败点）：

```
    if (s_selftest_timer != nullptr) {
      lv_timer_del(s_selftest_timer);
      s_selftest_timer = nullptr;
    }
```

⇒ 与 §20 记录的失败逐字相同：**同一棵树、同一处缺陷**。

### 22.2 为什么硬前置没能拦住这次错误（这是本轮唯一的实质性发现）

构建日志第 111–134 行显示：

```
== HARD precondition: verify-build-inputs.py --check ==
entries  : 5 checked
RESULT: PASS -- every external input matches the frozen manifest
input check rc = 0
input check PASS -> cold build follows
```

**输入校验 5/5 通过，而构建用的是另一棵树。** 原因是结构性的：`verify-build-inputs.py` 校验的是**它自己从 manifest 里读到的路径**（`origin`/`dest`），它**不知道 runner 实际用 `-Src` 传了哪棵树**。C 步把 manifest 的 `isolated_src.origin` 改成了新树，但 runner 的 `-Src` 默认值没跟着改 ⇒ 两者静默分叉。

**结论：`--check` 通过 ≠ "被构建的树 = 被冻结的树"。** 这是一个真实的 harness 缺口，不是纪律问题。

### 22.3 修复（CP2-SOURCE-FIX-001 follow-up，仅 runner 一个文件）

| # | 修复 | 内容 |
| --- | --- | --- |
| 1 | `-Src` 默认值 | `E:\claw4-a05-19fd979\src` → **`E:\claw4-a05-19fd979\src-cp2-003`** |
| 2 | **源树绑定守卫（新，fail-closed）** | 构建前读取 manifest 的 `isolated_src.origin`，要求 `-Src` 与之逐字一致（归一化后比较：绝对化 + 去尾反斜杠 + 小写）；不一致 → **`exit 3`，不构建**，并打印正确的 `-Src` 供直接复制。`isolated_src` 条目缺失/重复也判失败 |

守卫插在 **PATH 守卫之后、输入校验之前**，因此一次跑动最多花几秒就能判定，不会浪费一次冷构建。

### 22.4 守卫的判别性自测（**直接抽取脚本内真实片段执行**，非等价重写）

把 runner 中真实的守卫代码块（文件内 13,864–16,258 字节区段）原样抽出，包进最小外壳后在**子 PowerShell 进程**里执行，共 5 例：

| 输入 `-Src` | 结果 | 判定 |
| --- | --- | --- |
| `E:\claw4-a05-19fd979\src-cp2-003`（新默认） | `rc=0` | 放行 → 会构建 ✅ |
| `E:\claw4-a05-19fd979\src`（**本次实际编译的树**） | **`rc=3`** | **拒建** ✅ |
| `...\src-cp2-003\`（尾反斜杠） | `rc=0` | 归一化后放行 ✅ |
| `E:\CLAW4-A05-19FD979\Src-Cp2-003`（大小写混杂） | `rc=0` | Windows 语义放行 ✅ |
| `E:\c`（无关树） | **`rc=3`** | **拒建** ✅ |

**关键**：第 2 例正是本次失败的那次输入 —— 若上一轮就有这道守卫，**那次注定跑错的构建会被 3 秒内拦下，不会浪费 3 分 27 秒**。这就是该守卫存在的意义。

脚本语法：`SYNTAX OK`（506 行）。

### 22.5 新树自足性核验（下次构建不会再因缺件失败）

| 项 | 结果 |
| --- | --- |
| `managed_components` | **82 目录 / 14,656 文件**（在树内，与 manifest `dest` 一致） |
| `sdkconfig` | 127,103 B（批准冻结值 `a901f204…`） |
| `partitions\v1\32m_dual.csv` | 810 B（树内批准副本） |
| `main\CMakeLists.txt` | 24,812 B（A05 补丁后 `14ffc5c3…`） |
| `learning_screen.cc` | 27,352 B = `a41aeed7…`（已修） |
| 树文件数（不含 `managed_components`） | 1,354 |

### 22.6 本次运行的状态判定（不粉饰）

**这次运行不构成一次有效的 CP2 尝试** —— 它没有编译被修复的树，因此既不能算"修复后仍失败"，也不能算"通过"。如实记为：

```
CP2 = BLOCKED / Reason = INVALID RUN (built the wrong tree; harness gap, now fixed)
ARTIFACTS: 2/5 (bootloader.bin, partition-table.bin) -- 无效候选，不作为 CP2 证据
```

§9 证据方面仍只新增了 `partition-table.bin`（`ef0039b6…`，与 §20 那次逐字节相同，再次印证分区布局由批准 CSV 决定，与源码树无关）。**不主张任何新的候选证据。**

### 22.7 下一步：重跑（命令已修正）

```powershell
powershell -ExecutionPolicy Bypass -File E:\claw4-a05-build-m0\tools\dev\run-a05-cp2-build.ps1 -Build E:\claw4-a05-19fd979\build-cp2-004 -Fresh
```

新默认值已指向 `src-cp2-003`，**不传 `-Src` 也正确**；若哪天传错，守卫会在 3 秒内 `exit 3` 拒建。等价写法（显式指定，双保险）：

```powershell
powershell -ExecutionPolicy Bypass -File E:\claw4-a05-build-m0\tools\dev\run-a05-cp2-build.ps1 -Src E:\claw4-a05-19fd979\src-cp2-003 -Build E:\claw4-a05-19fd979\build-cp2-004 -Fresh
```

**F 停止条件不变**：出现**新**的 compile/link error → 立即停、存日志、不扩大源码修改；`exit=0` + 后验证 PASS → 写 `CP2 REVIEW_READY` 并停止；不进 CP3、不 Flash。

---

## 23. E 第二次执行（2026-09-15 17:09）：**Windows 命令行长度上限**被路径长度顶破

### 23.0 这次终于是对的树 —— 但撞上另一堵墙

本次运行（`cp2-build-20260915-170947.log`）**所有闸门都对**：

```
source       : E:\claw4-a05-19fd979\src-cp2-003     ← 修复后的树 ✅
PATH_GUARD: OK -- os.environ['PATH'] == Win32 PATH (len 874)
-- source/tree binding (-Src vs manifest isolated_src.origin) --
  manifest isolated_src.origin : E:\claw4-a05-19fd979\src-cp2-003
  OK -- -Src == manifest isolated_src.origin        ← §22 新增的守卫生效 ✅
input check PASS -> cold build follows              ← 硬前置 5/5 ✅
```

**§20 的 `learning_screen.cc:497` 编译错误没有再出现** —— 源码修复确实生效了。构建推进到 **2,439 / 2,643**，随后失败：

```
CreateProcess failed. Command attempted:
"E:\workbuddy\...\riscv32-esp-elf-g++.exe ... -c E:/claw4-a05-19fd979/src-cp2-003/main/audio/audio_codec.cc"
ninja: fatal: CreateProcess: The parameter is incorrect.
 (is the command line too long?)
```

**这不是编译错误、不是链接错误、不是源码缺陷，也不是环境注入** —— 是 **Windows `CreateProcess` 的 32,767 字符命令行上限**。

### 23.1 精确测量

| 项 | 值 |
| --- | --- |
| 失败命令行长度 | **33,505** 字符 |
| `CreateProcess` 上限 | 32,767 |
| **超出** | **738 字符** |
| `-I` 段数 | **379** |
| 该 TU | `main/audio/audio_codec.cc` |
| 错误码语义 | `ERROR_INVALID_PARAMETER`(87)，ninja 自己都提示 "is the command line too long?" |

### 23.2 根因：**是我的树路径命名把它顶过去的**（用全量 `compile_commands.json` 证明）

用本次构建**真实生成**的 `build-cp2-004/compile_commands.json`（2,422 条）逐条计算：

| 树路径 | 最长命令 | 距上限余量 | **超限条目数** |
| --- | --- | --- | --- |
| `E:/claw4-a05-19fd979/src`（**旧命名**） | **31,759** | 1,008 | **0 / 2,422** |
| `E:/claw4-a05-19fd979/src-cp2-003`（本次） | **33,463** | **−696** | **195 / 2,422** |

- 长命令的 TU 全部含 **212 个树路径引用**（379 个 `-I` 里绝大多数带树前缀）。
- 树路径从 `src`(3) 变成 `src-cp2-003`(11)，**每条 +8 字符 × 212 ≈ +1,700** ⇒ 把原本只剩 1,008 字符余量的整条分布**整体推过悬崖**。
- 交叉印证：`build-cp2-003`（旧命名）的 `compile_commands.json` 最长 **31,759**、**超限 0 条** —— 旧命名确实一直活得下去；是**我在 §21.3 选的目录名 `src-cp2-003` 太长了**。

⚠️ 另需点明：旧命名只剩 1,008 字符余量本身就是**隐患**（任何一个新增 include 都会再爆），所以"改回 `src`"不是好方案。

### 23.3 投影：缩短路径即可彻底解除（同一份真实数据计算）

| 候选（source / build） | 最长命令 | 余量 | 超限条目数 |
| --- | --- | --- | --- |
| `E:\a05c\s` / `E:\a05c\b` | **28,512** | **+4,255** | **0 / 2,422** |
| `E:\a05s` / `E:\a05b` | 28,082 | +4,685 | 0 / 2,422 |
| `E:\a05c\src` / `E:\a05c\build` | 28,948 | +3,819 | 0 / 2,422 |

**强烈建议取 `E:\a05c\s` + `E:\a05c\b`**：一次性拿到 4,255 字符余量，两个路径都短，且不依赖任何系统开关。

**这一改动是内容中性的**：manifest 的树摘要是**按相对路径**计算的，与树放在哪里无关 ⇒ 移树/重放后摘要应仍为 `36bc3d4d…`（应用时会实测验证）。

### 23.4 次要发现（必须记录）

本次构建把 3 个生成文件写入了 `src-cp2-003`，其中**两个连读都读不到**：

| 文件 | 字节 | 可读性 |
| --- | --- | --- |
| `main/i18n/i18n_strings_gen.h` | 78,840 | 可读，sha256 `b58d35a0…` |
| `main/assets/lang_config.h` | 10,639 | **读被拒（Permission denied）** |
| `main/mmap_generate_resources.h` | 18,246 | **读被拒（Permission denied）** |

⇒ §21.2 的 B 要求"记录这 3 个生成路径的 post existence/hash"这一步**在本机算不出那两个文件的哈希**。实测 `verify-build-inputs.py` 的处理是**如实记为 `sha256=UNREADABLE:13` 并继续**（不报错、不影响判定），因此**不阻塞**；但"post hash"对有 ACL 限制的两个文件而言等于"只有 bytes + exists"。这需要一并裁定（可能与本机应用控制策略同源）。

### 23.5 构建后输入复核（本轮手工补跑）：**PASS，无漂移**

runner 的 `[16]` 只在 `rc == 0` 时才跑，本次未跑到，故手工补跑（`inputcheck-after-cp2004.log`）：

```
-- generated paths excluded from the tree digest (state now; NOT compared) --
    main/assets/lang_config.h        exists=True  bytes=10639  sha256=UNREADABLE:13
    main/i18n/i18n_strings_gen.h     exists=True  bytes=78840  sha256=b58d35a0…
    main/mmap_generate_resources.h   exists=True  bytes=18246  sha256=UNREADABLE:13
entries  : 5 checked
failures : 0
RESULT: PASS -- every external input matches the frozen manifest
```

**意义**：① 报 20/§21 担心的"构建写入隔离树 ⇒ 下次会被硬前置拦下"**已消除** —— 排除 3 个生成路径后，`--check` 在**被构建写过的树上依然 PASS**，硬前置可用；② 除这 3 个生成路径外，**树的其余部分逐字节未变**（摘要仍等于冻结值）；③ 顺带证明 §21.2 的 `exclude_files` 在真实构建后场景下按设计工作。

### 23.6 状态与所需裁定

```
CP2 = BLOCKED / Reason = WINDOWS COMMAND-LINE LENGTH LIMIT (32,767) EXCEEDED
                         caused by the replay-tree directory name chosen in 21.3
ARTIFACTS: 2/5 (bootloader.bin, partition-table.bin) -- 非候选证据
```

**本次运行也不构成有效的 CP2 尝试**：它证明了源码修复有效（旧错误消失），但**没有跑到链接阶段**，因而仍未产出 5/5 构件。

**请裁定（属 CP 级：改动冻结输入的路径）**：

| 方案 | 内容 | 评估 |
| --- | --- | --- |
| **A（推荐）** | 重放到短路径：source `E:\a05c\s`、build `E:\a05c\b`；同步更新 manifest 与 runner 默认值 | 内容中性、余量 4,255、实测可证；一次性解决 |
| B | 只把目录名改回 `src` | 仅 1,008 余量，隐患仍在；不推荐 |
| C | 传 `-D CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES=ON` 让 include 走响应文件 | **属修改 CMake 配置，任务书禁止**；未获授权不得实施 |

**未做**：未移动/改名任何树、未改 manifest、未改 runner、未改 CMake/sdkconfig/partition/源码、未装工具、未 Flash、未进 CP3。

---

## 24. CP2-SOURCE-FIX-001 follow-up A：短路径重放（已执行，`-Src E:\a05c\s`）

用户批准方案 A 后执行。**改动仅 3 个文件**：`tools/dev/verify-build-inputs.py`、`integration/metalio_claw4/a05_build_input_manifest.json`、`tools/dev/run-a05-cp2-build.ps1`。未改源码 / CMake / sdkconfig / partition，未装工具，未 Flash，未进 CP3。

### 24.1 新树：干净重放到 `E:\a05c\s`

用**参数化的同一重放脚本**（pin → LF → A01 → learning sync @ `2c8f58f` → A05 → 组件）重放，逐步断言**全部通过**：

| 步 | 断言 | 结果 |
| --- | --- | --- |
| [1] pin 归档解包 | 1,264 文件 / 72,291,311 B / tree `72058f60…` | OK |
| [2] LF 归一化 | 434 文件改写 | OK |
| [3] pin 身份 vs A01 `upstream_sha256` | **4/4 OK** | OK |
| [4] A01 `--check` + apply | rc=0；**4/4 OK** | OK |
| [5] learning sync @ `2c8f58f` | 90 文件；sdkconfig = `a901f204…`；`learning_screen.cc` LF sha == 仓库 blob；**`s_selftest`/`SELFTEST`/`ResetToSeed` 出现 0 次** | OK |
| [6] A05 `--check` + apply | rc=0；`main/CMakeLists.txt` = `14ffc5c3…` == 期望 | OK |
| [7] managed_components 拷贝 | 14,656 文件 / 666,122,132 B；tree `83c759c8…` **match=True** | OK |
| [8] 新树（未排除）| 1,354 文件 / 72,532,983 B | — |
| — | **CLEAN REPLAY: PASS -- every expected hash matched** | **PASS** |

### 24.2 内容中性：**冻结摘要逐字不变（权威口径）**

重放后按 manifest 口径重新测量（`--write`）：

```
  managed_components   83c759c8306b11193feebd06..
  sdkconfig            a901f20491671a9cbca8cbe6..
  isolated_src         36bc3d4d9b847253b90068fd..   ← 与冻结值逐字相同
  origin_file_count    1353      origin_bytes     72457109   ← 与冻结值逐字相同
```

生成路径的 pre 状态也与原树构建前**完全一致**（`lang_config.h` 不存在、`i18n_strings_gen.h` = 75,874 B / `a19b324e…`、`mmap_generate_resources.h` 不存在）⇒ **换路径是内容中性的**，manifest 的树摘要按**相对路径**计算因而保持不变。

> 一处自我更正（如实记录）：我曾在重放脚本尾部追加"内容中性"断言，但原脚本以 `sys.exit(main())` 结尾 ⇒ **该断言是死代码、从未执行**。上面的结论来自 manifest 的权威 `--write`/`--check`，与那段死代码无关。脚本已按此更正说明。

### 24.3 manifest 与 runner 同步更新

⚠️ 实施中发现的一条结构事实：**manifest JSON 是 `verify-build-inputs.py` 内嵌 `INPUTS` 模板的生成产物** —— 直接改 JSON 会被下一次 `--write` 覆盖（我第一次就踩了：改了 JSON，`--write` 又写回旧路径）。**路径定义必须改工具里的 `INPUTS`**。已按此修正。

| 位置 | 改动 |
| --- | --- |
| `verify-build-inputs.py` `INPUTS` | `managed_components.dest` → `E:\a05c\s\managed_components`；`sdkconfig.dest` → `E:\a05c\s\sdkconfig`；`isolated_src.origin` → `E:\a05c\s`（version 里注明短路径迁移） |
| `run-a05-cp2-build.ps1` | `-Src` 默认 → `E:\a05c\s`；`-Build` 默认 → `E:\a05c\b`；头部注释新增"**短源路径是强制要求**"及禁止 `-D CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES` 的说明 |

### 24.4 三重验证

**① 命令长度投影（真实 `compile_commands.json`，2,422 条）**

| | 最长命令 | 余量 | 超限条目 |
| --- | --- | --- | --- |
| 迁移前 `.../src-cp2-003` + `.../build-cp2-004` | 33,463 | **−696** | **195 / 2,422** |
| **迁移后 `E:/a05c/s` + `E:/a05c/b`** | **28,512** | **+4,255** | **0 / 2,422** |

实际失败的那些 `audio_codec.cc` 命令：33,363～33,391 → **28,412～28,440**。

**② 源树绑定守卫（用**新** manifest 重测，脚本内真实片段在子进程执行）**

| 输入 `-Src` | rc | 判定 |
| --- | --- | --- |
| `E:\a05c\s`（新默认） | 0 | 放行构建 |
| `E:\claw4-a05-19fd979\src-cp2-003`（上一棵） | **3** | 拒建 |
| `E:\claw4-a05-19fd979\src`（最初那棵） | **3** | 拒建 |
| `E:\a05c\s\`（尾反斜杠） | 0 | 归一化放行 |
| `E:\A05C\S`（大小写） | 0 | Windows 语义放行 |

**③ 输入校验（新 manifest，构建前硬前置模拟）**

```
entries  : 5 checked
failures : 0
RESULT: PASS -- every external input matches the frozen manifest
```

### 24.5 下一步：重跑（命令）

```powershell
powershell -ExecutionPolicy Bypass -File E:\claw4-a05-build-m0\tools\dev\run-a05-cp2-build.ps1 -Build E:\a05c\b -Fresh
```

`-Src` 可省（默认已是 `E:\a05c\s`，且与 manifest 绑定）。开局应依次看到 `PATH_GUARD: OK` → `source/tree binding ... OK` → `input check PASS` → 冷构建。

**F 停止条件不变**：出现**新**的 compile/link error → 立即停、存日志、不扩大源码修改；`exit=0` + 后验证 PASS → 写 `CP2 REVIEW_READY` 并停止；不进 CP3、不 Flash。

---

## 25. E 第三次执行（2026-09-15 17:55）：✅ **CP2 REVIEW_READY**

### 25.0 结论

**冷构建成功**：`exit=0`、`Project build complete`、**`POST-BUILD VERIFICATION: PASS (items 7-16 satisfied; NO FLASH)`**。用时 **4 分 42 秒**，构建根 `E:\a05c\b`（`-Fresh` 新建），源树 `E:\a05c\s`。

**未经我修改任何源码 / CMake / sdkconfig / partition，未安装任何工具，未 Flash，未开始 CP3。**

### 25.1 三道闸门全过（本轮的树、路径、环境都正确）

```
PATH_GUARD: OK -- os.environ['PATH'] == Win32 PATH (len 874)
-- source/tree binding (-Src vs manifest isolated_src.origin) --
  manifest isolated_src.origin : E:\a05c\s
  OK -- -Src == manifest isolated_src.origin
input check rc = 0
input check PASS -> cold build follows
invoking: idf.py -C <src> -B <build> -D SDKCONFIG=<abs> build
...
Successfully created esp32p4 image.
Generated E:/a05c/b/xiaozhi.bin
Project build complete.
exit=0   elapsed=00:04:42.9007376   end=2026-09-15T18:01:58
```

⇒ 前一节的三类阻塞（宿主 PATH 失同步、上游源码缺陷、命令行长度上限）**全部不再出现**。

### 25.2 §9「最低验收证据」—— **17 / 17 全部产出**

| §9 项 | 证据 |
| --- | --- |
| 1 `verify-build-inputs` 5/5 PASS | `entries 5 checked / failures 0 / RESULT: PASS`（构建**前**） |
| 2 新 build root | `E:\a05c\b` + `-Fresh`（全新，无复用） |
| 3 `idf.py build` exit=0 | `exit=0`（4m42s） |
| 4 configure PASS | cmake 命令已捕获；`CMakeConfigureLog.yaml` 已收割 |
| 5 compile PASS | 全量编译完成，无 error |
| 6 link PASS | `Successfully created esp32p4 image.` |
| 7–11 五个必需构件 | **`required artifacts present : 5 / 5`** |
| 12 每个构件的 size + SHA256 | 见 §25.3（我已**独立重算**，与日志逐字一致） |
| 13 sdkconfig pre/post 相等 | 两侧均 `a901f204…` ⇒ configure 未改写 SDKCONFIG |
| 14 真实分区表反解 + 逐行比对 | **13 行全 OK**；无重叠、在界内（`RESULT: PASS`） |
| 15 app 真实字节数 + `ota_0` 余量 + `ota_1` fits | app **9,271,760 B**；`ota_0` 余量 **165,424 B (1.75%)**；`app fits ota_1 = False` |
| 16 构建后外部输入复核 | 重跑 `--check` → **`failures 0 / RESULT: PASS`（仍 5/5）** |
| 17 明确 `NO FLASH` | runner 打印 `NO flash / erase / monitor / flasher_args was executed.` |

### 25.3 构件（**我独立重算**，与 runner 输出逐字一致）

| 构件 | 字节 | sha256 |
| --- | --- | --- |
| `xiaozhi.bin` | **9,271,760** | `c035e1c09ebe472f5f14b490c93844aa3e298cd11b4e30f7fe1055e5b9278d36` |
| `xiaozhi.elf` | 80,102,764 | `fba68d58002e8139506df655310cabd79f8359f1fff11cfc49d566ea5da8aa60` |
| `xiaozhi.map` | 21,859,377 | `93f8e7d93e35cac0d2d52b073111b72b8069ac7afe431754db84717546b5bafe` |
| `bootloader\bootloader.bin` | 20,416 | `e2a455c5786943bc0c23a2b1cb71482b35763319a9b457d80c7e63fdc3b33f33` |
| `partition_table\partition-table.bin` | 3,072 | `ef0039b6366c57de098972c0f6e9fd991013b41968da9b866c4704cb68ef7e5f` |

> `partition-table.bin` 的 sha256 与 §16/§20/§22 三次由批准 CSV 生成的 bin **完全相同** ⇒ 分区布局自始至终由批准输入决定，与源码树/构建根无关。

### 25.4 真实分区表逐行校验（**对生成物执行，非自测**）

approved CSV `E:\a05c\s\partitions\v1\32m_dual.csv`（LF sha `c5277b4b…`）↔ 生成 bin（sha `ef0039b6…`），**13 行全部 OK**：

```
nvsfactory 0x0000a000/204800 | nvs 0x0003c000/860160 | otadata 0x0010e000/8192
phy_init 0x00110000/4096 | model 0x00111000/978944
ota_0 0x00200000/9437184 | ota_1 0x00b00000/4194304
resources 0x00f00000/4194304 | factory_test 0x01300000/614400
emote 0x01396000/4194304 | system 0x01796000/1048576
storage 0x01896000/7340032 | coredump 0x01f96000/65536
last partition end 0x01fa6000 ; trailing unallocated 368640 B
RESULT: PASS -- matches the approved input, no overlap, in range
```

### 25.5 ⚠️ 必须显式记录的容量事实（本候选，仅应用侧）

| 项 | 值 |
| --- | --- |
| app 映像 | **9,271,760 B** |
| `ota_0` 槽 | 9,437,184 B → **余量 165,424 B（1.75%）** |
| `ota_1` 槽 | 4,194,304 B → **`app fits ota_1 = False`** |
| 结论 | **app > `ota_1` ⇒ 双槽 OTA 不可用**，本包只提交 **application-only** 证据 |

按既有约定：**只记录，不改分区、不改 OTA 策略**；双槽 OTA 的流程/选择/回滚**不属于本包范围**。

### 25.6 附加（只读）证据：ELF 符号保留 —— 关闭此前列为"未完成"的一项

用交叉 `riscv32-esp-elf-nm` 对**真实 `xiaozhi.elf`** 取证，可靠性组件符号**确实被链接进最终镜像**（`T` = 已定义于 `.text`）：

```
T claw4::sync::SyncExecutor::runCycle(std::function<bool ()> const&, int)
T claw4::sync::SyncExecutor::SyncExecutor(AppCoordinator&, SyncTransport&, ...)
T claw4::sync::SingleFlightHttpTransport::request(std::string const&, ...)
T claw4::sync::SingleFlightHttpTransport::SingleFlightHttpTransport(std::function<...>)
V vtable / typeinfo for claw4::sync::SingleFlightHttpTransport
（map 中另有 TimeAuthority / InteractionArbiter / ReminderCore 等一组符号）
```

⇒ `-ffunction-sections -fdata-sections` + gc-sections **没有**把这些组件裁掉。该项此前只在"未完成"清单里，现由真实构件关闭。

### 25.7 诚实的边界（不粉饰）

1. **本包只验证"构建"**：未烧录、未上机、未做设备行为/monitor 验证 —— 那是 **CP3**，**未获授权，未开始**。
2. **Host 门禁仍是 28/29**（`learning_mcp_host_tests.exe` 被本机应用控制策略拦截，零断言失败，见 §21.5）；与本轮构建无因果，仍为环境遗留项，**不声称 29/29**。
3. **`isolated_src` 构建后复核之所以能 PASS，依赖于 3 个生成路径被排除**（`main/assets/lang_config.h`、`main/i18n/i18n_strings_gen.h`、`main/mmap_generate_resources.h`）。这 3 个路径本次**均可读**（`927c3ad3…` / `b58d35a0…` / `658c693a…`），与上一次两个 `UNREADABLE:13` 不同。**它们的字节会在每次构建变化，这不构成输入漂移**（除它们之外，树摘要与冻结值一致）。
4. **`ota_1` 不可用**（§25.5）是本候选的既成事实，如实上报而非回避。
5. 证据文件不在 Git 内：`E:/claw4-a05-19fd979/logs/cp2-build-20260915-175552.log`（已归档到 `E:/claw4-a05-cp1-fix-recon/logs/cp2-build-20260915-175552.SUCCESS.log`）、`cp2-postbuild-verification.txt`、`cp2-artifacts-and-symbols.txt`、`cp2-elf-symbols.txt`。

### 25.8 状态

```
CP2 = REVIEW_READY
  source            : E:\a05c\s   (isolated_src 36bc3d4d…, 1353 files, 72,457,109 B)
  build root        : E:\a05c\b   (fresh)
  idf.py exit       : 0            elapsed 4m42s
  artifacts         : 5 / 5
  sdkconfig         : a901f204… pre == post
  partition         : 13/13 rows OK, no overlap, in range
  app / ota_0 margin: 9,271,760 / 165,424 B (1.75%)
  ota_1 fits        : False  -> dual-slot OTA unusable (recorded, no partition change)
  flash             : NONE
  CP3               : NOT AUTHORIZED / NOT STARTED
```

**按要求停止：不再执行任何构建/烧录/后续阶段，等待 Codex 复核。**

---

## 26. CP3：链接、来源与既有缺陷审计（2026-09-16）

| 项 | 值 |
| --- | --- |
| 任务 ID | WB-A05-BUILD-001 / CP3 |
| 分支 | `workbuddy-a05-build-m0` → 远端 `workbuddy/a05-build-m0` |
| 被审对象 | 候选 `E:\a05c\b`（源树 `E:\a05c\s`，`isolated_src` `36bc3d4d…`） |
| 本轮修改文件 | **仅本报告**（审计任务，不改产品源码/CMake/sdkconfig/partition） |
| 前置 | CP2 = REVIEW_READY（`00b27e7`），Codex 已复核通过并放行 CP3 |

### 26.0 方法：为什么必须同时看 object / archive / map / ELF

`libmain.a` 在链接时是 **`--whole-archive`**（map 首节对每个成员给出的加载理由就是字面量 `( --whole-archive )`，见 `xiaozhi.map:489/491/493/495/497`）。**这意味着"归档里有符号"这件事对所有成员恒为真、不具区分力**。真正的判据只有两个：

1. **链接 map 的 `Discarded input sections`**（第 4536 行起）——成员是否被加载、哪些节被 `--gc-sections` 丢弃（size `0x0`）；
2. **最终 ELF 的符号表**——谁真的留在镜像里。

**因此本审计不把 archive 符号当最终 ELF 符号**：`libmain.a` 里这些模块各有 6/13/45/11/6/33 个 `claw4::` 符号，而最终 ELF 里只有 3/3/8/0/0/0。

### 26.1 模块矩阵（逐行五列；按任务书 §4 的三组）

**组 1：必须 Compile + Reference + 最终链接证据**

| 模块 | 源文件 → object | Archive | **ELF** | Referenced（引用点） | Runtime-wired |
| --- | --- | --- | --- | --- | --- |
| **SyncExecutor** | `learning/sync/sync_executor.cpp` → 579,552 B `d1c5569c…` | ✅ libmain.a（whole-archive） | ✅ **RETAINED**：`3 T` | `learning_backend_session.cpp:283` `SyncExecutor executor(app_, sync_transport_, …)` | ✅ 经 `LearningBackendSession` |
| **SingleFlightHttpTransport** | `learning/sync/single_flight_http_transport.cpp` → 200,828 B `9dbf5f74…` | ✅ | ✅ **RETAINED**：`3 T` + `3 V`（vtable/typeinfo）+ `3 W` | `metalio_http_transport.cpp:52` `make_unique<SingleFlightHttpTransport>(PerformRequest)` | ✅ 经 `CreateMetalioHttpTransport()` |
| **BackendSession**（`LearningBackendSession`） | `learning/sync/learning_backend_session.cpp` → 812,140 B `387a4620…` | ✅ | ✅ **RETAINED**：`8 T` + `2 d` + `2 t` | `learning_runtime.cpp:64` `make_shared<LearningBackendSession>(…)` | ✅ 设备运行链 |

**运行时调用链（逐跳有源可查，非推断）**：
`display/screen/learning_screen/learning_screen.cc:90` `LearningRuntime::Instance()` → `learning_runtime.cpp:64` 构造 `LearningBackendSession` → 其内部 `sync_executor.cpp` 的三相纪律 → `CreateMetalioHttpTransport()` → `single_flight_http_transport.cpp`。

> **如实标注**：`SingleFlightHttpTransport` 只有 `request()` 与构造函数留在 ELF；同一 TU 的 `rejectedBusy()` / `inFlight()` 两个节出现在 map 的 `Discarded input sections`（size `0x0`）——它们没有活引用。**线上的那条路径被完整保留，诊断用访问器被裁掉**，这符合预期，不算缺陷。

**组 2：必须纳入真实目标编译、对象/归档可定位；允许被最终 ELF 裁剪**

| 模块 | 源文件 → object | Archive | **ELF** | Referenced | Runtime-wired |
| --- | --- | --- | --- | --- | --- |
| **TimeAuthority** | `learning/time/time_authority.cpp` → 265,148 B `7fbda167…` | ✅ | ❌ **GC_DISCARDED**（ELF 中 0 命中；map 中该成员全部节 size `0x0`） | 仅 `reminder_core.cpp:16`（其本身也未接线） | ❌ **COMPILED_NOT_WIRED** |
| **InteractionArbiter** | `learning/interaction/interaction_arbiter.cpp` → 300,348 B `1f8b7e16…` | ✅ | ❌ **GC_DISCARDED** | 全树仅自身 `.cpp`/`.h`（无外部引用） | ❌ **COMPILED_NOT_WIRED** |
| **ReminderCore** | `learning/reminder/reminder_core.cpp` → 2,409,708 B `f332d000…` | ✅ | ❌ **GC_DISCARDED** | 全树仅自身 `.cpp`/`.h` | ❌ **COMPILED_NOT_WIRED** |

判定链条（`TimeAuthority` 为例，map `xiaozhi.map:493` + `:17330-17334`）：
```
--whole-archive 加载 → map 记为已加载成员
  .group  0x00000000  0xc  ...(time_authority.cpp.obj)
  .text   0x00000000  0x0  ...(time_authority.cpp.obj)   ← gc-sections 丢弃
  .data   0x00000000  0x0
  .bss    0x00000000  0x0
⇒ 最终 ELF 中 0 个符号（`grep -c TimeAuthority nm-elf-all.txt` = 0）
```
**按任务书要求：不强行保留。** 若后续要求休眠模块实际进入产品镜像，**另立接线任务**，不在本包隐式扩大范围。

**组 3：头文件需在真实目标 TU 中类型编译；模板/纯接口不要求独立 nm 符号**

| 模块 | 形态 | 在真实目标 TU 中的编译证据 | ELF 符号 | Runtime-wired |
| --- | --- | --- | --- | --- |
| **ReminderWakePort** | `learning/ports/reminder_wake_port.h`（纯接口） | 被 `reminder_core.h:34` 包含 → `reminder_core.cpp` 编译成功（object 2,409,708 B 已生成） | 不要求（纯接口）→ ELF 0 命中，**符合预期** | ❌ 否（wake 设备实现留 **C03/E01**） |
| **SessionLease**（含 Holder/Source） | `learning/sync/session_lease.h` | 被 `learning_runtime.h:16`、`learning_backend_session.h:39`、`session_snapshot_publisher.h:31` 包含，对应 object 均已生成 | ✅ **RETAINED**：`SessionLeaseHolder<LearningBackendSession>` 的 `vtable`/`~Hold`/`currentLease`、`SessionLeaseSource::isCurrent(SessionLease const&)`（`T`）均在 ELF | ✅ 运行时真实使用 |
| **SessionSnapshotPublisher** | `learning/sync/session_snapshot_publisher.h` | 被 `learning_runtime.cpp:19` 包含并在 `:120` **实际调用** `publishSessionSnapshot(...)` | 无独立 nm 符号（**已全内联**）——按任务书属"不要求独立符号" | ✅ 运行时真实调用 |

### 26.2 D5 核查 ①：`learning_screen` 的 tick deadline 与 timer 删除

源文件 `main/display/screen/learning_screen/learning_screen.cc`：696 行 / 27,352 B / **LF sha256 `a41aeed7b10dfab5aece840e0c582c8d9d3eb2e71805d78e7caafdabccbad809`**。

| 项 | 事实 |
| --- | --- |
| tick 周期 | `kRefreshMs = 1000`（1 s 刷新，`lv_timer_create(OnRefreshTick, kRefreshMs, …)` @690）；`kVoicePollMs = 50`（@693）；`kVoiceWindowMs = 15000` |
| deadline 语义 | `s_voice_deadline = lv_tick_get() + kVoiceWindowMs`（@438）；判定 `static_cast<int32_t>(lv_tick_get() - s_voice_deadline) >= 0`（@537-538）——**带符号差值，回绕安全** |
| 时间窗超时 | **复用本 timer，不新增/删除任何 timer**（@536-541 注释明示） |
| **timer 删除** | 全文件恰好 **2 处** `lv_timer_del`：`@494 s_ui.timer`、`@498 s_voice_poll_timer`，各自 `!= nullptr` 守卫后**立即置 nullptr** ⇒ **无重复删除** |
| 悬空引用 | `s_selftest_timer` / `SELFTEST` / `ResetToSeed` **0 处**（§21.1 的删除已生效） |

**代码 hash 独立于 vendor patch（已证）**：树内该文件 LF 摘要 `a41aeed7…` **等于仓库 blob @`2c8f58f`**（逐字节）；而 A01 vendor 补丁 `project-ca3aa3fa.patch` 对 `learning_screen.cc` 的**唯一一处提及，是把它加进 CMake 源列表**（补丁第 57 行 `+"display/screen/learning_screen/learning_screen.cc"`），**没有改这个文件本身**。

### 26.3 D5 核查 ②：audio yield、锁作用域、`pdMS_TO_TICKS(1)` 的真实值

**现有 patch**：A01 `integration/metalio_claw4/patches/project-ca3aa3fa.patch`（sha256 `58bfbe52…`），对 `main/audio/audio_service.cc` 的 hunk `@@ -239,10 +239,17 @@` **就是这次 yield 修复本身**：

```diff
-                    std::lock_guard<std::mutex> lock(wake_word_mutex_);
-                    if (wake_word_initialized_ && wake_word_) {
-                        wake_word_->Feed(data);
+                    {
+                        std::lock_guard<std::mutex> lock(wake_word_mutex_);   ← 锁收进内层作用域
+                        if (wake_word_initialized_ && wake_word_) {
+                            wake_word_->Feed(data);
+                        }
                     }
+                    // 锁已在内层作用域释放，此处 sleep 不会阻塞 wake_word_ 访问
+                    vTaskDelay(pdMS_TO_TICKS(1));                             ← delay 在锁外
                     continue;
```

**锁作用域**：正确。`lock_guard` 的生命周期止于内层 `{}`（源码 `audio_service.cc:242-249`），`vTaskDelay` 在 `:252`、位于锁外 ⇒ 让出 CPU 时**不持有** `wake_word_mutex_`。

**树内文件身份**：`audio_service.cc` LF sha256 = `f406e12a2fde1209e6e103727d2d0e107b0c41a2ffa8d22346a69d6c66cc58a2` = A01 元数据的 `patched_lf_sha256`（**一致**）。

**`pdMS_TO_TICKS(1)` 的真实结果 —— 用真实编译标志做静态断言，不口算**：

- 宏定义（`esp-idf/components/freertos/FreeRTOS-Kernel/include/freertos/projdefs.h:46`）：
  `( (TickType_t)( (TickType_t)(x) * (TickType_t)configTICK_RATE_HZ ) / 1000U )`
- `configTICK_RATE_HZ = CONFIG_FREERTOS_HZ`（`FreeRTOSConfig.h:92`），批准冻结配置里 **`CONFIG_FREERTOS_HZ=1000`**（`sdkconfig:1896`）
- 取证方式：取 `compile_commands.json` 中 **`main/audio/audio_service.cc` 的真实编译命令**（515 个 argv、保留 513 个含全部 `-I`/`-D`），把输入换成一个只含 `static_assert` 的 TU，`-fsyntax-only` 编译（命令行按 `CommandLineToArgvW` 规则切分，与 Windows 一致）。

```
configTICK_RATE_HZ == 1000                    ✔
pdMS_TO_TICKS(1)   == 1                       ✔
pdMS_TO_TICKS(10)  == 10 / (50) == 50 / (120) == 120   ✔
(1 * 100) / 1000   == 0                       ✔   ← 反事实
RESULT: PASS -- all static_asserts held under the REAL compile flags.
```

⇒ **本候选下 `pdMS_TO_TICKS(1)` = 1 tick = 1 ms。**

⚠️ **历史"10 ms"的描述必须作废**（任务书要求不得沿用）：`pdMS_TO_TICKS(1)` 在 **HZ=100** 时是 `(1×100)/1000 = 0` tick，即 **0 ms —— 完全不让步**，不是 10 ms。把"1 tick = 10 ms"错读成"`pdMS_TO_TICKS(1)` = 10 ms"是整数截断造成的经典误读。**本包未改 yield 实现**（只读审计）。

**并且**：静态存在 ≠ 20 轮真机稳定。本节只证明"编译产物对该 yield 的求值正确、锁作用域正确"，**不主张** audio 路径在真机上已稳定。

### 26.4 完整 Host Gate 重跑：**29 / 29 PASS**

因产品源码有经准许的编译修订（`learning_screen.cc`，§21.1）⇒ 按任务书必须重跑，已重跑：

```
HOST_GATE_CP3 = 0
unit summary : 29 / 29 PASS
interface: exit=0   (P4 交叉编译)
RESULT: NATIVE CPP TEST GATE PASS
RUN FAIL = 0   LAUNCH FAIL = 0   failures=[1-9] = 0
```

**顺带解决 §21.5 / §25.7 的遗留**：此前 6 次重跑卡在 `learning_mcp_host_tests.exe` 被本机应用控制策略拦截（28/29）；**本次该 exe 被放行，29/29 达成，零断言失败**。⇒ 该环境遗留项**已闭合**，不再是未决项。

### 26.5 必须**单列**（不得用 Host/Build 证据替代）的设备侧项

| # | 项 | 本包覆盖情况 | 依据/现状 |
| --- | --- | --- | --- |
| 1 | **TCP loopback** | ❌ 未做，**单列** | Codex 规划审阅已记为 `ENV_VERIFY_REQUIRED`，明确"不能凭 socket 桩改成 TCP 已通过" |
| 2 | **底层网络总 deadline** | ❌ 未解决，**单列** | `EspTcp::Connect()` 不继承 HTTP timeout、`Disconnect()` 可能等 10 s（WB_V53_NEXT_001 §7.3 已记录）。本包**不通过改 managed component 解决** |
| 3 | **UI 延迟** | ❌ 未测，**单列** | 设备侧指标；本包只有 Host 侧"UI 不堵"的设计证据，二者不等价 |
| 4 | **真 NVS / 掉电** | ❌ 本包未接设备，**单列** | 历史设备证据 `DEVICE_L1C_PERSISTENCE=PASS`（2026-09-04）属**另一批次**，不作为本包证据 |

### 26.6 范围偏差与自查

| 检查项 | 结论 |
| --- | --- |
| 修改范围 | **仅本报告**；未改源码 / CMake / sdkconfig / partition / bootloader |
| 是否自行扩大范围 | **否**。未实现"休眠模块接线"，未改 OTA/分区策略，未碰 yield 实现 |
| 是否 Flash / 接设备 | **否**（NO FLASH；未接串口、未 monitor） |
| 是否宣称跨机器可复现 | **否**。仅声明本机、本工具链、本 manifest 下可重跑 |
| 是否把 archive 符号当 ELF 符号 | **否**（26.0 明示区别，并给出两列数字） |

### 26.7 建议 Codex 的复检重点

1. **三类判定口径**：`RETAINED`（组1）/ `GC_DISCARDED` + `COMPILED_NOT_WIRED`（组2）/ "类型编译 + 内联无独立符号"（组3）是否符合你对任务书 §4 的解读。
2. **`--whole-archive` 的推论**：我据此主张"Archive 列对本仓库不具区分力、判据必须落在 map + ELF"，请确认这个方法论可接受。
3. **D5 的 1 ms 结论**：我用真实编译命令 + `static_assert` 取证（而非口算），并据此作废历史"10 ms"描述；请确认是否需要在报告或看板里显式标记该历史描述 `SUPERSEDED`。
4. **组2 是否需要在 CP4 前另立接线任务**：按任务书我**没有**强行保留它们；若产品镜像需要 Reminder/TimeAuthority，请另立任务（C03/E01）。

### 26.8 本轮证据文件（不入 Git）

| 路径 | 内容 |
| --- | --- |
| `E:/claw4-a05-cp1-fix-recon/logs/nm-elf-all.txt`（22,520 行） | `riscv32-esp-elf-nm -C` 对最终 `xiaozhi.elf` 的完整符号表（26.1 的 ELF 列全部由此得出） |
| `E:/claw4-a05-cp1-fix-recon/logs/nm-libmain.txt`（13,263 行） | 对 `libmain.a` 的符号表（用于"归档 vs ELF"对照） |
| `E:/a05c/b/xiaozhi.map` | 链接 map：`Archive member included…`(1 行起) 与 `Discarded input sections`(4536 行起) |
| `E:/claw4-a05-cp1-fix-recon/logs/probe-pdms-to-ticks.txt` + `probe_pdms.cc` + `probe_pdms_driver.py` | 26.3 的 `static_assert` 取证（真实编译命令、真实 `-I`/`-D`） |
| `E:/claw4-a05-cp1-fix-recon/logs/host_result-cp3.txt` + `E:/claw4-a05-build-m0/out/a05-cp3-host/` | 26.4 的 29/29 完整门禁日志 |

---

## 附录 A. 本轮取证工作产物（非交付物，均在本机）

| 路径 | 内容 |
| --- | --- |
| `E:/workbuddy/claw4-a05-cp0-recon/evidence.md` | sdkconfig 三候选 hash + 关键值、分区表、IDF/工具版本、组件清单、A01 身份 |
| `E:/workbuddy/claw4-a05-cp0-recon/evidence2.md` | lock ↔ managed_components 逐项对照、A01 四文件实测 hash、资源清单 |
| `E:/workbuddy/claw4-a05-cp0-recon/evidence3.md` | `E:/c` vs `MetalioClaw4-ascii` 全量逐文件比对 |
| `E:/workbuddy/claw4-a05-cp0-recon/branch-attempt*.txt` / `path-diag.txt` / `reftest.txt` / `clone-e-root.txt` | Git 引用缺陷的逐次命令与输出（§1.3 证据） |
| `E:/workbuddy/claw4-a05-cp0-recon/blocked-evidence.md` | 5 项阻断套件的 exe hash 与单次运行结果 |
| `E:/workbuddy/claw4-a05-cp0-recon/host-fingerprint.json` / `.txt` | §12.2 的 Host 复用指纹基线（156 文件树哈希 + 两编译器哈希 + 参数哈希 + 合成指纹） |
| `E:/workbuddy/claw4-a05-cp0-recon/partition_expected.txt` | §12.3 已批准分区 CSV 经 IDF 工具解析后的期望布局、归一化哈希、尾部未分配空间与逐条断言 |
| `E:/workbuddy/claw4-a05-cp0-recon/pin-source.txt`、`pin-replay.txt`、`pin-vs-mirror.txt` | §13.1 pin 仓库身份、补丁前向/反向重放、`E:/c` vs 真实 pin 差异账目 |
| `E:/workbuddy/claw4-a05-cp0-recon/component-content-verify.{txt,json}`、`component-content-negative-control.txt` | §13.2 82 组件内容哈希校验 + 单比特篡改负向对照 |
| `E:/claw4-a05-build-m0/out/a05-cp0-host/` | 本轮完整 Host 门禁日志、逐套件输出、`host_result.txt` |
| `E:/claw4-a05-cp1-fix-recon/` | §16 取证：`HEAD_verify-partition-table.py`（修复前版本）、`FALSIFIED_verify-partition-table.py`（证伪副本）、`normal.bin`/`mutated.bin`（批准 CSV 与其单行变异各自生成的真实分区表）、`logs/*.log`（修复前后对照、自测、证伪、指纹复核） |
| `E:/claw4-a05-cp1-fix-recon/logs/probe-*.{py,txt}` | §17（ENV-DIAG-001）取证：`probe-realenv-{PREPEND,NOPREPEND}.txt`（`os.environ` vs `GetEnvironmentVariableW` 对照）、`probe-site-variants.txt`（`-S`/`-E -S`/去 `PYTHONPATH` 判别）、`probe-minimal-repro.txt`（最小复现）、`probe-path-dump*`、`probe-shim-listing.txt` |
| `E:/claw4-a05-19fd979/build-envdiag-001/` | §17 全新构建根（**零构件**，仅 configure 残留）：`CMakeFiles/CMakeConfigureLog.yaml`、`log/idf_py_{stdout,stderr}_output_36216`、`toolchain/` |
| `E:/claw4-a05-19fd979/logs/envdiag-*` | §17 ENV-DIAG 专段、idf.py 实际 cmake 命令、`export` rc/stdout/stderr、收割的 configure 日志目录 |
| `E:/claw4-a05-19fd979/logs/envdiag-pathguard.py` | §18 P0 PATH 守卫脚本（同进程 PATH 一致性判定） |
| `E:/workbuddy/claw4-a05-cp0-recon/cp2_clean_replay.py`、`cp2-clean-replay.{json,txt}` | §21.3 干净重放脚本与逐步断言日志（pin → LF → A01 → sync @ `2c8f58f` → A05 → components） |
| `E:/workbuddy/claw4-a05-cp0-recon/host-fingerprint-cp2-sourcefix.json` | §21.5 重算后的 Host 指纹基线（`eec141fd…`） |
| `E:/claw4-a05-cp1-fix-recon/logs/selfcheck-exclude-files.py` | §21.2 `exclude_files` 的判别性自测（9 例含反向用例 + 目录级排除 + 排除生效的反向对照） |
| `E:/claw4-a05-build-m0/out/a05-cp2-sourcefix-host/` | §21.5 Host 门禁日志（含 `host_result.txt` 与 6 次运行的留档副本 `host_result-run{2..6}.txt`，位于 recon/logs 下） |
| `E:/claw4-a05-19fd979/logs/cp2-build-20260915-13*.log` | §18/§19 守卫拒绝构建的实跑日志（`invoking: idf.py` = 0） |
| `E:/claw4-a05-cp1-fix-recon/logs/selfcheck-binding.txt` + `harness-binding.ps1` | §22.4 源树绑定守卫判别性自测：`harness-binding.ps1` 是**从 runner 原样抽取**的真实守卫片段，5 例输入（新树/旧树/尾反斜杠/大小写/无关树）逐例退出码 |
| `E:/claw4-a05-19fd979/logs/cp2-build-20260915-165252.log` + `build-cp2-003/log/idf_py_{stdout,stderr}_output_*` | §22 E 首次执行的完整日志：第 9 行 `source = ...\src`（旧树）证明本次构建了错误的树；`learning_screen.cc:497` 为失败点 |
| `E:/claw4-a05-19fd979/build-cp2-004/compile_commands.json`（+ `build-cp2-003/compile_commands.json`） | §23 命令行长度的**全量实测依据**（2,422 条），两个命名下的最长命令/超限条目数均由它直接算出 |
| `E:/claw4-a05-19fd979/build-cp2-004/log/idf_py_stderr_output_42124` | §23 失败原文：`CreateProcess failed ... ninja: fatal: ... (is the command line too long?)` |
| `E:/claw4-a05-cp1-fix-recon/logs/inputcheck-after-cp2004.log` | §23.5 构建后输入复核 PASS（含 `UNREADABLE:13` 的如实记录） |
| `E:/workbuddy/claw4-a05-cp0-recon/cp24_replay_short.py` + `cp24-replay-console.log` + `cp24-clean-replay.{json,txt}` | §24.1 短路径干净重放的脚本与逐步断言日志（`NEW = E:\a05c\s`）；仅 `NEW` 与输出名相对原脚本改动 |
| `E:/claw4-a05-cp1-fix-recon/logs/cmdline-projection-shortpath.log` | §24.4① 迁移前后命令行长度的全量投影（2,422 条） |
| `E:/claw4-a05-cp1-fix-recon/logs/selfcheck-binding2.{txt}`、`harness-binding2.ps1` | §24.4② 用**新** manifest 重测绑定守卫（5 例，含两棵被拒的旧树） |
| `E:/claw4-a05-cp1-fix-recon/logs/manifest-write-shortpath2.log` | §24.2 权威 `--write` 输出：`isolated_src = 36bc3d4d…` 与冻结值一致 |
| `E:/claw4-a05-cp1-fix-recon/logs/cp2-build-20260915-175552.SUCCESS.log` | §25 **成功冷构建**的完整日志（`exit=0`、`Project build complete`、`POST-BUILD VERIFICATION: PASS`）；原件在 `E:/claw4-a05-19fd979/logs/` |
| `E:/claw4-a05-cp1-fix-recon/logs/cp2-postbuild-verification.txt` | §25.2–25.5 构建后验证原文（§9 第 7–16 项、真实分区表 13 行、余量与 `ota_1` 结论） |
| `E:/claw4-a05-cp1-fix-recon/logs/cp2-artifacts-and-symbols.txt`、`cp2-elf-symbols.txt` | §25.3 五个构件**独立重算**的 size/sha256；§25.6 `riscv32-esp-elf-nm` 对真实 ELF 的符号过滤输出 |
| `E:/claw4-a05-19fd979/logs/envdiag-*20260915-175552*` | §25.1 该次运行的 ENV-DIAG 专段与 cmake 实际命令 |
| `E:/claw4-a05-cp1-fix-recon/logs/nm-elf-all.txt`、`nm-libmain.txt` | §26.1 最终 ELF 与 `libmain.a` 的完整符号表（模块矩阵 ELF/Archive 两列的唯一来源） |
| `E:/claw4-a05-cp1-fix-recon/logs/probe-pdms-to-ticks.txt`、`probe_pdms.cc`、`probe_pdms_driver.py` | §26.3 audio yield 取证：用**真实编译命令**对 `pdMS_TO_TICKS(1)` 做 `static_assert` |
| `E:/claw4-a05-build-m0/out/a05-cp3-host/`、`E:/claw4-a05-cp1-fix-recon/logs/host_result-cp3.txt` | §26.4 CP3 完整 Host 门禁日志（**29/29**、interface exit=0） |

## 附录 B. 报告口径

- 不使用 `CI PASS`（无 CI）。
- 不把 socket 桩写作 TCP loopback 通过。
- 不因 Host 门禁 PASS 推断固件可构建、Device Gate 或真机行为。
- 未解决的项逐一列出，不做合并粉饰。
