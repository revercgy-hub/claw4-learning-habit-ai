# CODEX-APP-FIRST-001 AF1a 报告：target-portable wire codec

- 任务：`CODEX-APP-FIRST-001 / AF1a`
- 执行：Codex（WorkBuddy 按用户要求暂停）
- 分支：`codex/app-first-mvp-loop`
- 实现提交：`ea3afb2`（已普通 push 到 `origin/codex/app-first-mvp-loop`）
- 范围：仅主机/平台无关协议边界；未连接真机、未刷写 Flash、未修改 BSP/分区/启动链路。

## 1. 实际修改

- `firmware/main/sync/wire_codec.h/.cpp`
  - 新增无 IDF/HTTP/TLS 依赖的 C++17 JSON codec。
  - 覆盖 `/devices/challenge`、`/devices/auth`、Today tasks、`/events/batch` request/response。
  - 保留事件 payload、timestamp source、sequence、ACK consecutive prefix 和五类事件结果（accepted/duplicate/conflict/rejected/gap）。
  - 解析失败、字段缺失、未知状态均返回 `false`，不产生部分成功结果。
- `firmware/tests/unit/sync/wire_codec_tests.cpp`
  - 覆盖中文、引号、反斜杠、换行等 JSON 转义；Auth/Challenge/Today/Batch 正反例和 outcome 映射。

## 2. 验证证据

命令（输出放在仓库外，避免构建物进入 Git）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/dev/verify-host-cpp-tests.ps1 `
  -CompilerPath E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe `
  -CrossCompilerPath E:\workbuddy\claw4-idf-tools\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-g++.exe `
  -OutputDir E:\workbuddy\学习习惯培育AI\out\app-first-af1-host
```

结果：

- native C++ smoke：PASS；host unit summary `12 / 12 PASS`。
- 新增 `wire_codec_tests`：`all PASS`。
- interface cross-check：exit `0`；headers `34/34`、interaction/mcp implementation `3/3`、contract `1/1`。
- `learning_domain`、UI、interaction/mcp/host_glue 禁止依赖扫描：PASS。

## 3. 自检与边界

- [x] JSON 编解码不依赖 IDF、FreeRTOS、网络库或 Python。
- [x] Auth challenge/auth response、Today、Events/ACK 的字段名与 Backend schemas 对齐。
- [x] 事件序列与 ACK 只在 codec 中搬运，未绕过 outbox 的 commit-then-publish 规则。
- [x] 非法 JSON、缺失字段、未知 outcome/status 有负例测试。
- [x] 未修改 `vendor/MetalioClaw4`、BSP、driver、sdkconfig、partition、bootloader、ota_1、C5/eFuse。
- [ ] 尚未完成真实 HTTP relay、真实 Backend E2E；因此 AF1a `CHECKPOINT_READY`，AF1 总体仍 `IN_PROGRESS`。

## 4. 下一 checkpoint（AF1b）

实现一个只负责传输的本地 HTTP relay/adapter：接收 codec 生成的 UTF-8 JSON，调用现有 Backend `/devices/challenge` → `/devices/auth` → `/children/{child}/tasks/today` → `/events/batch`，原样返回 JSON；同时由真实 C++ runner 驱动，不允许 Python 合成或改写事件。完成后再进入 AF1c 的真实 C++ App↔Backend↔PWA 单链 E2E。

真机状态：仍不需要连接。只有 AF0~AF4 全部通过并冻结唯一候选后，才通知用户接入设备做一次阶段末验收。
