# WB_A05_CP4_CANDIDATE_FREEZE_001 — 唯一 M0 Candidate 冻结报告

**任务**：WB-A05-CP4-001（CP4 / Candidate Freeze / A05-BUILD Closeout）
**分支**：`workbuddy/a05-build-m0`
**日期**：2026-09-16
**原则**：**Freeze, don't develop.**

---

## 1. Candidate 身份

```text
claw4-v53-m0-a05-2c8f58f
```

| 项 | 值 |
| --- | --- |
| 类型 | **APPLICATION_ONLY**（`ota_0` payload） |
| Manifest | `integration/metalio_claw4/candidates/claw4-v53-m0-a05-2c8f58f.json` |
| 候选身份构成 | 源码 revision + 输入清单身份 + 5 构件 size/SHA256 + 分区/容量事实，四者缺一不可 |

⚠️ **易混淆点（已在 manifest 内显式写明）**：`e590626` 是**审计报告的 revision**，**不是**固件产品源码 revision。固件源码 revision 是 `2c8f58f`。

---

## 2. Authoritative source SHA

```text
2c8f58f53506402918284c695100f007798433d8
```

CP2 成功构建所依据的、含 §21.1 经准许源码修订（`learning_screen.cc` 删除漏删的 stale cleanup block）的权威产品源码。

---

## 3. CP3 audit SHA

```text
e590626d289e9b67a41200493b36733cd1a07d8e
```

CP3 链接/来源/模块矩阵审计（报告 §26）。**本轮工作树 HEAD 即此值，且与远端 `workbuddy/a05-build-m0` 一致、工作树 clean。**

---

## 4. Build input identities

| 输入 | SHA256 | 说明 |
| --- | --- | --- |
| **isolated source tree** | `36bc3d4d9b847253b90068fd8776d28e80cfd501765723471205b6f60f7e8ba9` | **1353 文件 / 72,457,109 B**；根 `E:\a05c\s` |
| **SDKCONFIG** | `a901f20491671a9cbca8cbe60c4ac4b26d91ac1916d36530b9475b6704fabc26` | 冻结 C5 配置；树内实测一致；configure 前后相等 |
| **ESP-IDF 5.5.4 tree** | `9979c6c36539dd7c7021a881c7602a333801c49586b55cc651914ed84d3d5381` | `E:\workbuddy\esp-idf-5.5.4-ascii` |
| **IDF tools tree** | `78ad1256ef07b11b92e903920c3f5f1a3d49d60887ad2b874f110b51cf1c2f44` | cmake 3.30.2 / ninja 1.12.1 / riscv32-esp-elf esp-14.2.0_20260121 |
| **managed_components** | `83c759c8306b11193feebd061cd8752bfe7db4384e752132a078f312395a9af4` | 82 组件 / 14,656 文件 |

`isolated_src` 摘要对**相对路径**计算，排除 `managed_components`、`build`，以及 3 个**每次构建都会重写**的生成路径（`main/assets/lang_config.h`、`main/i18n/i18n_strings_gen.h`、`main/mmap_generate_resources.h`）——这三个路径只记录、不比较，其余文件全部在摘要内。

**构建根 `E:\a05c\b`**：短路径是**强制要求**（长 TU 带 379 个 `-I`、其中 212 个内嵌源码根路径，而 Windows `CreateProcess` 命令行上限 32,767 字符；见 WB_A05_BUILD_001_REPORT.md §23/§24）。

---

## 5. 五个 artifact：size + SHA256

**CP4 未复制报告值** —— 对现有 CP2 构件**重新做了只读 size + SHA256 校验**，逐项 `MATCH`：

| 构件 | size | SHA256 | 角色 |
| --- | --- | --- | --- |
| `xiaozhi.bin` | **9,271,760 B** | `c035e1c09ebe472f5f14b490c93844aa3e298cd11b4e30f7fe1055e5b9278d36` | **APPLICATION_IMAGE**（唯一拟刷写物，`ota_0` @ `0x00200000`） |
| `xiaozhi.elf` | 80,102,764 B | `fba68d58002e8139506df655310cabd79f8359f1fff11cfc49d566ea5da8aa60` | EVIDENCE_ONLY_NOT_FOR_FLASH |
| `xiaozhi.map` | 21,859,377 B | `93f8e7d93e35cac0d2d52b073111b72b8069ac7afe431754db84717546b5bafe` | EVIDENCE_ONLY_NOT_FOR_FLASH |
| `bootloader/bootloader.bin` | 20,416 B | `e2a455c5786943bc0c23a2b1cb71482b35763319a9b457d80c7e63fdc3b33f33` | EVIDENCE_ONLY_NOT_FOR_FLASH |
| `partition_table/partition-table.bin` | 3,072 B | `ef0039b6366c57de098972c0f6e9fd991013b41968da9b866c4704cb68ef7e5f` | EVIDENCE_ONLY_NOT_FOR_FLASH |

```text
RESULT: 5 / 5 MATCH
```

`partition-table.bin` 的 SHA256 与历次由**批准 CSV**往返生成的 bin **逐字节相同** ⇒ 分区布局自始至终由批准输入决定，与源码树/构建根无关。

---

## 6. 分区 13/13 PASS 摘要

批准 CSV `partitions/v1/32m_dual.csv`（LF SHA `c5277b4bf6348c676cdb1e02fcd4275d6b7a3630675f0ad1291f85fa8b01116b`）↔ 真实生成 `partition-table.bin`（`ef0039b6…`）：

```text
nvsfactory 0x0000a000/204800 | nvs 0x0003c000/860160 | otadata 0x0010e000/8192
phy_init 0x00110000/4096 | model 0x00111000/978944
ota_0 0x00200000/9437184 | ota_1 0x00b00000/4194304
resources 0x00f00000/4194304 | factory_test 0x01300000/614400
emote 0x01396000/4194304 | system 0x01796000/1048576
storage 0x01896000/7340032 | coredump 0x01f96000/65536
last partition end 0x01fa6000 ; trailing unallocated 368,640 B
RESULT: PASS -- matches the approved input, no overlap, in range
```

```text
NO PARTITION CHANGE IN CP4
NO OTA POLICY CHANGE IN CP4
```

---

## 7. `ota_0` headroom

```text
ota_0 slot      : 9,437,184 B   (offset 0x00200000)
app image       : 9,271,760 B
ota_0 headroom  :   165,424 B   (1.75%)
```

⚠️ 余量仅 **1.75%**。后续**任何影响 binary 的构建**都必须重跑 **app size vs `ota_0` HARD GATE**。

---

## 8. `ota_1` 不可用

```text
ota_1 slot      : 4,194,304 B   (offset 0x00b00000)
app fits ota_1  : FALSE
```

```text
DUAL-SLOT OTA UNAVAILABLE FOR THIS CANDIDATE
```

⇒ 本候选只提交 **application-only** 证据；**不改分区、不改 OTA 策略**。双槽 OTA 的流程/选择/回滚不属于本包。

---

## 9. 模块状态（继承 CP3，不扩大审计范围）

**判定口径（必须保留）**：`libmain.a` 以 **`--whole-archive`** 链接 ⇒ "归档里有符号"对所有成员恒真、**不具区分力**；保留与否**只看** `map 的 Discarded input sections` + **最终 ELF 符号表**。**不得把 "Archive contains symbol" 写成 "ELF retained"。**

### Group 1 —— 已接入最终运行链

| 模块 | 状态 | ELF |
| --- | --- | --- |
| `SyncExecutor` | COMPILED / REFERENCED / **ELF_RETAINED** / RUNTIME_WIRED | 3 T |
| `SingleFlightHttpTransport` | COMPILED / REFERENCED / **ELF_RETAINED** / RUNTIME_WIRED | 3 T + 3 V + 3 W |
| `LearningBackendSession` | COMPILED / REFERENCED / **ELF_RETAINED** / RUNTIME_WIRED | 8 T + 2 d + 2 t |

```text
LearningScreen → LearningRuntime::Instance() → LearningBackendSession
              → SyncExecutor → CreateMetalioHttpTransport() → SingleFlightHttpTransport
```

### Group 2 —— 编译但未接线（**这是 M0 的合法状态，CP4 不得改变**）

| 模块 | 状态 | ELF |
| --- | --- | --- |
| `TimeAuthority` | COMPILED / **GC_DISCARDED** / **COMPILED_NOT_WIRED** | 0 |
| `InteractionArbiter` | COMPILED / **GC_DISCARDED** / **COMPILED_NOT_WIRED** | 0 |
| `ReminderCore` | COMPILED / **GC_DISCARDED** / **COMPILED_NOT_WIRED** | 0 |

**CP4 未添加任何引用**。接线会改变 binary ⇒ 当前候选失效 ⇒ 必须重走完整 Build Gate 并重新 CP3 审计。**接线属于 C03，且不是 A05-DEVICE 的验收前提。**

### Group 3 —— 类型编译成立

| 模块 | 状态 |
| --- | --- |
| `ReminderWakePort` | 类型编译（`reminder_core.h` 包含）；纯接口**不要求**独立 ELF 符号；设备实现留 C03/E01 |
| `SessionLease / Holder / Source` | 类型编译；**ELF_RETAINED**（`SessionLeaseHolder<LearningBackendSession>` 的 vtable/`~Holder`/`currentLease`、`SessionLeaseSource::isCurrent` = `T`） |
| `SessionSnapshotPublisher` | 类型编译 + **运行时实际调用**（`learning_runtime.cpp:120`）；inline 实现**无独立符号**，可接受 |

---

## 10. D5：audio yield 正式口径 + `SUPERSEDED 10 ms`

### 10.1 `learning_screen`

```text
tick deadline  = signed-difference / wrap-safe
timer timeout  = reuse existing timer
lv_timer_del   = 2 处，均 nullptr guard + immediately nullptr
无 s_selftest_timer / 无 SELFTEST / 无 ResetToSeed
```

源文件 LF SHA256 `a41aeed7b10dfab5aece840e0c582c8d9d3eb2e71805d78e7caafdabccbad809` —— **等于权威源码 revision 的仓库 blob**（逐字节），而 A01 vendor 补丁对该文件的唯一提及只是把它加入 CMake 源列表 ⇒ **代码哈希独立于 vendor patch**。

### 10.2 audio yield 正式口径

```text
CONFIG_FREERTOS_HZ = 1000
vTaskDelay(pdMS_TO_TICKS(1)) = 1 tick = 1 ms
```

锁作用域：A01 `project-ca3aa3fa.patch`（SHA256 `58bfbe5266a9fa00980be5e5078b570b9915165e199981e6dd54dcb21aa81937`）hunk `@@ -239,10 +239,17 @@` 把 `lock_guard` 收进**内层作用域**、`vTaskDelay` **外置在锁外** ⇒ 让出 CPU 时**不持有** `wake_word_mutex_`。树内文件 LF SHA `f406e12a…` == A01 `patched_lf_sha256`。

取值方式：用 `compile_commands.json` 中该 TU 的**真实编译命令**（515 argv / 保留 513 个含全部 `-I`/`-D`）对 `static_assert` 做 `-fsyntax-only` 编译，**不是口算**。

### 10.3 `10 ms` 描述 → **SUPERSEDED**

```text
历史材料中的 "10 ms" 描述 = SUPERSEDED，不得再引用。
```

理由：`CONFIG_FREERTOS_HZ=100` 时 `pdMS_TO_TICKS(1) = (1×100)/1000 = 0` tick ⇒ **0 ms（完全不让步）**，不是 10 ms。把"1 tick = 10 ms"读成"`pdMS_TO_TICKS(1)` = 10 ms"是整数截断造成的误读。

```text
1 ms 静态正确  ≠  真机 20 轮稳定性已验证
```

真机稳定性属于 A05-DEVICE / 后续设备任务。**CP4 未改 yield 实现。**

---

## 11. Host Gate

```text
29 / 29 PASS
```

`unit summary 29/29 PASS`｜`interface (P4 cross compile) exit=0`｜`RESULT: NATIVE CPP TEST GATE PASS`｜`RUN FAIL 0 / LAUNCH FAIL 0 / failures=[1-9] 0`。

CP4 **未修改产品源码**，故按任务书 §11 **未重新执行** Host Gate；此处引用的是 CP3 阶段针对同一源码 revision 的准确证据（`2c8f58f`）。

---

## 12. 尚未验证的真机项目

**统一标记 `DEVICE_VERIFY_REQUIRED` / `FUTURE_TASK_REQUIRED`，CP4 不把它们写成 PASS：**

| 项 | 标记 |
| --- | --- |
| TCP real loopback | `DEVICE_VERIFY_REQUIRED`（`ENV_VERIFY_REQUIRED`，不能凭 socket 桩宣称通过） |
| 底层网络 total deadline | `DEVICE_VERIFY_REQUIRED`（`EspTcp::Connect()` 不继承 HTTP timeout、`Disconnect()` 可能等 10 s） |
| device UI latency | `DEVICE_VERIFY_REQUIRED` |
| 本候选真 NVS / 掉电回归 | `DEVICE_VERIFY_REQUIRED`（历史 `DEVICE_L1C_PERSISTENCE=PASS` 属另一批次） |
| 20 轮 audio/device stability | `DEVICE_VERIFY_REQUIRED` |
| ReminderCore 真机运行 | `FUTURE_TASK_REQUIRED`（C03） |
| 主动提醒 | `FUTURE_TASK_REQUIRED`（C03） |
| ReminderWakePort device implementation | `FUTURE_TASK_REQUIRED`（C03/E01） |

---

## 13. NO FLASH 声明

```text
flash            : NOT PERFORMED
device           : NOT CONNECTED
a05_device       : NOT STARTED
```

CP4 未执行 `flash` / `app-flash` / `erase-flash` / `monitor`，**未连接串口、未做设备启动验证、未跑真机行为测试**，**未执行 `flasher_args`**。

---

## 14. A05-DEVICE 必须使用 exact Candidate

```text
A05-DEVICE MUST FLASH THIS EXACT CANDIDATE:

claw4-v53-m0-a05-2c8f58f

xiaozhi.bin:
c035e1c09ebe472f5f14b490c93844aa3e298cd11b4e30f7fe1055e5b9278d36

target: esp32p4 / ota_0 offset 0x00200000 / application-only
```

**A05-DEVICE 开始时必须先重新 hash 待刷 `xiaozhi.bin`，只有 hash 完全相同才允许 Flash。**

禁止：**重新 build 后再刷**。若因任何原因需要重新 build ⇒ **当前 CP4 Candidate 立即失效**，回到 Build Gate。

---

## 15. CP4 自检记录

| 检查 | 结果 |
| --- | --- |
| `git status` | 空（clean） |
| `git diff` | 空 |
| `git rev-parse HEAD` | `e590626d289e9b67a41200493b36733cd1a07d8e`（== CP3 audit revision，== 远端） |
| 产品源码/CMake/sdkconfig/partition 改动 | **无** |
| 5 artifacts 重算 size + SHA256 | **5 / 5 MATCH** |
| Candidate manifest JSON 合法性 | 合法 |
| manifest artifact hashes == CP2 frozen hashes | **一致** |
| manifest 内 SHA256 是否全为完整 64 位 | **是（0 个短 hash）** |
| Host Gate | `29/29 PASS` |
| 是否发生范围偏差 | **无** |

**本轮变更仅三项**：candidate manifest、本冻结报告、`WB_A05_BUILD_001_REPORT.md` 状态更新。

---

## 16. 本轮不做的事（明确边界）

- ❌ 不为让 Group 2 进入 ELF 而添加任何引用
- ❌ 不重新构建（无 `idf.py build` / `ninja` / `cmake configure` / `set-target` / `menuconfig`）
- ❌ 不 Flash、不接设备、不做真机验证
- ❌ 不改分区表、不改 OTA 策略、不改 yield 实现
- ❌ 不自行启动 A05-DEVICE

---

## 17. 收口结论

```text
CP4 = REVIEW_READY
        ↓
A05-BUILD = READY_TO_CLOSE
        ↓
下一任务：A05-DEVICE（必须使用 exact candidate claw4-v53-m0-a05-2c8f58f）
```

后续路线（**仅记录，不执行**）：

```text
CP4 → A05-BUILD ACCEPTED → A05-DEVICE
  ├─ exact candidate hash check  ├─ Flash  ├─ Boot  ├─ Monitor
  ├─ UI smoke  ├─ Network smoke  ├─ Voice smoke  └─ persistence/regression smoke
→ C01 → C03-HOST/BUILD → C03-DEVICE → C04
```

C03 开始后须特别关注 `ReminderCore` / `TimeAuthority` / `InteractionArbiter` 从 `GC_DISCARDED → RETAINED` 后的 **app size**（当前 `ota_0` 仅剩 165,424 B / 1.75%）。
