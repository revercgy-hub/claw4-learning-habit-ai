# Metalio 集成改动登记表（integration_manifest）

## 2026-09-15 A05 补丁层：新增五个 learning 源登记

`A05` 层在 A01 之上只做一件事：把本包新增的五个实现单元登记进上游 `main/CMakeLists.txt` 的 `SOURCES`。补丁与元数据：

- [a05-0001-cmake-source-registration.patch](patches/a05-0001-cmake-source-registration.patch)（sha256 `51e2db0411917176c0cbdaafa927d579f7c3547b60e16f838acb753ba463ec5e`）
- [a05-0001-cmake-source-registration.json](patches/a05-0001-cmake-source-registration.json)（base / 应用顺序 / 前后 hash / 回滚）

| 序号 | 上游 commit | file | 变更 | 原因 | 回滚 | 风险 | 状态 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A05-1 | `ca3aa3fa` | `main/CMakeLists.txt` | `SOURCES` 追加 5 行：`learning/sync/sync_executor.cpp`、`learning/sync/single_flight_http_transport.cpp`、`learning/time/time_authority.cpp`、`learning/interaction/interaction_arbiter.cpp`、`learning/reminder/reminder_core.cpp` | WB-A05-BUILD-001 CP1：本包新增的 5 个实现单元此前不在镜像 CMake 清单内，固件未包含其代码 | `git apply -R -p1 <repo>/integration/metalio_claw4/patches/a05-0001-cmake-source-registration.patch` | 低：源列表仅追加；未改任何语义、未加 dummy 调用/whole-archive/强制符号/关节裁剪 | **PATCH READY**（隔离 fixture 前向/反向均 rc=0，结果 hash 命中；`main/CMakeLists.txt` 补丁后 `14ffc5c3…`） |

**应用顺序（不可颠倒）**：`project-ca3aa3fa.patch` → `a05-0001-cmake-source-registration.patch`。
A05 的 base 是 **A01 补丁之后**的 `main/CMakeLists.txt`（`9f7098e0…`，与 `E:/c` 当前该文件逐字节相同）；A05 只对 `main/CMakeLists.txt` 生效，不改 BSP / 驱动 / sdkconfig / 分区 CSV / bootloader / ota_1 / eFuse。

**隔离重放树**：`E:/claw4-a05-19fd979/src`，构造顺序 = `pin ca3aa3fa 归档 → LF 归一 → A01 → 19fd979 学习源码同步（90 文件）→ 冻结 C5 sdkconfig → A05`。

**同步缺口（19fd979 相对旧镜像 `E:/c`）**：29 个学习文件与 19fd979 不一致，其中 **13 个真实内容差异**、16 个仅行尾差异；**17 个文件在镜像里不存在**（`time/`、`reminder/`、`interaction/interaction_arbiter.*`、`sync/{sync_executor,single_flight_http_transport,session_lease,session_snapshot_publisher}.*`、`ports/reminder_wake_port.h`、`assistant/command*.h`、`telemetry/logger.h`、`metalio_claw4/device/core/learning_boot_policy.h`）；**镜像侧没有任何文件缺少 repo 来源（零未解释漂移）**。完整账目见实施报告 §CP1。

## 2026-09-14 精确补丁取证补充

历史 #1–#6 及 AF3/APP2 的现存四文件集成变化，已从固定 upstream `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950` 与只读 `E:/c` 比较保存为 [project-ca3aa3fa.patch](patches/project-ca3aa3fa.patch)，对应 [SHA清单](patches/project-ca3aa3fa.json)。主agent已在临时fixture验证应用、四文件hash及反向恢复。它是既有改动的取证，不代表新增部署/音频变更授权或本批IDF构建通过。

历史表内整文件checkout回滚命令仅保留为原始记录；`E:/c` 无可用Git元数据，本轮不得执行。后续候选使用固定upstream和精确补丁重建，回滚先核对同一候选，再在隔离工作树使用补丁反向校验；不覆盖他人完整文件。现存CMake尚未登记新TimeAuthority等源码，A05必须补齐注册并重新构建。验证脚本：`tools/dev/tests/verify-metalio-patch.py`。

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
| 4 | `ca3aa3fa` | `main/CMakeLists.txt` | `SOURCES` 列表新增一行 `"learning/metalio_claw4/device/ports/metalio_voice_session.cpp"` | L4 (P25 Voice STT)：VoiceSessionPort 设备实现（`SetVoiceUiDesired` / `AbortSpeaking` 映射）纳入编译 | `git -C E:/c checkout -- main/CMakeLists.txt`（连同 #1~#3 一并还原） | 低：源列表仅追加一行，不影响其它源 | **APPLIED**（L4 build rc=0，`xiaozhi.bin` 9,265,344 B） |
| 5 | `ca3aa3fa` | `main/display/lv_adapter_display.cc` | ① 顶部新增 `#include "screen/learning_screen/learning_screen.h"`；② `SetChatMessage` 路由策略新增一条：**学习屏在前台**时把 `user` 文本交给 `LearningScreen::QueueVoicePhrase(content)` 后 return，`bot` 文本丢弃 | L4 (P25)：上游原策略为「其它屏 -> 直接丢弃」，导致学习屏**永远拿不到语音识别文本**，L4 无法接线 | 单文件最小 diff：`git -C E:/c checkout -- main/display/lv_adapter_display.cc` | 中：位于显示分发路径，但仅新增一个前台判断分支；学习屏不在前台时行为与上游逐字节一致 | **APPLIED**（L4 build rc=0；字面量自检 `LearningVoice`/`语音`/`还剩约` 等全部命中） |
| 6 | `ca3aa3fa` | `main/audio/audio_service.cc` | wake word 喂数循环（`AS_EVENT_WAKE_WORD_RUNNING` 分支）：`Feed()` 后新增 `vTaskDelay(pdMS_TO_TICKS(1))` 让步点，并把 `lock_guard` 收进内层作用域以免持锁 sleep | **修复 task_wdt 全系统卡死**：AFE ringbuffer 满时 `Feed()` 变为非阻塞立即返回，原循环直接 `continue` 无让步点 → 退化为忙循环饿死同核 IDLE0（实测 8733 次 ringbuffer 告警、每 10 s 触发一次 `task_wdt`，`CPU 0: audio_input`） | `git -C E:/c checkout -- main/audio/audio_service.cc` | **中高**：位于音频主路径；每轮最多增加 1 tick(10 ms)，音频数据由硬件/软件缓冲吸收。需回归唤醒、对话与 AEC 链路 | **APPLIED**（L4 修复版 build rc=0；**待真机验证**） |

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

| APP2-P1 | `firmware/main/sync/backend_provisioning.h` + `integration/metalio_claw4/device/ports/nvs_backend_provisioning.{h,cpp}` | `main/CMakeLists.txt` source list | 独立 `learning_cfg` NVS 配置，endpoint/device/child/secret 原子写入与读回；ResetToSeed 不清理凭据 | MVP secret 仍为 NVS 明文存储，禁止日志/UI/Git 暴露；不改 secure boot/flash encryption | **C5 BUILD ONLY PASS**（与 APP2-P2 同批） |
| APP2-P2 | `integration/metalio_claw4/device/ports/metalio_hmac_signer.{h,cpp}` | `main/CMakeLists.txt` source list | mbedTLS HMAC-SHA256 + standard Base64，输入 `device_id|challenge_id|nonce`；`ConfigureProvisionedBackend()` 注入 signer | 不打印 secret/nonce/signature/token；不从 LVGL callback 触发网络 | **C5 BUILD ONLY PASS**（`xiaozhi.bin` 9,179,872 B；SHA-256 `C57DF844…87872251`；ota_1 既有容量告警保留） |
