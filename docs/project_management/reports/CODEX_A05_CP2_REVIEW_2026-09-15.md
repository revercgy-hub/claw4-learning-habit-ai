# A05 CP1/CP2 交付审查

日期：2026-09-15。审查：Codex。被审提交：`00b27e78bfb4dc4a684e477467d7f44bbbb45bfa`，WorkBuddy 本地 HEAD 与 fetch 后远端一致，工作树干净。产品源变更提交为 `2c8f58f`；此前产品基线 `19fd979`。本次只审查、运行既有验证和临时负向探针、更新管理文档，未修改产品或工具实现，未运行 IDF 构建或操作设备。

**结论：本次 CP2 冷构建成功事实确认；CP1/CP2 工具与交付整体为 CHANGES_REQUIRED。** 现有候选证据保留，不要求先重做 CP0 或重复冷构建。先由 WorkBuddy 完成下列有界整改，再提交复核；CP3 保持 QUEUED。

## 一、已独立确认的结果

| 项目 | Codex 复核 |
| --- | --- |
| 实际构建来源 | `E:/a05c/b/CMakeCache.txt` 指向源 `E:/a05c/s`、显式 SDKCONFIG `E:/a05c/s/sdkconfig`、target esp32p4、预期 Ninja；该目标是 P4 主芯片，C5 是协处理器，不能把主 target 改成 esp32c5 |
| 冷构建 | 成功日志包含完整构建完成、exit=0、post-build PASS；本轮没有重编译，冷构建过程依据留存日志与新 build root 记录 |
| 全部外部输入 | 在被审提交运行 `verify-build-inputs.py --manifest integration/metalio_claw4/a05_build_input_manifest.json --check`，5 entries / failures 0 / exit 0 |
| 安装后的组件 | 对 `E:/a05c/s/dependencies.lock` 与 `managed_components` 运行现有内容校验，83 锁条目含 idf，实际内容 82/82，failures 0 |
| 分区生成物 | 对实际 partition-table.bin 与源树 CSV 执行既有 verifier，13 行一致、无重叠、32MB 范围内；table offset 0x9000 |
| 学习代码同步 | 按现有同步脚本的 14 个目录映射，逐个比较 `2c8f58f` Git blob 与隔离树，87 文件经 CRLF/LF 归一后全部一致 |
| A05 补丁 | patch blob SHA 与 JSON 一致；隔离临时 fixture 固定 LF、core.autocrlf=false 后反向/正向均 exit 0，前后 LF hash 均匹配。源树 CMake 原字节为 CRLF，原字节 hash 与 LF hash 不混用 |
| 新增编译单元 | compile_commands 中 sync_executor、single_flight_http_transport、time_authority、interaction_arbiter、reminder_core 各 1 条真实编译命令 |
| 最终链接 | 独立对真实 ELF 执行交叉 nm：SyncExecutor::runCycle、SingleFlightHttpTransport::request 均为已定义 T 符号。其它模块仍按 CP3 矩阵区分对象/归档/ELF，不能因 map 出现名称就认定保留或设备接线 |
| 产品源码修正 | `2c8f58f` 仅删除 learning_screen unload 中已无定义的 selftest timer 清理四行，与此前撤除自测一致，未扩展业务语义 |

五个构件已由 Codex 独立重算，全部匹配 WorkBuddy §25.3：

| 文件（相对 E:/a05c/b） | 字节 | SHA256 |
| --- | ---: | --- |
| xiaozhi.bin | 9271760 | c035e1c09ebe472f5f14b490c93844aa3e298cd11b4e30f7fe1055e5b9278d36 |
| xiaozhi.elf | 80102764 | fba68d58002e8139506df655310cabd79f8359f1fff11cfc49d566ea5da8aa60 |
| xiaozhi.map | 21859377 | 93f8e7d93e35cac0d2d52b073111b72b8069ac7afe431754db84717546b5bafe |
| bootloader/bootloader.bin | 20416 | e2a455c5786943bc0c23a2b1cb71482b35763319a9b457d80c7e63fdc3b33f33 |
| partition_table/partition-table.bin | 3072 | ef0039b6366c57de098972c0f6e9fd991013b41968da9b866c4704cb68ef7e5f |

## 二、必须整改

### R1 / P1：删除前缺少构建目录保护，且未强制冷构建

`tools/dev/run-a05-cp2-build.ps1:137-141` 将调用者的 `-Build` 直接交给递归 Remove-Item，发生在 source binding 和输入验证之前。误传 `-Build` 为源树、工具链或项目目录时，`-Fresh` 会先删除该目录；目前只有路径存在性检查。另一方面不带 Fresh 时允许复用已有 build，却仍输出 cold build follows。

要求：任何写入/删除前规范化并检查构建根；限定明确的专用构建父目录，拒绝盘符根、源树/仓库/SDK/工具链/日志/候选目录及其祖先或重叠路径，处理 junction/reparse point。优先采用不存在的新构建目录；若保留删除功能，只删除经验证且带本任务所有权标记的构建目录。已有缓存不得被当作 cold build。用临时 fixture 验证拒绝时 sentinel 保留，绝不拿真实目录测试删除。

### R2 / P1：清单缺少身份字段仍能 PASS

`tools/dev/verify-build-inputs.py:336-342` 只比较 reference 中存在的键。Codex 使用临时 JSON `{"entries":[{"id":"isolated_src"}]}` 配合 `--check --only isolated_src`，实际得到 **rc=0 / 1 checked / failures 0 / PASS**，虽完全没有期望 hash。`--only typo` 也得到 **rc=0 / 0 checked / PASS**。

同类缺口：`verify-component-content.py:100-143` 用仅含 idf 的合法锁文件时，实际 **verified 0 / failures 0 / rc=0**，不满足至少一个真实组件的门禁。此次正式清单实际完整且 82 个组件已验证，负向探针不证明本次产物损坏，但证明工具尚非严格失败门禁。

要求：按 kind 校验必需路径、hash 格式、计数和排除规则；缺少字段、重复 ID、未知 ID、非法 only、零检查项应失败。完整检查必须覆盖全部预期输入。组件锁应可靠解析 dependencies，至少有一个真实组件；A05 应验证预期组件集合，不能让未登记目录悄悄进入构建。新增缺 hash、空集、仅 idf、重复条目、额外目录和正常输入的判别性测试。

### R3 / P2：实际 IDF/工具链仍未绑定冻结清单

runner 的 `-IdfPath`、`-IdfTools` 可覆盖实际 IDF 与 Python/编译工具目录，但 `verify-build-inputs.py` 测量的是内部 INPUTS 固定路径；source binding 只绑定了 Src。使用另一套 SDK/工具链，原目录未变时仍可能通过原清单验证，重现“验证 A、使用 B”的同类问题。verifier 也未比较 origin/dest 的实际值。当前成功日志使用默认路径，没有证据表明此次用了错误工具链。

要求：执行任何工具/构建前，将实际 Src、SDKCONFIG、IDF、IDF_TOOLS、组件目标和批准 CSV 绑定到同一清单；路径和实际解析到的关键工具身份不一致即拒绝。移除不需要的自由覆盖参数或严格验证，不能靠重新冻结错误目录掩盖差异。用不触发真实构建的参数/路径负向测试验收。

### R4 / P2：阶段定义与当前状态未同步

WorkBuddy 报告 §25.7 第 1 项（2295 行）把设备行为/monitor 说成 CP3；任务书 §4 明确 CP3 是“链接、来源与既有缺陷审计”。看板、任务书入口和根 AGENTS 仍写 CP0 READY / CP1 不得开工，最新完成事实只堆在实施报告中。

本次管理文档已追加现状覆盖，保留历史记录。WorkBuddy 需将其报告首部收敛为单一当前状态、修正 §25.7，并把 §14.0 的用户覆盖授权记录与历史 Codex 门禁分开陈述。授权记录来自 WorkBuddy 报告，本次不据此补写前序工作流 ACCEPTED。CP3 为无硬件审计，CP4 为候选冻结；A05-DEVICE 另行授权。

## 三、证据边界与剩余风险

- ota_0 9437184 B，本 app 剩 **165424 B / 1.75%**；ota_1 4194304 B 无法容纳本 app。此事实不要求本包改分区或做尺寸优化，但后续新增功能须预算尺寸；禁止把该候选当成双槽 OTA 可用。
- Host 最新实测日志仍 **28/29**，learning_mcp_host_tests 为进程启动阻断（exit=-1），不是已证明的断言失败，也不能写成 29/29。此次不重跑全部 Host。前序整体可靠性验收和硬件行为验收均未由此次构建审查替代。
- 三个生成头仍从输入树摘要精确排除。本轮审查进程对其中两个读到 UNREADABLE:13；这是当前审查可读性限制，不否定成功构建时记录的可读 hash，也不声称已独立重算这两个头。CP4 应保存构建时生成物及对应 hash，明确输入与输出。
- 未上机、未验证屏幕、网络 deadline、20 轮语音、真 NVS/掉电与触控延迟。本次未调用任何 flash、erase 或 monitor。

## 四、交给 WorkBuddy 的唯一活动整改包

**任务 ID：WB-A05-CP2-REVIEW-FIX-002，READY。** 从 `00b27e7` 及本审查文档建立实施提交，按 R1 → R2 → R3 → R4 顺序完成，每项独立提交并记录验证；全部完成后交 REVIEW_READY 停止，等待 Codex 复核。

允许修改：`tools/dev/run-a05-cp2-build.ps1`、`tools/dev/verify-build-inputs.py`、`tools/dev/verify-component-content.py`、`tools/dev/tests/` 下仅本包工具测试、本流报告。若确认必须调整 manifest schema，可修改 `integration/metalio_claw4/a05_build_input_manifest.json`，必须保留原内容 hash 并说明 schema 迁移，不能重新测量后静默覆盖漂移。

禁止：产品实现、上游源码/CMake、SDK/组件/SDKCONFIG/分区、原构建树与既有五个产物。不得为通过测试删除真实源树、重新构建、刷机、进 CP3 或重跑全部 Host。允许临时隔离 fixture 与只读验证。

验收：R1 的误传路径均在副作用前拒绝，sentinel 不丢；R2 负向输入均非零，正常冻结输入仍通过；R3 错配参数不调用 idf.py；R4 阶段定义一致。保留本次原产物与 hash，补充脚本提交 SHA 和复核日志。工具修复不自动使旧构建无效；若测试发现此次真实输入或产物来源错误，再停止并由 Codex明确是否需重新构建。
