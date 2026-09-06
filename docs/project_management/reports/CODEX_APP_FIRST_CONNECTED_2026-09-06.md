# App-first 真实 HTTP / 重启 / 浏览器闭环复核

任务：CODEX-APP-FIRST-001；执行：Codex；分支：`codex/app-first-mvp-loop`。

## 结论

Host connected checkpoint：`CHECKPOINT_READY`。AF3 host scheduler 与 runtime 启动恢复接线已完成；**不是** `APP_FIRST_MVP_LOOP=PASS`，不安排刷机。

真实 C++ LearningApp → BackendClient/wire codec → loopback HTTP → SQLite → PWA 已有业务证据。设备注册/凭据、页面诊断和精确镜像集成仍待实施；当前设备适配源码仍只存在于本仓库，未宣称已进入 E:/c 构建树。

## 不可变代码提交与修改范围

| 提交 | 范围 | 修复/实现 |
| --- | --- | --- |
| `190fcb8` | frontend App 与集成测试 | 刷新后恢复 Authorization；退出清除令牌 |
| `e4a0df8` | coordinator、reducer、后端投影及单测 | 重启时钟基准、零时刻计时、暂停次数传输与幂等投影、非法输入 422 |
| `649f7d1` | connected C++ runner + Python relay | 真正结束/重建进程；复用设备 outbox codec；Python 只负责 HTTP/签名/文件 I/O，不构造或改写业务事件 |
| `1757ef2` | 三个 gate 脚本 | 子脚本不再清除早先失败；修正 UI include 误报；共用实现每轮编译一次；Full 不重复跑 host；接入 connected gate 与 lint |
| `49ce7a8` | `ScheduledHttpTransport`、Metalio HTTP 薄适配、runtime 启动恢复接线及报告 | 所有设备 HTTP 调用经 `Application::Schedule` 主循环；host 16/16 与 scheduler 单测通过 |
| `bf16b03` + `e115074` | 白名单镜像同步工具、AF3 CMake BUILD ONLY 与屏侧启动保护 | 64/64 文件 SHA 一致；C5 构建成功编译两个新 adapter 对象及 Learning screen 保护 |

关键复检发现：

- 原测试只推进 epoch，没有推进 monotonic，导致完成事件时长为零；新 runner 同时推进两个时钟，并断言时长和 XP。
- reducer 将合法 monotonic=0 当成未开始，现按 Running 状态判定，同时保留时钟回退拒绝。
- 暂停后重启保留旧计时起点，可能导致 Resume 拒绝。`prepareAfterBoot` 保留已结算时长、暂停次数、pending 和 sequence，持久化新起点；失败不发布。关机时间、未结算时间不凭墙上时间补算。设备 runtime 已接入启动前恢复门禁，但尚未在 E:/c 的正确 C5 镜像中编译验证。
- 完成/保存/终止事件补充可选 pause_count，后端兼容旧事件默认 0；新值只接受非负整数或整数字符串，重复事件不重复累计。
- 原 `$global:FAILED` 会被 interface 子脚本重置，导致之前失败被改写成 PASS；已改为本脚本私有变量。UI 扫描由 `sync` 子串改为 `sync/` 模块路径，避免错拦 `ui/sync_diagnostics.h`。

## 验证

直接测试活动工作树，不复制产品源码到其他工作树代测：

```powershell
& tools/dev/run-app-first-gate.ps1 -Mode Full `
  -CompilerPath E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe `
  -PythonPath 'E:/workbuddy/学习习惯培育AI/backend/.venv/Scripts/python.exe' `
  -LogDir out/full-20260906
```

- Full exit 0；09:34:15～09:35:41，约 86 秒（本机本轮，不作为跨机器基准）。运行时 HEAD 是 `49ce7a8`；以下 host 证据对应该提交及其父提交边界。
- 共用实现 12 个对象各编译一次；C++ 16/16 测试二进制、interface gate PASS。coordinator 26 cases、domain 28 cases。
- Backend：79 passed，1 条已有依赖弃用告警；含 9 个新增参数化计数案例。
- PWA：typecheck/lint/build PASS，5 files / 31 tests PASS。
- AF3 scheduler：C++ host gate 16/16 PASS（新增 `scheduled_http_transport_tests`）；adapter 只在 device TU 引用 `board.h`/`application.h`，BackendClient 仍保持纯 C++。
- repo→E:/c 白名单同步：64 个文件，`Check` 结果 `mismatch_before=0/mismatch_after=0`；同步工具为 `tools/dev/sync-app-first-mirror.ps1`，只复制 allowlist，不删除文件。
- 屏侧保护：`LearningScreen::RefreshUi` 在 `bootReady()==false` 时不解引用空 app、不接受触摸/self-test，并显示恢复失败；正常态显示本地 pending/ACK/配对待验证。
- connected：5 轮、55 次真实进程重启；30 accepted + 30 duplicates；每轮 pending 6→0、ACK 0→6。
- 首次 Start 注入保存失败：旧 blob 字节不变、无活动会话、pending 0。断网暂停保留 3 pending；进程重启恢复后 Resume/Complete 成功。
- 后端提交后丢弃响应，再 kill/restart；重传请求字节完全一致，6 个 event_id 全唯一，sequence 1..6 连续。
- 每任务仅一条完成记录，120 秒、pause_count=1、XP=2；重新拉取 Today 为 Completed。

生成物位于忽略目录 `out/full-20260906/`、`out/connected-app-timing/`。文件原子替换证明进程恢复，不等价于操作系统掉电或真实 NVS 验证。

## 浏览器业务证据

通过真实 PWA 表单创建 `合成计时全链 B/C/D 20260906` 三个任务。后端仅监听 127.0.0.1:18765，PWA 为 127.0.0.1:5173。

connected runner 的 `--base-url`/`--task-id` 模式执行这些同一 task_id：3 场景、6 次进程重启、18 accepted + 18 duplicates。任务不是 Python 代建，设备事件仍由 C++ 产生。

实际页面确认：B/C/D 各一条记录，均为 **2 分、暂停 1 次、+2 XP、已完成**；reload 后无需重新登录，今日任务中三项均显示已完成。

这是同一浏览器会话的三个业务场景，不是三次全新浏览器环境的自动化启动。9 月 5 日 A 场景 0 秒/0 次记录保留为缺陷证据，计时结论已被 B/C/D 替代。浏览器工具按 computer-use 技能执行真实表单及页面复核。

## 冷构建与配置门禁

独立 C5 配置副本 cold build exit 0：`E:/workbuddy/claw4-idf-cold-c5-20260906/xiaozhi.bin`，9,176,176 B，SHA-256 `f3a8a58c9ea093f6363a264a72802b7323693982c3bb0efef8a047b5a38bcac3`。本次 BUILD ONLY 日志明确包含 `scheduled_http_transport.cpp`、`metalio_http_transport.cpp` 与 Learning screen 保护的编译和链接；**禁止刷写，不是 L2/L3 候选**，因为注册/凭据、设备诊断和真机网络链尚未完成。

旧冷构建 `E:/workbuddy/claw4-idf-cold-20260905c/xiaozhi.bin`（9,188,624 B，SHA-256 `400b6ba665cb07767236ba2cbad7f6c8462c9999a0143f8dd7e9b2bd39b30d50`）同样仅作历史证据，禁止刷写。

输入 E:/c/sdkconfig 已偏离已验收 E:/b/config/sdkconfig.h：前者 ESP-Hosted 选择 H2，缺少原 C5 remote 选择。修正 ESP_IDF_VERSION=5.5 后输出副本默认 remote C6，仍不等于原 C5 基线。编译成功不证明硬件配置正确。ota_1 溢出告警保留，未改分区。

只读核对发现，`E:/workbuddy/学习习惯培育AI/vendor/MetalioClaw4/sdkconfig` 保留 C5/C5、32 MB、metalio-claw-4、32m_dual.csv；其显式设置与 E:/b/config/sdkconfig.json 同名键比较差异 0，且本次 C5 构建输出与 vendor C5 同名键差异 0。这不是完整重建零漂移证明，未覆盖全部 unset/default/工具链差异。

本轮只在构建输出目录使用 sdkconfig 副本；E:/c 仅同步 allowlist 源文件并追加两条已登记 CMake source registration，未回写 sdkconfig/vendor 配置；无串口、刷写、擦除操作。

## 下阶段

1. 完成设备注册/凭据与页面诊断入口，并继续保持网络回调只经主循环；host/BUILD ONLY 证据不等于真机 Wi-Fi/TLS/NVS 已验证。
2. 在不改 C5/BSP/driver/sdkconfig/分区的前提下，继续跑 PWA→Backend→device runtime 的接口接线和 host 契约测试；禁止继续使用会自动擦除 learning namespace 的旧自测链处理真实任务。
3. 只有设备网络链、屏侧诊断和一次性真机验收单齐备后，才冻结唯一候选并通知用户连接设备。

范围：本轮 Host/PWA/后端/验证工具。未触碰官方设备源码、Flash、partition、bootloader、ota_1、eFuse、真实儿童数据。设备全链未完成。
