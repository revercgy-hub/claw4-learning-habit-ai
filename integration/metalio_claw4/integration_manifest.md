# Metalio 集成改动登记表（integration_manifest）

> 政策：`docs/METALIO_ADAPTER_BRIDGE_PLAN_P17.md` §2B（底层少动，上层深做）。
> 任何 **Metalio 官方文件**改动必须在触碰前登记本表并满足：
> - 仅限允许范围（Home App Registry / Learning Screen registration / CMake component·source registration / 必要 include·build glue）；
> - 单条可回滚（最小 diff、可反向/checkout）；
> - 严格禁止：BSP / board driver / MIPI / GT911 / Audio low-level / ESP-Hosted / Power driver / sdkconfig / partition CSV / bootloader / ota_1 / eFuse / Secure Boot / Flash Encryption。

| 序号 | upstream commit | file | lines/functions | reason | rollback | risk | 状态 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `ca3aa3fa` | `main/display/screen/home_screen/home_screen.cc` | ① 顶部新增 `#include "learning_screen/learning_screen.h"`；② `kApps[]` 前新增 `LaunchLearning()` + `learning_lifecycle_cb()`（参照 LaunchVibrate 模式：Create→screen_attach_lifecycle→lv_screen_load）；③ `kApps[]` 在 settings 行后新增 `{"learning","学习",LaunchLearning,learning_lifecycle_cb,false}`（无图标资源，格子图标留空但可点） | Home App Registry 极小集成补丁：Home 网格增加 Learning 入口（保留原厂 Home/Chat/OpenClaw/Settings/Test，不改默认 Boot Home/不隐藏原厂 App） | 单文件最小 diff：`git -C E:/c checkout -- main/display/screen/home_screen/home_screen.cc` | 低：仅新增条目；若 Learning 缺失不影响其余 App；原厂 Chat 等路径零改动 | **APPLIED**（P18 build PASS，`xiaozhi.bin` 0x89B7F0） |
| 2 | `ca3aa3fa` | `main/CMakeLists.txt` | `SOURCES` 列表新增一行 `"display/screen/learning_screen/learning_screen.cc"` | 把 Learning L0 screen 源纳入官方 main 组件编译 | 同文件 checkout 还原该行 | 低：源列表仅追加，不影响其他源 | **APPLIED**（P18 build PASS） |
| 3 | `ca3aa3fa` | `main/CMakeLists.txt` | ① `INCLUDE_DIRS` 追加 `${CMAKE_CURRENT_SOURCE_DIR}/learning`；② `SOURCES` 增 11 行 learning 源（`learning/learning_domain/reducer.cpp`、`learning/sync/outbox_core.cpp`、`learning/application/coordinator.cpp`、`learning/interaction/dispatcher.cpp`、`learning/interaction/stt_mapper.cpp`、`learning/mcp/learning_mcp_host.cpp`、`learning/ui/presenters.cpp`、`learning/metalio_claw4/device/core/outbox_codec.cpp`、`learning/metalio_claw4/device/ports/learning_clock.cpp`、`learning/metalio_claw4/device/ports/nvs_outbox_storage.cpp`、`learning/metalio_claw4/device/app/learning_runtime.cpp`） | WB-LEARNING-V4-L1：host 业务核心 + L1 device core/ports 编入固件（镜像目录 `E:/c/main/learning/`，权威副本在 repo firmware/main 与 integration/metalio_claw4/device） | `git -C E:/c checkout -- main/CMakeLists.txt`（连同 #1/#2 一并还原） | 中：新增编译单元仅由 Learning 屏调用，不注册中断/服务；镜像树与 repo 双份，同步脚本见 `integration/metalio_claw4/device/README.md` | **APPLIED**（L1a/L1b `idf.py build` exit=0，learning 源入固件，sdkconfig diff=0） |

> 新增学习屏源码本身（`learning_screen.{h,cc}`）是 **repo 侧新代码**（放入 E://c 构建树），按 P18 范围仅 UI mock（无 backend/NVS/voice/STT/MCP/AI），非官方文件改动、不在本表登记范围（与官方文件区分记录于 `METALIO_ADAPTER_BRIDGE_PLAN_P17.md`/报告 §13 后段）。

- 正式 pin（复核于 2026-09-03）：`ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950`（vendor/MetalioClaw4 HEAD 与此一致，无上游漂移）。
- 新增 repo 侧（非官方文件，无需登记）：`integration/metalio_claw4/host_glue/*`（P17a，host 可测）、`firmware/tests/unit/metalio/learning_app_glue_tests.cpp`。

## AF3 追加登记（待正确 C5 镜像 BUILD ONLY）

| 序号 | repo 文件 | 设备镜像目标 | 接入点 | 禁止事项 | 状态 |
| --- | --- | --- | --- | --- | --- |
| AF3-1 | `integration/metalio_claw4/device/ports/metalio_http_transport.{h,cpp}` | `main/learning/metalio_claw4/device/ports/` | `NetworkInterface::CreateHttp`，由 `Application::Schedule` 调度 | 不改 BSP/driver/sdkconfig/partition/bootloader/ota_1/eFuse；不在网络线程直接触碰 LVGL/domain | **REPO READY / BUILD ONLY 待 C5 镜像** |
| AF3-2 | `integration/metalio_claw4/device/app/learning_runtime.cpp` | 同路径 | `prepareAfterBoot(clock_.monotonicMs())` 启动恢复门禁 | 恢复失败不得启动 LearningApp；不以墙上时间补算未结算时长；禁止旧自测擦除真实任务 | **REPO READY / BUILD ONLY 待 C5 镜像** |
| AF3-3 | `firmware/main/sync/scheduled_http_transport.cpp` + `integration/metalio_claw4/device/ports/metalio_http_transport.cpp` | `main/CMakeLists.txt` source list 追加两行 | 平台无关等待边界与 Metalio 主循环 HTTP 实现进入同一 BUILD ONLY | 仅 source registration；不改 CMake 其它组件、BSP、sdkconfig、partition、bootloader、ota_1/eFuse | **BUILD ONLY PASS**（CMake SHA `3E4A76BD…` → `603BD4A4…`；C5 mirror SHA check 64/64） |
| AF3-4 | `integration/metalio_claw4/device/learning_screen/learning_screen.cc` | 同路径 | 启动恢复失败时关闭交互并显示非硬件诊断；正常态显示 pending/ACK/配对待验证 | 不显示或伪造 Wi-Fi/TLS/NVS 已验证结论；self-test 仅在 `bootReady()` 后启动 | **BUILD ONLY PASS**（C5 `xiaozhi.bin` 9,176,176 B） |

| AF3-5 | `firmware/main/sync/{wire_codec.cpp,backend_client.cpp,learning_backend_session.cpp}` | `main/CMakeLists.txt` source list | 将已验证的 target-portable codec、BackendClient 与认证→今日快照→事件 ACK 会话编入设备应用；凭据由上层 signer 注入，不在固件硬编码 | 不写入真实凭据；网络会话必须从 worker/task 调用，禁止在 LVGL 回调阻塞；不改 BSP/sdkconfig/partition/bootloader/ota_1/eFuse | **BUILD ONLY PASS**（C5 `xiaozhi.bin` 9,182,112 B；SHA-256 `EF2AEA45…A11F9BB`；ota_1 既有容量告警保留） |
