# CODEX-APP-FIRST-002 P1/P2 Provisioning 与 HMAC signer 报告

> 日期：2026-09-06  
> 分支：`codex/app-first-mvp-loop`  
> 状态：`CHECKPOINT_READY`（C5 BUILD ONLY；尚未刷入本候选）

## 1. 实施内容

### P1 独立 provisioning 配置

- 新增平台无关 `BackendProvisioning` 契约和 `ProvisionedBackendConfig`。
- 新增 Metalio `NvsBackendProvisioning`，使用独立 NVS namespace `learning_cfg` 保存 endpoint、device ID、child ID 和 device secret。
- `ResetToSeed()` 只清理 learning 状态，不清理 `learning_cfg`，避免演示任务重置破坏配对凭据。
- 配置字段长度受限，写入要求四字段完整；读取/写入失败不会伪造 Ready。

### P2 target signer

- 新增 `CreateMetalioHmacSigner`，使用 ESP-IDF mbedTLS HMAC-SHA256。
- 签名原文固定为 `device_id|challenge_id|nonce`，输出标准 Base64，与 Backend security 契约一致。
- `LearningRuntime::ConfigureProvisionedBackend()` 从 provisioning 读取 endpoint/身份并注入 signer；当前仍由后续 worker checkpoint 调用。
- secret、nonce、signature、token 不进入日志或屏侧诊断。

## 2. 验证

### C5 BUILD ONLY

命令：`idf.py -C E:\c -B E:\workbuddy\claw4-idf-cold-c5-20260906 build`

结果：exit 0；新增 `nvs_backend_provisioning.cpp`、`metalio_hmac_signer.cpp` 编译并链接；`xiaozhi.bin` 生成。

| 产物 | 大小 | SHA-256 |
|---|---:|---|
| `xiaozhi.bin` | 9,179,872 B | `C57DF8444F8EDC0438E5A4352CD1C0485E0A4F9B0EC23949F9E10B8B87872251` |
| `xiaozhi.elf` | — | `44C97CFA3E2156F8DC85F46E6BC6CFC9727FF822A8998D2705AC45F9D3791CAC` |

已知告警仍为既有 `ota_1` 分区不足；未修改分区、bootloader、sdkconfig、C5 或 eFuse。

### Host/target 证据

- 新增 `backend_provisioning_tests.cpp` 覆盖未配置、保存/读回、无效保存、清除、保存失败保留旧配置。
- 当前环境没有可用 host `clang++/g++`，完整 native host gate 无法重跑；使用 ESP-IDF RISC-V g++ 对该测试执行 `-std=c++17 -Wall -Wextra -Werror` 目标语法编译，exit 0。
- 前一 checkpoint 已通过的 host Quick 17/17 仍作为基线，不把本次新增测试误报为已执行 host runtime。

## 3. 尚未完成

1. P3 worker/调度接线：当前 `ConfigureProvisionedBackend()` 尚未在设备后台任务调用，故刷入后仍会显示 `backend 未配置`，这是预期。
2. COM7 provisioning 工具和物理确认流程尚未实现。
3. 真实 Wi-Fi、Backend endpoint、设备注册 secret 和 HMAC 联调尚未执行。

## 4. 下一步

先完成 P3 worker 与 P4 诊断接线，再实现 USB provisioning 工具；收到局域网 Backend/Wi-Fi 输入后，冻结唯一候选，进行一次 ota_0 application-only 真机联调。

