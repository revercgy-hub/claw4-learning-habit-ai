# WB-A05-BUILD-001
## Claw4 V5.3 固件总装与唯一候选冻结任务书

日期：2026-09-14

---

# 0. 任务定位

本任务是：

`A05-BUILD`

目标不是开发新功能，而是把已经通过 Host 审查的 V5.3 模块**真正登记进 Claw4 firmware image，完成完整 ESP-IDF 构建，并冻结一个唯一候选固件**。

本任务只有在以下前置条件全部满足后才能开始：

1. `WB-V53-NEXT-001-FINAL-CONCURRENCY-CLEANUP` 已完成；
2. Codex 最终复审给出：
   `WB-V53-NEXT-001 = ACCEPTED`
3. 当前远端代码 tip 已明确记录；
4. 工作树 clean；
5. 不存在未解释的源码漂移。

如果上述任一条件未满足：

**不得开始 A05-BUILD。**

---

# 1. 已确认的产品路线

本阶段冻结以下决定：

## 1.1 A05 只做“总装 + 构建 + 稳定性基础验证”

本阶段：

- 不新增学习功能；
- 不扩展提醒规则；
- 不做家长 PWA；
- 不做复杂 AI；
- 不做 Light Sleep；
- 不做正式家庭部署；
- 不接真实儿童数据。

本阶段只回答一个问题：

> 已经在 Host 层验证通过的模块，能否被完整、可复现地装进 Claw4 固件，并形成唯一可供后续真机测试的候选。

---

## 1.2 第一次真机仍使用开发后台 / 测试任务

A05-BUILD 只做固件构建，不 Flash。

后续 A05-DEVICE 第一次真机：

- 使用现有开发后台 / 测试 backend；
- 使用测试 device_id / child_id；
- 使用测试任务；
- 不接正式家庭数据库；
- 不接真实儿童数据。

---

## 1.3 A05 通过后，下一主线是 C01 + C03

A05 完成后优先：

```text
C01
真实 TodayPlan / 每日任务后台契约
        ↓
C03-HOST/BUILD
设备时间 + Reminder + 播放/Overlay 薄适配
        ↓
C03-DEVICE
真正实现 Claw4 主动提醒
```

PWA、复杂 AI、Light Sleep 暂缓。

---

# 2. 当前 A05-BUILD 的核心目标

当前已知 Host 代码中，以下模块尚未完整进入最终 firmware image / CMake：

- `time`
- `sync_executor`
- `single_flight_http_transport`
- `interaction_arbiter`
- `reminder`

A05-BUILD 必须完成：

```text
Host 已验证模块
      ↓
登记到 firmware image / CMake
      ↓
与 MetalioClaw4 上游 patch 合并
      ↓
完整 ESP-IDF build
      ↓
检查链接与符号
      ↓
生成唯一候选固件
      ↓
记录 SHA256 / 构建证据
```

本阶段不做设备行为结论。

---

# 3. Git 与基线规则

## 3.1 基线

WorkBuddy 开始前必须读取并记录：

- 当前远端分支；
- 当前不可变 SHA；
- 上一任务最终 ACCEPTED 报告；
- A01 patch provenance；
- 当前 `project-ca3aa3fa.json`；
- 当前 `TASK_BOARD.md`；
- 当前 MetalioClaw4 upstream commit。

不得从：

- main 旧基线；
- 历史 worktree；
- 未审查 SHA；
- 本地脏目录

直接开始。

---

## 3.2 分支

建议新建独立 A05 工作分支：

`workbuddy/a05-build-m0`

如本机仍存在“含 `/` 的 Git ref 异常”，本地允许使用：

`workbuddy-a05-build-m0`

但远端必须：

`workbuddy/a05-build-m0`

并在报告中明确记录该环境偏差。

---

## 3.3 禁止

禁止：

- rebase 已审查历史
- reset 已审查提交
- squash 已审查提交
- force-push
- 推官方 Metalio origin
- 改写上游历史

---

# 4. A05-BUILD 修改范围

允许修改范围仅限构建整合与必要薄适配。

## 4.1 允许

### firmware 构建登记

- firmware / integration 的 CMake / component source list
- 必要 include path
- 必要 component dependency
- 必要编译选项修正

### MetalioClaw4 patch

仅允许：

- 将已审查模块登记进 image
- 极小的编译兼容性修正
- 现有 D5 double-free / audio yield 等已接受补丁继续保留

### 构建工具

- 可复现构建脚本
- build manifest
- artifact hash 脚本
- 候选冻结清单

---

## 4.2 原则上不得修改业务逻辑

A05-BUILD 不是再次开发：

- Coordinator
- SyncExecutor 语义
- ReminderCore 规则
- InteractionArbiter 规则
- TimeAuthority 规则
- ACK 规则
- tombstone 规则
- SessionLease 规则

如完整 IDF build 暴露编译问题：

允许做**最小编译修正**。

如暴露行为/架构问题：

必须停止并报告 Codex，不得自行扩大修复。

---

# 5. 第一阶段：总装前事实核对

开始改 CMake 前先完成：

## 5.1 确认 Host 状态

记录：

- 最终 Host suite 数量
- Local Host Gate PASS
- interface gate PASS
- cross syntax gate PASS
- CI 是否存在

A05 不允许通过删除测试换取 build。

---

## 5.2 确认待进镜像模块

逐个检查：

```text
firmware/main/time
firmware/main/sync/sync_executor.*
firmware/main/sync/single_flight_http_transport.*
firmware/main/interaction/interaction_arbiter.*
firmware/main/reminder/*
firmware/main/ports/reminder_wake_port.h
```

对每个模块记录：

- 是否纯 Host C++
- 是否已有设备调用点
- 是否需要加入 SRCS
- 是否只需 include
- 是否需要新 component dependency
- 是否已有 Host test
- 是否进入最终 image

---

## 5.3 确认设备侧现有调用点

重点读取：

```text
integration/metalio_claw4/device/app/learning_runtime.*
integration/metalio_claw4/device/ports/metalio_http_transport.*
integration/metalio_claw4/device/ports/nvs_outbox_storage.*
integration/metalio_claw4/host_glue/*
integration/metalio_claw4/patches/project-ca3aa3fa.json
```

确认：

- `LearningRuntime` 的生产路径真实引用哪些新模块；
- CMake 是否遗漏；
- link-time 是否会出现 undefined symbol；
- 是否存在 Host 已通过、image 未包含的代码。

---

# 6. 第二阶段：CMake / image 最小总装

## 6.1 必须实现

把以下模块纳入最终固件构建：

### Time

- `TimeAuthority`
- 当前已有 ClockPort 适配关系

### Sync

- `SyncExecutor`
- `SingleFlightHttpTransport`
- `LearningBackendSession` 所需完整实现

### Interaction

- `InteractionArbiter`

### Reminder

- `ReminderCore`
- 纯 Host `ReminderWakePort` 接口允许进入头文件依赖

注意：

**本阶段不要求实现真实 ReminderWakePort 设备驱动。**

如果设备当前没有实际使用它：

允许只保证：

- 接口编译
- 不产生未解析符号

真实 wake 适配留 C03 / E01。

---

## 6.2 不得做

A05-BUILD 不得顺手：

- 把 Reminder 接真实喇叭
- 接 UI Overlay
- 接 Light Sleep
- 新写 `esp_sleep`
- 新增正式定时器唤醒驱动
- 改 AI 对话逻辑
- 改语音业务规则

---

# 7. 第三阶段：完整 ESP-IDF Build

## 7.1 必须做真实完整构建

不能只做：

- Host g++
- syntax-only
- 单文件 compile

必须做：

**完整 ESP-IDF firmware build**

要求记录：

- ESP-IDF 版本
- target
- toolchain 版本
- upstream commit
- patch SHA256
- source HEAD SHA
- sdkconfig 来源
- build command
- build output directory

---

## 7.2 构建必须成功到最终镜像

至少确认产物：

- bootloader
- partition table
- application image

但是：

**禁止 Flash。**

---

## 7.3 不允许为了 build PASS 修改

禁止：

- partition layout
- bootloader policy
- OTA slot layout
- eFuse
- Secure Boot
- Flash Encryption

如镜像尺寸超出当前 partition：

立即停止并报告。

不得自行扩大 partition。

---

# 8. 第四阶段：链接与符号检查

完整 build 后必须确认：

## 8.1 新模块真的进了固件

不能只看“CMake 加了文件”。

需要用：

- map file
- nm
- objdump
- link manifest
- 或 ESP-IDF build 输出

至少证明以下符号/实现被链接：

- `TimeAuthority`
- `SyncExecutor`
- `SingleFlightHttpTransport`
- `InteractionArbiter`
- `ReminderCore`

---

## 8.2 防止“编译了但没用”

报告区分：

```text
Compiled
Linked
Referenced
Runtime-wired
```

A05-BUILD 最少要求：

- Compiled = YES
- Linked = YES

对已经存在设备调用点的模块：

- Runtime-wired = YES

对仍等待 C03 的模块：

明确：

`Linked / not yet device-wired`

不得写“真机功能已完成”。

---

# 9. 第五阶段：已有 D5 / 基础回归检查

A05 看板包含已有 D5 回归。

必须检查：

- double-free 修复补丁仍存在；
- audio yield 修复仍存在；
- patch hash 与 upstream 对得上；
- A01 provenance 没被意外覆盖。

如果 patch manifest 改变：

必须重新生成：

- patch SHA256
- 文件 SHA256
- 变更说明

不得只改 `main/CMakeLists.txt` 而不更新 provenance。

---

# 10. 第六阶段：候选冻结

完整 build 成功后冻结：

`M0 Candidate`

建议命名：

`claw4-v53-m0-a05-<shortsha>`

必须记录：

## 10.1 Source

- Git commit SHA
- branch
- upstream Metalio commit
- patch manifest SHA256

## 10.2 Toolchain

- ESP-IDF version
- riscv32 toolchain version
- build host

## 10.3 Artifacts

至少：

- app `.bin`
- bootloader `.bin`
- partition-table `.bin`
- build manifest
- map file

对每个关键 artifact 计算 SHA256。

---

## 10.4 只能有一个“正式候选”

可以有失败 build 目录，但最终必须明确：

```text
唯一 A05-DEVICE 候选：
<commit>
<artifact hashes>
```

后续所有真机测试必须使用这个候选。

不得：

- 一项测试刷 candidate A
- 下一项测试刷 candidate B
- 再汇总成同一次 Device PASS

---

# 11. Host Gate 回归

CMake/image 总装完成后，再运行完整 Host Gate。

要求：

- 所有旧 suite 保留；
- 所有新增 suite 保留；
- 全 PASS；
- interface PASS；
- cross syntax PASS。

如果完整 IDF build 为修编译做了源码调整：

必须再跑 Host Gate。

---

# 12. A05-BUILD 验收标准

必须同时满足：

| 项目 | 要求 |
| --- | --- |
| 最终并发 cleanup | Codex ACCEPTED |
| Host Gate | PASS |
| 完整 ESP-IDF build | PASS |
| image CMake | 新模块已登记 |
| link evidence | 有 |
| D5 patch | 保留 |
| A01 provenance | 更新/有效 |
| artifact hashes | 完整 |
| 唯一候选 | 已冻结 |
| Flash | 未执行 |
| 真机 | 未执行 |

完成后状态：

`A05-BUILD = REVIEW_READY`

不是：

`A05 = PASS`

---

# 13. A05-BUILD 最终报告

新增：

`docs/project_management/reports/WB_A05_BUILD_001_REPORT.md`

至少包括：

## 13.1 基线

- source SHA
- accepted parent SHA
- branch
- upstream SHA

## 13.2 CMake / image 变更

逐个模块说明：

```text
time
sync_executor
single_flight_http_transport
interaction_arbiter
reminder
```

分别写：

- 加入位置
- 编译状态
- 链接状态
- 设备 wiring 状态

## 13.3 完整 build

完整命令与结果。

## 13.4 Artifact Manifest

文件名 + size + SHA256。

## 13.5 Patch Provenance

更新后的 patch manifest / hash。

## 13.6 未完成边界

必须继续写：

`HARDWARE_VERIFY_REQUIRED`

包括：

- 触控 P95
- 网络分段耗时
- 20 轮语音
- 真 NVS
- 真掉电
- Reminder 真实播放
- ReminderWakePort 设备实现
- Light Sleep
- TCP loopback（如仍缺）

---

# 14. 完成后的停止条件

A05-BUILD 完成后：

1. push；
2. 工作树 clean；
3. 报告完整；
4. 唯一 M0 candidate 冻结；
5. 状态：

`REVIEW_READY`

然后停止。

禁止：

- Flash
- A05-DEVICE
- C01
- C03
- PWA
- AI 扩展
- Light Sleep

等待 Codex 审查。

---

# 15. A05-DEVICE 预定义入口（本包禁止执行）

A05-BUILD 被 Codex ACCEPTED 后，才生成并执行：

`A05-DEVICE`

后续第一次上机只使用：

**A05-BUILD 冻结的唯一候选固件。**

第一次真机只测：

```text
启动
 ↓
学习页面
 ↓
触控
 ↓
Wi-Fi
 ↓
开发 backend
 ↓
测试 Today 任务
 ↓
任务状态操作
 ↓
NVS 重启恢复
 ↓
网络挂起时 UI 是否仍流畅
 ↓
连续语音稳定性
```

不在第一次 A05-DEVICE 中新增：

- 主动 Reminder 硬件播放
- Light Sleep
- 正式家庭 backend

这些留 C01 / C03。

---

# 16. A05 通过后的开发顺序

A05-BUILD + A05-DEVICE 均通过后：

```text
C01
TodayPlan / 每日任务真实契约
       ↓
C03-HOST/BUILD
Reminder 真正接设备
       ↓
C03-DEVICE
主动提醒真机闭环
       ↓
C04
Focus / DailyReview / 习惯反馈
       ↓
D01 / D02
AI 对话与任务提醒深度融合
```

PWA 与 Light Sleep 不抢在主动提醒核心闭环之前。

---

# 17. WorkBuddy 完成口令

完成 A05-BUILD 后最终只汇报：

```text
A05-BUILD REVIEW_READY

Source SHA:
Upstream SHA:
Patch SHA256:
ESP-IDF:
Host Gate:
Full IDF Build:
Candidate:
App SHA256:
Bootloader SHA256:
Partition SHA256:
Hardware:
NOT RUN
```

然后停止，等待 Codex 审查。
