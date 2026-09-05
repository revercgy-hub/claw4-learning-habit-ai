# CODEX-APP-FIRST-001 AF0 报告（2026-09-05）

> 结论：`AF0=CHECKPOINT_READY`。统一 Quick gate、真实 C++ LearningApp Virtual Device runner、Backend 与 PWA 组件门禁均通过；没有连接设备、没有刷写、没有修改分区或发布配置。下一步为 AF1 真实 C++ App ↔ Backend 契约链。

## 1. 代码与执行环境

| 项 | 值 |
| --- | --- |
| 工作树 | `E:\workbuddy\claw4-l1-ready` |
| 分支 | `codex/wb-learning-v4-l1-ready`（共享 Git refs ACL 阻止新建 `codex/app-first-mvp-loop`，故沿用已推送的 Codex ready 分支） |
| 基线提交 | `8574090c28b251458d3de0ed382761ebf4919f23` |
| 编译器 | `E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe` |
| 设备状态 | 未连接、未操作；本 checkpoint 不需要真机 |

## 2. 实际修改

- `tools/dev/run-app-first-gate.ps1`：新增 Quick/Full/IdfBuild 三档统一门禁，支持外部日志目录和机器可读 JSON summary。
- `tools/dev/verify-virtual-device-app.ps1`：编译并运行 Virtual Device App 场景。
- `firmware/tests/host/virtual_device_app.cpp`：薄 JSONL runner，复用真实 `LearningApp`、CommandDispatcher、AppCoordinator 和 Outbox；共享 FakeDisk 只用于模拟进程重启。
- `tools/dev/verify-host-cpp-tests.ps1`、`verify-interface-contracts.ps1`：支持外部输出目录；修正 host glue 自包含头路径被错误标为 Metalio 依赖的问题。
- `AGENTS.md`、`TASK_BOARD.md`：记录 AF0 收口与 AF1 调度。

## 3. 验证证据

### 3.1 统一 Quick gate

命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File E:\workbuddy\claw4-l1-ready\tools\dev\run-app-first-gate.ps1 `
  -Mode Quick `
  -CompilerPath E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe `
  -CrossCompilerPath E:\workbuddy\claw4-idf-tools\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-g++.exe `
  -LogDir E:\workbuddy\学习习惯培育AI\out\app-first-af0
```

结果：

```text
compiler        PASS
hostCpp         PASS
virtualDeviceApp PASS
overall         PASS
```

统一 summary 位于仓库外 `out\app-first-af0\gate-summary.json`，避免生成物进入 Git。

### 3.2 Virtual Device runner

场景为：

```text
boot → seed → touch start → touch pause → touch resume → touch complete
→ destroy/recreate LearningApp over the same FakeDisk → summary
```

结果：

- 所有四个 transition 均 `Accepted`；
- 重建进程后 `pendingAfterRestart=6`；
- 重建后 `activeAfterRestart=null`；
- 状态输出来自真实 `LearningApp`，runner 未复制 reducer 或 outbox 状态机。

### 3.3 Backend 与 PWA 基线

- ready 工作树 `backend/` 70 个测试：**70 passed**；使用外部已安装 Python 依赖运行，未写入 ready 工作树。
- ready 工作树 `backend/` 与依赖工作树逐文件 SHA-256 一致（18 个源码/配置文件）。
- PWA：typecheck PASS、lint PASS、Vitest **30/30 PASS**、production build PASS。
- ready 工作树 `frontend/` 与依赖工作树逐文件 SHA-256 一致（30 个源码/配置文件）。
- 由于当前 ready worktree 的依赖目录尚未安装且其 ACL 禁止运行时写入临时 Vite 文件，PWA 组件门禁在依赖可用的等价源码工作树执行；AF0 不把这次环境差异伪装成“ready 本地 Full gate 已运行”。后续 bootstrap 将把依赖与临时目录纳入可复现路径。

## 4. 自检与范围

| 验收项 | 结果 |
| --- | --- |
| 真实 LearningApp 纵向状态流 | PASS |
| 进程重启恢复 | PASS |
| C++ host/interface gate | PASS |
| Backend/PWA 现有组件 gate | PASS |
| 真机/串口/Flash | 未执行，非本 checkpoint 范围 |
| partition/bootloader/ota_1/C5/eFuse | 未触碰 |
| 范围偏差 | 无；仅增加 host runner、门禁编排和输出路径能力 |

## 5. 下一步 AF1

实现 target-portable 的 Auth/Today/Events/ACK JSON codec 和本地 HTTP relay，让 Virtual Device runner 产生的真实 C++ pending events 进入 FastAPI，再把 today snapshot/ACK 回灌 C++。Python 不得手写或改写业务事件；完成后才进入 AF2 离线/故障矩阵和浏览器级 PWA E2E。

真机保持断开。只有 AF0~AF4 全部通过并冻结唯一候选后，才通知用户进行阶段末一次性真机验收。
