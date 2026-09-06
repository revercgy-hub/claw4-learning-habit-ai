# CODEX-APP-FIRST-002 — 设备侧 Backend 真机联调

> 状态：`READY`（Codex 执行；不调度 WorkBuddy）  
> 前置：`CODEX-APP-FIRST-001` AF3-5 `49599f9`，COM7 ota_0 刷写 `FLASH_PASS_HASH_VERIFIED`  
> 目标：把已编译的 `BackendClient` 会话层接到安全 provisioning、设备 worker 和真实测试 Backend，完成一次可复现的 L2/L3 真机闭环。

## 1. 任务边界

本批只覆盖：设备注册凭据的安全注入、HMAC challenge signer、今日任务拉取、outbox 事件同步/ACK、重试/认证暂停、屏侧诊断和一次 ota_0 application-only 真机验收。

不包含：整片 Flash 擦除、bootloader、partition table、`ota_1`、C5、eFuse、Secure Boot、Flash Encryption、发布 OTA、Voice/STT/MCP/AI、摄像头和儿童数据上传到第三方服务。

## 2. 有序 checkpoint

### P0 — 联调输入冻结（用户配合）

用户需提供或确认：

1. 测试 Backend 的局域网地址和端口（优先 HTTP 局域网 relay；若使用 HTTPS，提供证书校验策略）；
2. 已注册测试设备的 `device_id`、绑定 `child_id` 和一次性 `device_secret` 交付方式；secret 不提交 Git、不写入聊天记录、不打印到日志；
3. Claw4 可连接的 Wi-Fi 配置方式；
4. 是否保持 Home 默认启动，还是在本批临时让 Learning 作为启动后的首屏。

未收到这些输入前，只做 host/build，不把空配置写成联网失败。

### P1 — Provisioning 契约与存储

- 新增平台无关 `BackendProvisioning` 接口：读取 endpoint、device ID、child ID 和不可回显 secret；缺失时返回 `NotConfigured`。
- 设备适配使用现有 NVS 能力，限制在 learning 业务命名空间；不得擦除已有任务/outbox 状态，不在日志中打印 secret/token/nonce/signature。
- 本实现使用独立 `learning_cfg` namespace，避免 `ResetToSeed()` 清理业务状态时删除配对凭据；MVP 仍记录为 NVS 明文存储，正式安全存储另列硬化任务。
- 写入采用临时缓冲→校验→commit；失败保留旧配置。
- host fake 覆盖空配置、损坏配置、重启读回和旧配置保留。

验收：secret 不出现在源码、Git diff、日志和诊断字符串；配置 commit 失败不改变旧值。

### P2 — Target-portable HMAC signer

- 使用现有 ESP-IDF/mbedTLS 能力实现 `HMAC-SHA256(device_secret, device_id|challenge_id|nonce)`；纯 host 版本使用同一字节串和测试向量。
- signer 只在认证调用栈内使用，认证后立即释放临时明文；不得持久化 access token 到日志。
- 增加 challenge 负例：secret 缺失、签名输入顺序错误、后端 401、challenge 重放。

验收：与 Backend security 测试向量一致；host signer tests 全绿；C5 编译通过。

### P3 — Worker/调度接线

- 在设备主循环/worker context 调用 `LearningRuntime::RunOnlineCycle()`，不得从 LVGL callback 阻塞调用。
- 周期：启动后首次在线尝试一次；之后由 worker 按退避执行；网络离线/认证暂停时只更新诊断，不破坏本地 outbox。
- 保持 `ResetToSeed()` 释放旧 session；真实任务不得被 demo seed 覆盖。

验收：host fake scheduler 覆盖首次拉取、离线、恢复、401 pause/reset；静态检查证明 LVGL 回调不直接触发 HTTP。

### P4 — 诊断与屏侧验收准备

Learning 诊断至少显示：`configured`、network、auth、pending、last ACK、last error、last operation、last sync time。未配置状态必须明确显示 `backend 未配置`，不得显示“在线/认证成功”。

验收：诊断字段来自 `BackendSessionDiagnostics`，不复制业务状态机，不显示 secret/token/儿童内容。

### P5 — Host/Backend 联调门禁

- 真实 Backend 注册/配对后的 challenge→auth→today→events/ACK 全链；
- 断网、响应丢失、401、重复 ACK、连续重启至少 10 轮；
- PWA 只出现一条 completed session，pending 最终归零，ACK 单调前进；
- 冻结唯一 C5 候选，Quick/Full/IDF Build 全绿。

验收标志：`APP_FIRST_DEVICE_BACKEND_HOST=PASS`。

### P6 — 一次性真机验收

只执行 ota_0 application-only 写入 `0x200000`，不擦除整片 Flash。用户按屏侧验收单执行：

1. Learning 显示候选 build/diagnostics；
2. 在线拉取今日任务；
3. 断网 Start→Pause→Resume→Complete；
4. 重启后任务状态和 pending 保留；
5. 恢复网络，pending 归零、ACK 前进；
6. PWA 刷新后仅有一条学习记录。

验收标志：`APP_FIRST_DEVICE_BACKEND_HW=PASS`。失败时只采集屏侧错误码和必要 monitor 证据，不自动 erase 或换分区。

## 3. 当前停止条件

- 没有 endpoint、Wi-Fi 或 secret 注入方案：停在 P0/P1，不刷机；
- 认证签名向量不一致、越权、重复事件不幂等：立即停止；
- 需要改 partition/bootloader/ota_1/C5/eFuse：停止并重新请求用户授权；
- 发现真实任务会被 demo seed 或 reset 路径覆盖：停止并先修复持久化边界。

## 4. 交付物

- 本任务包对应报告：`docs/project_management/reports/CODEX_APP_FIRST_002_DEVICE_BACKEND_*.md`；
- 每个 checkpoint 独立提交并 push；
- 最终报告记录 endpoint 类型、候选 SHA、串口、操作地址、屏侧结果和未解决风险；
- 任何 secret、token、nonce、原始儿童数据和完整设备日志均不得入 Git。
