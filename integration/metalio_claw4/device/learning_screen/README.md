# Learning L0 screen（P18 设备侧源码权威副本）

> 用途：本目录为 Learning L0 screen 的**仓库侧权威副本**； 时同内容同步到 Metalio 构建树
> `E:/c/main/display/screen/learning_screen/{learning_screen.h,learning_screen.cc}`（E://c = vendor/MetalioClaw4 @ ca3aa3fa 的短路径构建镜像）。
> 官方文件改动（home_screen.cc / main/CMakeLists.txt）逐条登记于 `../integration_manifest.md`。
> L0 范围：Home→Learning→Mock Task→Start→Back；无 backend/NVS/voice/STT/MCP/AI（UI 本地 mock 状态）。

## L1 device tree（WB-LEARNING-V4-L1）

- `../core/`：`outbox_codec.{h,cpp}`（OutboxState<->blob，纯 C++17 host 可测）、`demo_seed.h`（首启演示快照，L2 被 server 快照替换）。
- `../ports/`：`learning_clock.{h,cpp}`（ClockPort<-esp_timer）、`nvs_outbox_storage.{h,cpp}`（OutboxStorage<-NVS namespace `learning`/key `st`）。
- 构建镜像：repo 权威副本同步到 `E:/c/main/learning/`（fw 业务核心 = repo `firmware/main/{learning_domain,sync,application,interaction,mcp,ui,ports}`；glue/device = repo `integration/metalio_claw4/{host_glue,device}`），官方 `main/CMakeLists.txt` 由 `integration_manifest.md` #3 登记（INCLUDE_DIRS +learning、SOURCES +10 行）。
- AF3 更新：`sync/scheduled_http_transport.{h,cpp}` 是平台无关的主循环边界；`device/ports/metalio_http_transport.{h,cpp}` 才引用官方 `Application`/`Board`/`NetworkInterface`，每次网络调用都由 `Application::Schedule` 执行。该 adapter 需在恢复 C5 构建镜像后加入 source registration 并做 BUILD ONLY 验证，当前不宣称已编入固件。
- 同步命令（在仓库根执行）：
```bash
rm -rf /e/c/main/learning && mkdir -p /e/c/main/learning
for d in learning_domain sync application interaction mcp ui ports; do mkdir -p "/e/c/main/learning/$d"; cp -r "firmware/main/$d/." "/e/c/main/learning/$d/"; done
mkdir -p /e/c/main/learning/metalio_claw4/host_glue /e/c/main/learning/metalio_claw4/device/core /e/c/main/learning/metalio_claw4/device/ports
cp -r integration/metalio_claw4/host_glue/. /e/c/main/learning/metalio_claw4/host_glue/
cp -r integration/metalio_claw4/device/core/. /e/c/main/learning/metalio_claw4/device/core/
cp -r integration/metalio_claw4/device/ports/. /e/c/main/learning/metalio_claw4/device/ports/
```
