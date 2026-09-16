# A05-DEVICE 实机 Smoke 矩阵（device-smoke-matrix）

设备：ESP32-P4 rev v1.3 / 32MB / MAC 80:f1:b2:d2:ed:14 / COM7
Candidate：`claw4-v53-m0-a05-2c8f58f`（sha256 `c035e1c0…8d36`）
时间窗：2026-09-16 12:30 – 15:00 (+08:00)
**脱敏（任务书 §34）**：本文件不含 Wi-Fi 密码 / token / credentials / NVS dump；含凭据产物仅本地。

| 项目 | 状态 | 证据 / 备注 |
|---|---|---|
| Exact Candidate Hash | **PASS** | 刷前重算 size 9,271,760 / sha256 `c035e1c0…` == 冻结值；刷后回读 0x200000 同样 SHA256 一致；**联网后再次全量回读仍然一致** |
| Device Identity | **PASS** | ESP32-P4 rev v1.3, USB-Serial/JTAG, 32MB GigaDevice c8:4019 |
| Flash Size | **PASS** | 32MB ≥ 冻结布局 0x01FA6000 |
| Partition Gate | **PASS** | 实机 13 项 == 冻结 `32m_dual.csv`；且候选 partition-table.bin 与实机 @0x9000 逐字节相同 |
| OTA Selection | **PASS** | otadata[0] seq=1/state=VALID ⇒ boot_index=(1-1)%2=0 ⇒ ota_0；**联网后复核仍未变** |
| Recovery Path | **CONFIRMED** | ROM download mode 本轮多次实证可达；备用 UART0/CH340(COM6) |
| Pre-flash Backup | **PASS** | 8 个区域，rc=0，逐个 SHA256（含 ota_0/ota_1 全量 app，可完整回滚） |
| Application Flash | **PASS** | `Hash of data verified.` 擦写范围 0x200000–0x0AD7FFF（未溢出 ota_0） |
| Boot | **PASS** | 冷启动 1 次（拔插电源）+ 软复位 ≥10 次；`Calling app_main()` 稳定到达；无 reboot loop |
| Monitor fatal scan | **PASS** | 46 处关键字命中**全部是 `rst:0x17 (CHIP_USB_UART_RESET)`**（主机开串口触发）；panic/Guru Meditation/LoadProhibited/StoreProhibited/IllegalInstruction/stack overflow/heap corruption/assert/brownout/WDT = **0 命中** |
| UI Smoke | **PASS** | 现场观察：屏幕正常显示应用界面、画面稳定、触摸正常、可进入 Learning Screen、页面可切换 |
| Audio/Voice Smoke | **PASS（基础）** | 现场观察：上电/复位听到开机提示音（音频链路在用）。⚠️ 云语音交互（MQTT 8883 / WS）本轮**未取得有效会话**，用户侧见"离线"字样见下；20 轮稳定性与语音闭环均保持 DEVICE_VERIFY_REQUIRED |
| Network Smoke | **PASS（含限制）** | ① 配网凭据落盘（IDF `nvs_tool.py` 解析）+ 跨掉电保持；② **两次完整 OTA HTTPS 往返**：服务端下发的 `mqtt:username/password` 写入 NVS（该键只能由 `ota.cc:178-191` 解析 OTA 响应写入）；③ 用户现场确认屏幕显示已在线/已激活；④ 上游 `api.tenclass.net` 可达（443/80/8883 OK）；⑤ **Learning backend 链路 NOT_EXERCISED**（后端 `http://192.168.3.26:18765` 不在设备网段，用户确认非 3 网段；主机侧同不可达）；⑥ NETWORK_TOTAL_DEADLINE **不得**宣称 PASS |
| Learning Runtime Smoke | **PASS（运行时级）** | 入口级：可进入 Learning Screen、任务列表正常；运行时级：配网后窗口内 outbox **新增 4 个事件 id + 1 个新会话 id（`sess-3c6b94c087314621`）、0 移除**，全部落 NVS（`nvs_event_delta.py`）⇒ LearningRuntime / OutboxStorage / 事件工厂在真机持续工作；同步失败优雅（无崩溃、无丢失） |
| NVS Regression Smoke | **PASS（基础）** | `nvs_tool.py -i` 无 error；多次复位 sha256 恒定；**拔插掉电后**配网凭据与学习状态保持；参见 nvs-persistence.txt |
| Reboot Stability | **PASS** | 连续复位 3 次 + 累计 ≥10 次复位 + 1 次真实掉电上电，均正常启动，无 panic/异常复位源 |

## 现场观察记录（用户确认）
| 项 | 观察结果 |
|---|---|
| 屏幕状态 | 正常应用界面，画面稳定 |
| 其他可感知现象 | 听到开机音/提示音 |
| 触摸 + Learning Screen | 触摸正常，可进入学习页，任务列表可见且正常，页面可切换 |
| 网络（配网 + 拔插后） | **已在线 / 已激活**（无激活码待绑定） |
| 语音交互 | 出现"网络离线"类提示（位置待确认；源码中两处"离线"判定均与公网无关，见下） |

## 关于"网络离线"提示的定性（不得判 FAIL）
1. `learning_screen.cc:301` → 学习页诊断行"· 网络离线"来自 `BackendSessionDiagnostics.network_online`，
   即**学习后端**（192.168.3.26:18765）可达性。设备不在该网段 ⇒ **预期**。
2. `learning/ui/presenters.cpp:25 isOffline()` → 首页离线徽标 = `OfflineIdle || Error || !time_synced`；
   `DomainState.time_synced` 默认 false，M0 **未接入时间同步**（时间/`/today` = C01）⇒ **预期**。
3. 与公网无关：设备已完成 OTA HTTPS 往返（见 Network Smoke ②），Wi-Fi/互联网链路是通的。

## 已明确不在本轮范围
| 项 | 状态 |
|---|---|
| ReminderCore / TimeAuthority / InteractionArbiter 主动提醒 | NOT IN SCOPE（C03；本候选 GC_DISCARDED + COMPILED_NOT_WIRED，**主动提醒不触发是预期状态**，不得判 FAIL） |
| 20-round audio stability | DEVICE_VERIFY_REQUIRED |
| DEVICE_UI_LATENCY | DEVICE_VERIFY_REQUIRED |
| NETWORK_TOTAL_DEADLINE / TCP real loopback / 各段 latency | DEVICE_VERIFY_REQUIRED（`EspTcp::Connect()` 不继承 HTTP total timeout；`Disconnect()` 可能等 ~10 s） |
| Learning backend 真实同步往返 | NOT_EXERCISED（后端局域网地址不在设备网段；改配置需写 NVS，§1.2/§15 禁止） |
| 云语音闭环（MQTT/WS 真实会话） | DEVICE_VERIFY_REQUIRED |
| 上游自动 OTA 升级路径实测 | 未触发（本轮上游未下发更高版本）；**风险已记录**：`ota_url` 指向公网上游，联网状态下存在被替换的可能，故本轮已做候选完整性复核（otadata + ota_0 全量 sha256 + ota_1 头，全部未变） |
| Dual-slot OTA | UNAVAILABLE（app 9,271,760 B > ota_1 4,194,304 B） |
| 完整 `app_main()` 之后串口日志 | UNOBTAINABLE_THIS_ROUND（USB-Serial/JTAG 在应用初始化 USB PHY 时重枚举；主机侧只能观测前 ~1.4 s，见 monitor-log.txt） |
