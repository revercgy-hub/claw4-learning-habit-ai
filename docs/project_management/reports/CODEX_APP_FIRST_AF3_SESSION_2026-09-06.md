# CODEX-APP-FIRST-001 AF3-5 设备 Backend 会话层报告

> 日期：2026-09-06  
> 负责人：Codex  运行分支：`codex/app-first-mvp-loop`  
> 状态：`CHECKPOINT_READY`（主机与 C5 BUILD ONLY 通过；真实设备联网验收尚未声明）

## 1. 授权与范围

用户已授权实现设备侧 BackendClient 认证、今日任务拉取、事件同步和诊断接口，并允许后续进行刷机。本 checkpoint 只完成安全的代码集成、主机测试和 C5 `BUILD ONLY`；没有写入真实 endpoint、签名密钥或儿童数据，也没有执行刷机、擦除 Flash、修改分区、bootloader、`ota_1`、C5 或 eFuse。

## 2. 实施内容

新增 `firmware/main/sync/learning_backend_session.{h,cpp}`，把已经通过 AF1 的 target-portable `BackendClient` 与真实 `AppCoordinator` 串成以下顺序：

1. 注入 endpoint、设备/儿童 ID 和 challenge signer；
2. challenge/authentication；
3. 使用权威 `fetchToday` 快照调用 `applyTodaySnapshot`；
4. 通过 coordinator 的 transactional outbox 批量发送事件并处理 ACK；
5. 输出脱敏诊断：configured/auth/network、auth pause、pending、last ACK、最近错误、HTTP 状态、最后操作和同步时间。

`LearningRuntime` 暴露 `ConfigureBackend`、`RunOnlineCycle`、`BackendDiagnostics`。凭据由上层 signer 注入，不在固件硬编码或日志输出；网络周期必须由 worker/task 调度，不能从 LVGL 回调直接调用。`ResetToSeed()` 会先释放旧会话，避免会话持有已重建 coordinator 的悬空引用。Learning 页面仅显示实际会话诊断，未配置时明确显示 `backend 未配置`。

## 3. 修改文件

- `firmware/main/sync/learning_backend_session.h`
- `firmware/main/sync/learning_backend_session.cpp`
- `firmware/tests/unit/sync/learning_backend_session_tests.cpp`
- `integration/metalio_claw4/device/app/learning_runtime.h`
- `integration/metalio_claw4/device/app/learning_runtime.cpp`
- `integration/metalio_claw4/device/learning_screen/learning_screen.cc`
- `integration/metalio_claw4/integration_manifest.md`

## 4. 验证证据

### 4.1 Host Quick gate

命令：`tools/run-app-first-gate.ps1 -TaskId CODEX-APP-FIRST-001 -Mode Quick -OutputRoot out/af3-session-quick-20260906-r3`

结果：`passed=true`；host C++ 测试 **17/17 PASS**（含新增 `learning_backend_session_tests`）；Virtual Device App gate PASS；接口契约检查 PASS。

新增测试覆盖：

- 配置 endpoint/signer 后 challenge → auth → today → coordinator sync 的顺序；
- 权威今日快照进入真实 coordinator；
- 无 pending 时不会伪造 events 请求；
- endpoint/signer 缺失时不触网并返回 `auth_not_configured`。

### 4.2 Repo → C5 mirror

命令：`tools/dev/sync-app-first-mirror.ps1 -MirrorRoot E:\c -Mode Sync`

结果：允许列表 **66 files**，`mismatch_after=0`。外部镜像只作为构建输入，不作为权威源码。

### 4.3 C5 BUILD ONLY

构建目录：`E:\workbuddy\claw4-idf-cold-c5-20260906`  
命令：`idf.py -C E:\c -B E:\workbuddy\claw4-idf-cold-c5-20260906 build`

结果：exit 0，`xiaozhi.elf` 链接成功，`xiaozhi.bin` 生成成功。

| 产物 | 大小 | SHA-256 |
|---|---:|---|
| `xiaozhi.bin` | 9,182,112 B | `EF2AEA459A13969DA10D7F9F1A6F94740CDF06EBDA835F3C9E151FC32A11F9BB` |
| `xiaozhi.elf` | 78,212,856 B | `73882B1DE2C36718282FF569EECCABA46C96862AE0706514C6D4089D5AF596ED` |

构建唯一告警是既有 `ota_1` 分区容量不足；本次未修改分区表，也未把该镜像声明为双槽 OTA 候选。`sdkconfig`、partition、bootloader、BSP、driver、eFuse 均未改动。

## 5. 尚未完成与下一门禁

本 checkpoint **不等于真实设备 L2/L3 通过**。当前没有配置真实 endpoint、设备 signer、Wi-Fi/TLS 凭据，因此尚未验证真机认证、今日任务拉取、断网重试、ACK 收敛或屏侧真实网络状态。下一步需要先确定凭据注入/安全存储方案和可访问的测试 Backend，再冻结唯一 ota_0 application-only 候选，进行一次受控刷机与完整屏侧验收；不得把签名密钥提交到 Git。

## 6. 建议复检重点

1. 核对 `LearningBackendSession` 没有复制业务状态机，今日任务只走 `applyTodaySnapshot`，事件只走 coordinator/outbox。
2. 核对 transport 的主循环等待约束没有从 LVGL callback 触发。
3. 核对设备配置层不会把 endpoint、token、signer 或儿童数据写入日志/仓库。
4. 真实设备批次重点观察 `auth_paused`、`pending_count`、`last_acked_sequence`、`last_error` 和 `last_operation`，失败先依据屏侧诊断定位。

