# CODEX-APP-FIRST-001：learning NVS 演示任务重置复检

任务：`CODEX-APP-FIRST-001 / L1 demo reset`；执行：Codex；分支：`codex/app-first-mvp-loop`。

## 结论

`ACCEPTED`（限定范围）：用户授权的 learning 命名空间清空、演示任务重新生成，以及学习页本地 Start/Pause/Resume/Complete 交互已在 COM7 目标设备上完成复测。设备当前回到可继续操作的演示任务状态。

这不是 L2/L3 真机网络、设备注册、TLS、语音、MCP 或发布固件验收；这些范围仍保持原看板门禁。

## 授权与实现边界

- 用户明确授权清空 learning NVS 并重新生成演示任务。
- 实现通过屏幕上的“重新生成演示任务”按钮调用 `LearningRuntime::ResetToSeed()`。
- `ResetToSeed()` 仅调用 `nvs_erase_all()` 于 `learning` 命名空间，随后写入 `DemoTodaySnapshot()`；未擦除整片 Flash，未改 partition、bootloader、ota_1、C5 或 eFuse。
- 复检中发现并修复一个边界：重置会清掉一次性自测标记；现由 `ResetToSeed()` 在成功 re-seed 后重新写入 `stest=1`，避免再次进入页面时自动重跑调试自测覆盖用户进度。

## 真机证据

目标串口为 COM7（Espressif USB serial，COM3 不存在）。本次仅执行 ota_0 application-only app-flash，写入偏移 `0x200000`；没有执行 `erase_flash` 或其它分区写入。

刷写候选（包含屏幕重置按钮）：

| 项目 | 值 |
| --- | --- |
| BIN | `E:/workbuddy/claw4-idf-cold-c5-20260906/xiaozhi.bin` |
| BIN 大小 | 9,176,448 B |
| BIN SHA-256 | `0d9dc14abcf079b71105fdffb7b9abb736681ed54b5897a01e6698b13c51f9cb` |
| ELF SHA-256 | `e43d0f127372cb94044463ab1eb1c3d768b6d4e3b4e800445e2b6b45afa2705a` |

监视器关键行：

```text
LearningNvs: save blob=344 bytes set=0 commit=0
LearningRt: self-test cleanup: re-seeded=1
LearningScreen: demo reset requested from completed state -> ok=1
LearningNvs: load peek=0 len=344
LearningNvs: save blob=... bytes set=0 commit=0
LearningScreen: touch kind=2/3/4 ... -> status=0 intent=0
```

随后连续看到 learning blob 长度增长（687、764、841、918、1066、1344、1425、1506、1589、1743），且每次 `set=0 commit=0`；用户确认屏幕显示和交互均正常。`kind` 数值按 `CommandKind` 映射为 Start=1、Pause=2、Resume=3、Complete=4。

## 构建与源码校验

- 外部 C5 镜像 allowlist 同步：64 文件，`mismatch_before=1`（本次 runtime 修复），`mismatch_after=0`。
- 同一构建目录重新 BUILD ONLY：exit 0；仅保留 ota_1 容量不足告警，未改分区。
- 修复后的 BUILD ONLY 产物（未再次刷入设备）：BIN 9,178,752 B，SHA-256 `cfa68f40be48e282f196f2ad893acf4a13030595c0fdc78cf481694e5d0606b9`；ELF SHA-256 `da88e05464538e464c2a238c6757a2fb06e3a5ec9938b9823dda1bdd134972d9`。
- `git diff --check` 通过。

## 验收限制与后续

- 本报告只验收 learning 本地 namespace reset/reseed 与屏侧本地状态链路。
- Wi-Fi/TLS/Backend、真实用户/设备注册、语音、MCP、AI Coach 和长期稳定性仍需单独阶段门禁。
- 后续若再次需要演示任务，直接使用页面按钮即可；不需要整片 Flash 擦除。
