# CODEX-APP-FIRST-002 P3/P6 worker、NVS 注入与真机刷写报告

## 1. 范围与授权

- 任务：`CODEX-APP-FIRST-002`，Codex 直接实施；本轮不调度 WorkBuddy。
- 设备：Claw4 / ESP32-P4 rev1.3 / COM7 / MAC `80:f1:b2:d2:ed:14`。
- 允许范围：`ota_0` 应用刷写、已校验的 NVS 配置注入；未触碰 bootloader、partition table、ota_1、eFuse 或整片擦除。
- 本机服务：Backend `127.0.0.1:8000`，LAN relay `192.168.3.26:18765`；设备实际拿到 `192.168.3.49`。

## 2. 实际修改

- `integration/metalio_claw4/device/app/learning_runtime.{h,cpp}`
  - 启动时加载独立 `learning_cfg`；配置不存在时保持未配置。
  - 新增 FreeRTOS backend worker，每 15 秒执行 auth → today → events cycle。
  - `std::recursive_mutex` 保护 LearningApp/coordinator 与诊断快照的 UI/worker 访问。
  - 诊断返回 redacted value snapshot，不返回 secret 或 token。
- `integration/metalio_claw4/device/learning_screen/learning_screen.cc`
  - UI 回调与刷新持有状态锁；诊断栏展示 configured/network/auth/error/op。
- `tools/dev/merge_nvs_provisioning.py`
  - 本地 NVS 合并工具；secret 仅从 stdin 读取，不写日志/仓库。
  - 重建前后校验所有原 namespace/key，追加 `learning_cfg` 的 4 个字段。

## 3. 验证证据

### NVS 只读检查与注入

- 原分区：`0x3C000`，大小 `0xD2000`（860160 bytes）。
- 原 NVS 完整性：所有已写页 CRC32 `OK`；namespace 为 `board/network/wifi/display/mqtt/websocket/audio/learning`。
- dry-run 合并：`preserved_namespaces=8 preserved_entries=28 added_entries=5`。
- 生成镜像校验：`learning_cfg` 出现，原 8 个 namespace 保留，CRC32 `OK`。
- 设备写入：esptool `write_flash 0x3C000`，`Hash of data verified`。
- 设备回读：readback SHA-256 与生成镜像一致；无 secret 值输出。

### 构建与应用刷写

- C5 mirror build：`ninja -C E:\workbuddy\claw4-idf-cold-c5-20260906`，exit 0。
- 产物：`xiaozhi.bin` 9,249,600 bytes，SHA-256 `682FFBCB58A4F955340A100B2E76AE9BE1049919C25390F675294D4AF1075C44`。
- 已刷 `ota_0 @ 0x200000`，esptool `Hash of data verified`。
- 已知告警：`ota_1` 仍小于当前镜像；本次未修改 partition，属于既有基线告警。

### 真机网络基础

- 启动日志确认 Wi-Fi station connected，设备 IP `192.168.3.49`，网关 `192.168.3.1`。
- 电脑地址 `192.168.3.26`；relay 已监听 `192.168.3.26:18765`，局域网转发到 loopback Backend。
- 用户已进入“学习”页；worker 已发起请求，但设备日志为 `EspTcp: Failed to connect to 192.168.3.26:18765, code=0x71`，尚未到达 HTTP/auth 层。
- 本机 loopback/relay 自测仍为预期 `401`；Windows 入站防火墙规则创建返回“拒绝访问”，当前 Codex 进程无法取得管理员权限。

## 4. 验收结论

- P1/P2：`CHECKPOINT_READY`；配置注入与 HMAC 代码已编译并在设备 NVS 落盘。
- P3：`CHECKPOINT_READY`；worker 与诊断构建通过，运行期已确认发起 TCP 请求，但被本机防火墙阻断。
- P6：`CHECKPOINT_READY`（ota_0 application-only flash）；真机业务链路仍为 `HARDWARE_VERIFY_REQUIRED`。
- 未发生范围偏差；未提交构建目录、NVS dump、镜像或任何密钥。

## 5. 下一步

管理员放行私有 Wi‑Fi 网段到 TCP 18765 后，Codex 再读取 COM7 日志验收 `authenticate`、`today`、`events` 与屏幕诊断；若失败，只修复对应出口，不重刷 bootloader/partition。
