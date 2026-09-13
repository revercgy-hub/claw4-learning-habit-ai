# V53 B01 TimeAuthority Host 报告

## 1. 任务与范围

- 任务：`CODEX-V53 / B01`
- 分支：`codex/v53-b01-time-authority`
- 基线：`1ae2a0c546615666a57abdf601b343bc88a043c4`
- 范围：纯 C++17 Host 时间权威模块与专属确定性测试。
- 未修改 `ClockPort`，未引入 IDF、SNTP、真实 NVS、网络或设备适配。

## 2. 修改文件

- `firmware/main/time/time_authority.h/.cpp`
- `firmware/tests/unit/time/time_authority_tests.cpp`

## 3. 实现摘要

`TimeAuthority` 组合现有 `ClockPort`，仅读取 `monotonicMs()`；不读取现有
`epochSeconds()` 或 `isTimeSynced()`，避免把 uptime/占位值当作 UTC。每个对象持有
调用方提供的 `boot_id`，新对象从 `Unsynced` 开始，不跨 boot 恢复旧单调值。

`acceptSync(epoch_ms, source, uncertainty_ms)` 显式接收可信校时，并返回
`SyncResult`（接受/拒绝、InitialAnchor/ForwardJump/BackwardJump/None、调整量和来源），
供上层执行 reconcile。负 epoch、负不确定度、负或回退的单调值会被拒绝；单调回退后
epoch 明确失效，避免计算跨回绕或倒退的年龄。

`TimeStatus` 分离 epoch 有效性、epoch/年龄 optional、最近同步 epoch 与本 boot 单调锚点、
质量（`Unsynced`、`Synced`、`Holdover`、`Stale`）、不确定度和 `calendar_allowed`。
默认策略阈值为 stale 1h、最大 holdover 24h、日历误差预算 60s，均可配置，属于待硬件
漂移实测确认的策略值；不对停电期间的未知时间作补算。

## 4. 验证

命令（PowerShell）：

```powershell
$env:PATH = 'E:\workbuddy\toolchains\w64devkit-2.9.1\bin;' + $env:PATH
& 'E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe' -std=c++17 -Wall -Wextra -Werror -Ifirmware/main firmware/main/time/time_authority.cpp firmware/tests/unit/time/time_authority_tests.cpp -o time_authority_tests.exe
& .\time_authority_tests.exe
```

结果：`TimeAuthority: 8 cases, 0 failures`。

逐项覆盖：Unsynced 无 epoch、同步及年龄、Holdover 到 Stale、误差预算独立判定、
校时前跳/回拨 DTO、单调回退拒绝、重启 boot 隔离、无效输入和 int64 饱和边界。

## 5. 风险与边界

- epoch 单位是毫秒；调用方须传入 Unix epoch milliseconds。
- 时间质量和日历许可是策略判断，不代表任何真实 RTC/SNTP 或硬件漂移已验收。
- `ReconcileKind` 只返回调整意图，不触发提醒、任务或领域状态变化。
- `HARDWARE_VERIFY_REQUIRED`：设备 monotonic 来源、真实同步来源、漂移分布、阈值与
  时区日历行为需在后续设备/Backend 工作包验证。

## 6. 范围偏差与复检重点

无范围偏差。建议复检：epoch optional/valid 的失效语义、Holdover 日历误差预算、
monotonic 回退后的恢复方式，以及后续 Reminder reconcile 是否保持“倒计时只用单调时钟”。
