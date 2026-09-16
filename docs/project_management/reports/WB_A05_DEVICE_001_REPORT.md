# WB_A05_DEVICE_001_REPORT — M0 唯一候选实机 Bring-up 与基础 Smoke Test

**任务**：`WB-A05-DEVICE-001`
**前置**：`WB-A05-BUILD-001`（CP0～CP4 全部 ACCEPTED，A05-BUILD CLOSED）
**完成时间**：2026-09-16 15:05 (+08:00)
**结论**：**D0 PASS / D1 PASS / D2 PASS（含 3 项明确限制，见 §16）**

**脱敏声明（任务书 §34）**：本报告**不含** Wi-Fi 密码、token、`device_secret`、MQTT 凭据、完整 NVS dump。
含凭据的原始产物（`_postconfig-nvs.bin`、`_now2-nvs.bin`、`_d2-nvs-minimal.txt`、`_d2-nvs-diff.txt` 等）
**仅本地归档并已 gitignore**，本报告只记录文件名 / offset / length / SHA256。

---

## 1. Candidate identity

| 项 | 值 |
|---|---|
| Candidate | `claw4-v53-m0-a05-2c8f58f` |
| 权威产品源码 revision | `2c8f58f53506402918284c695100f007798433d8` |
| Candidate Freeze commit | `420d0df1431536a042966617c2ff317cfbbc2ebd` |
| application image | `xiaozhi.bin` |
| 冻结 size | `9,271,760 B` |
| 冻结 SHA256 | `c035e1c09ebe472f5f14b490c93844aa3e298cd11b4e30f7fe1055e5b9278d36` |
| 刷前重算 | size 一致 / SHA256 一致 ⇒ `FLASH_INPUT_HASH = MATCH` |
| 刷后回读（0x200000 全量） | SHA256 一致 |
| **联网后第 3 次回读（14:40）** | SHA256 **仍一致** ⇒ 未发生上游自动 OTA 替换 |

## 2. Device identity

| 项 | 值 |
|---|---|
| 端口 | `COM7`（USB-Serial/JTAG，VID:303A / Espressif） |
| 芯片 | **ESP32-P4 (rev v1.3)**，PSRAM 32MB / 200MHz，flash 32MB |
| Flash 器件 | GigaDevice，`c8/4019`，32 MB |
| MAC | `80:f1:b2:d2:ed:14` |
| 备用串口 | `COM6`（CH340/UART0 桥） |
| 与预期平台一致性 | **符合** Claw4 / ESP32-P4 预期 ⇒ 无 HARD STOP |

## 3. Preflight（D0）

| Gate | 结果 |
|---|---|
| DEVICE_IDENTITY | PASS |
| FLASH_SIZE | PASS（32 MB ≥ 冻结布局末尾 `0x01FA6000`，尾部余量 368,640 B） |
| BACKUP | PASS（8 区域 rc=0） |
| PARTITION_TABLE | PASS（实机 13 项 == 冻结 `partitions/v1/32m_dual.csv`） |
| OTA_SELECTION | PASS（otadata[0] `ota_seq=1` / `ota_state=0x2 = ESP_OTA_IMG_VALID` ⇒ boot_index `(1-1)%2=0` ⇒ **ota_0**） |
| RECOVERY_PATH | CONFIRMED（ROM download mode 可达；备用 UART0/CH340） |
| CANDIDATE HASH | MATCH |

关键事实：实机分区表位于 **0x9000**（`CONFIG_PARTITION_TABLE_OFFSET=0x9000`），
候选构建的 `partition-table.bin` 与实机 @0x9000 **逐字节相同**（`prefix_equal=True`）；
`flasher_args.json` 的偏移（parttable 0x9000 / app 0x200000 / otadata 0x10e000）与实机完全吻合。

## 4. Pre-flash backups

`docs/project_management/evidence/a05-device/backups/`（**仅本地**，gitignore），清单见 `preflash-backup-manifest.txt`：

| 文件 | offset | length |
|---|---|---|
| backup-0x000000-0x010000-bootloader-and-parttable.bin | 0x000000 | 0x010000 |
| backup-0x00a000-0x032000-nvsfactory.bin | 0x00A000 | 0x032000 |
| backup-0x03c000-0x0d2000-nvs.bin | 0x03C000 | 0x0D2000 |
| backup-0x10e000-0x002000-otadata.bin | 0x10E000 | 0x002000 |
| backup-0x110000-0x001000-phy_init.bin | 0x110000 | 0x001000 |
| backup-0x111000-0x0ef000-model-srmodels.bin | 0x111000 | 0x0EF000 |
| backup-0x200000-0x900000-ota_0-current-app.bin | 0x200000 | 0x900000 |
| backup-0x0b00000-0x400000-ota_1-current-app.bin | 0x0B00000 | 0x400000 |

全部 `rc=0`，逐个记录 SHA256 ⇒ 具备完整回滚条件。

## 5. Real partition comparison

实机 @0x9000 解码 13 项，与冻结 `32m_dual.csv` **逐项一致**；重点项：

```
ota_0  offset 0x00200000  size 9,437,184  (>= 9,271,760  ✓)
ota_1  offset 0x00b00000  size 4,194,304
nvs / otadata / phy_init / model / storage / resources / system 均一致
```
⇒ `DEVICE_PARTITION_GATE = PASS`。

## 6. OTA state

```
otadata @0x10E000  sha256 8ba3b110139f45443d4f268d1a3373ef99a1718b71d51664531b83ee2d4b91a3
sector0: 01 00 00 00 | ota_state 0x00000002 (VALID) | crc 0x474398 9A
⇒ boot_index = (ota_seq - 1) % 2 = 0  ⇒  ota_0
```
D0 判定后**在本轮所有写入与联网之后复核仍然未变** ⇒ 启动槽位稳定为 ota_0，未被固定指向 ota_1。

## 7. Flash command / result

```
tool      esptool.py v4.12.0
port      COM7  (USB-Serial/JTAG)
before    default_reset      after   hard_reset
command   write_flash --flash_mode dio --flash_size 32MB --flash_freq 40m
          0x200000 E:\a05c\b\xiaozhi.bin
result    2,265,280 bytes compressed -> 9,271,760 bytes written
          Hash of data verified.   (verify PASS)
range     0x200000 – 0x0AD7FFF  ⇒ 未溢出 ota_0 (0x200000–0xADFFFF)
scope     最小写入：仅 ota_0。bootloader / partition table / NVS / storage /
          resources / otadata 一律未写（任务书 §15/§16 遵守）
post-read 0x200000 全量回读 9,271,760 B 的 SHA256 == 冻结值
```
> 一次尝试因串口瞬时 `PermissionError(13)/ERROR_GEN_FAILURE` 中断（端口恢复后重试成功），
> 该中断**发生在写入与 verify 之前**，未产生半写状态；重试后的完整 `Hash of data verified.` 证明镜像完整。
> 全程未使用 `erase_flash` / full chip erase。

## 8. Boot log summary

**结构**：ROM -> 二级引导 -> 应用前段，每一次复位都可稳定复现；`main_task: Calling app_main()` 为可观测到的最后一行。

```
ESP-ROM:esp32p4-eco2-20240710 / Build:Jul 10 2024
rst:0x17 (CHIP_USB_UART_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
SPI mode:DIO, clock div:2  ->  entry 0x4ff29ed0
I (650) mmu_psram: .rodata xip on psram
I (1004) app_init: App version:      2.0.51
I (1004) app_init: Compile time:     Sep 15 2026 17:59:12
I (1005) app_init: ELF file SHA256:  fba68d580....     <- == 冻结 xiaozhi.elf
I (1005) efuse_init: Chip rev:       v1.3
I (1008) esp_psram: Adding pool of 23808K of PSRAM memory to heap allocator
I (1009) spi_flash: detected chip: gd      flash io: qio
I (1030) H_SDIO_DRV: sdio_data_to_rx_buf_task started
I (1031) main_task: Calling app_main()
I (1447) METALIO_CLAW_4: LCD hardware reset done (GPIO 3).   <- 应用自身首条日志
```

**身份核对**：Project `xiaozhi` / App version `2.0.51` / Compile time `Sep 15 2026 17:59:12` /
ELF SHA256 `fba68d580…` —— 与冻结 `xiaozhi.elf` 全部 MATCH ⇒ **跑的就是冻结候选**。

**⚠️ 观测限制（如实记录，不是应用缺陷）**：本板唯一控制台是 USB-Serial/JTAG（`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`、
`CONFIG_ESP_CONSOLE_UART_NUM=-1`、`CONFIG_ESP_CONSOLE_SECONDARY_NONE=y`）。实测每次复位后约 1.1 s 有一次
USB 重枚举毛刺（无干扰轮询：0.04 s PRESENT → 1.08 s ABSENT → 1.34 s PRESENT，之后 8.6 s 稳定），
主机句柄失效；而**重新打开串口自身又会复位芯片**，形成自持循环 ⇒ **主机侧只能观测每次运行的前 ~1.4 s**，
`app_main()` 之后的 Wi-Fi/LVGL/音频/Learning 初始化行本轮**未取得**。
bootloader 日志本就无输出（`CONFIG_BOOTLOADER_LOG_LEVEL_NONE=y`），启动槽位由 §6 的 otadata 静态推理承担。
**补偿**：改用 NVS 侧信道观测（§12/§13/§14），以设备自身写 flash 的副作用证明应用健康与联网。

## 9. Fatal scan

扫描 `A05_DEVICE_BOOT_001–005.log` + `*.rawlog` + `_steady-attach.log` + `_nonreset-attach.log` + `_liveattach.log`
（关键字：panic / abort / assert / watchdog / WDT / stack overflow / heap corruption / Guru Meditation /
LoadProhibited / StoreProhibited / IllegalInstruction / brownout / reset reason / rst:0x / Backtrace / Fatal / `E (`）：

```
total_keyword_hits = 46
  46/46 = rst:0x17 (CHIP_USB_UART_RESET)   <- 全部由主机打开串口触发
  panic / Guru / LoadProhibited / StoreProhibited / IllegalInstruction = 0
  stack overflow / heap corruption / assert / brownout / WDT = 0
```
⇒ `MONITOR_FATAL_SCAN = PASS`（无应用级致命错误，无 reboot loop）。

## 10. UI smoke

现场观察（用户确认）：屏幕正常点亮并显示应用界面、画面稳定、触摸正常、**可进入 Learning Screen、任务列表可见且正常、页面可切换**。
⇒ `UI_SMOKE = PASS`（`DEVICE_UI_LATENCY` 仍 `DEVICE_VERIFY_REQUIRED`）。

## 11. Audio / voice smoke

- 通过：上电/复位可听到**开机提示音** ⇒ 音频链路（audio_service / codec / 扬声器）在工作，无死锁、无连续重启。
- 未取得：**云语音闭环**（MQTT 8883 / WebSocket 真实会话）本轮未验证成功——用户侧出现"离线"类提示。
  源码定性见 §12 附注：设备上两处"离线"判定均**不表示公网掉线**（一处是学习后端可达性，一处是 `!time_synced`，M0 未接入时间同步）。
  上游 `api.tenclass.net:8883` 从公网可达（主机侧探测 True），故**不得**据此判定候选固件缺陷。
- `20-round audio stability` 保持 `DEVICE_VERIFY_REQUIRED`。

## 12. Network smoke（本任务重点，详见 `evidence/a05-device/network-smoke.txt`）

| 子项 | 结果 | 证据 |
|---|---|---|
| Wi-Fi 凭据写入 | PASS | IDF 官方 `nvs_tool.py -d minimal`：`wifi:ssid` / `wifi:ssid1` / `wifi:password1` 存在（明文脱敏）；`-i` 无 error |
| 掉电保持 | PASS | 拔 USB → 5 s → 插回后条目仍在，应用直接用其联网 |
| **基础 TCP/HTTP（HTTPS）通信** | **PASS** | **至少两次完整 OTA 往返**：`mqtt:username/password` 由 OTA 响应写入 NVS（该键只能由 `ota.cc:176-191` 解析响应产生）；14:35 与 14:55 两次读出值不同（服务端观察到的客户端 IP 字段变化） ⇒ DNS + TLS + HTTP + JSON + 落盘闭环 |
| 联网状态 | PASS | 用户现场确认屏幕 **已在线 / 已激活**（无激活码待绑定） |
| 上游端点可达性 | PASS | `api.tenclass.net -> 47.112.110.152`；443 / 80 / 8883 通，1883 不通（固件默认 8883） |
| **Learning backend 基础链** | **NOT_EXERCISED** | 设备 `learning_cfg.base_url = http://192.168.3.26:18765`（局域网地址）；用户确认本次 Wi-Fi **不是** 192.168.3.x；主机侧同不可达（TCP/ICMP False）。改配置需写 NVS，任务书 §1.2/§15 禁止 ⇒ 本轮不改 |
| latency（connect/request/disconnect） | 未采集 | 受 USB 控制台 ~1.4 s 观测窗口限制，如实记录 |
| NETWORK_TOTAL_DEADLINE / TCP real loopback | **不得**宣称 PASS | 保持 `DEVICE_VERIFY_REQUIRED`（已知遗留：`EspTcp::Connect()` 不继承 HTTP total timeout） |

**联网后的候选完整性复核（防上游自动 OTA 覆盖）**：候选 `application.cc:179` 在 `ota.HasNewVersion()` 时会
`UpgradeFirmware()`。设备已联网 ⇒ 已复核：otadata **未变**、`ota_0` 全量 sha256 **== 冻结值**、`ota_1` 头 64 B **未变**
⇒ 本轮上游未下发更高版本，**冻结候选未被替换**（该风险已写入 §16）。

## 13. Learning runtime smoke

真实 runtime chain（`LearningScreen → LearningRuntime → LearningBackendSession → SyncExecutor → SingleFlightHttpTransport`）：

| 子项 | 结果 | 证据 |
|---|---|---|
| LearningScreen 能进入 | PASS | 现场观察（用户确认） |
| LearningRuntime 初始化 | PASS | 任务列表正常渲染；`learning:*` 状态从 NVS 解出（`NvsOutboxStorage` 走 `loadWithPresence`） |
| BackendSession 不导致崩溃 | PASS | fatal-scan 0 命中；连续运行期间无 panic/WDT；后台 worker 每 15 s 跑一次 `RunOnlineCycle()`（`learning_runtime.cpp:131-152`）而系统稳定 |
| **基础同步调用可触发** | **PASS（旁证）** | 配网后窗口内 outbox **新增 4 个事件 id + 1 个新会话 id（`sess-3c6b94c087314621`）**、**0 移除** ⇒ 事件工厂/session 工厂/持久化链在真机持续工作；removed=0 ⇒ 同步未成功（后端不可达），**失败优雅**：事件保留、无丢失 |
| HTTP transport 不造成系统异常 | PASS | 对不可达后端反复尝试期间无 panic/assert/WDT/内存错误 |

## 14. NVS smoke（详见 `evidence/a05-device/nvs-persistence.txt`）

```
nvs_tool.py -i（4 个快照）       无 error / 无 warning
多次复位 NVS sha256 恒定           PASS（无损坏、无 torn write）
拔插掉电后配网凭据与学习状态保持    PASS
学习 outbox 事件增量 +4 / 会话 +1   PASS
nvs_tool 解析出的命名空间           board / network / wifi / display / mqtt / audio /
                                  websocket / learning（含 outbox blob）均正常
```
⇒ `NVS_REGRESSION_SMOKE = PASS（基础回归）`；强持久化压力（写满/GC 极限、掉电打断写入）保持 `DEVICE_VERIFY_REQUIRED`。

## 15. Stability smoke

- 连续软复位 3 次（专用脚本）+ 累计 ≥10 次复位：全部正常启动，无 reboot loop。
- **1 次真实掉电上电**（拔 USB ≥5 s 再插）：冷启动正常，配网凭据与学习状态保持。
- 用户侧若干轮 UI 操作 + 进入/退出 Learning 页 + 一次学习会话：无崩溃、无卡死、无 panic。
⇒ `REBOOT_STABILITY = PASS`；`20-round audio stability` 保持 `DEVICE_VERIFY_REQUIRED`。

## 16. Remaining DEVICE_VERIFY_REQUIRED / NOT_IN_SCOPE

| 项 | 状态 | 说明 |
|---|---|---|
| `app_main()` 之后的完整串口日志 | UNOBTAINABLE_THIS_ROUND | USB-Serial/JTAG 在应用初始化 USB PHY 时重枚举；须换用板载 UART0 控制台或把 `ESP_CONSOLE` 改为 UART（均属改 sdkconfig，本轮禁止） |
| Learning backend 真实同步往返 | NOT_EXERCISED | 后端为局域网地址 `192.168.3.26:18765`，不在设备网段；待 C01 前把设备接入同网段或正式后端 |
| 云语音闭环（MQTT/WS 会话） | DEVICE_VERIFY_REQUIRED | 本轮未取得有效会话 |
| 20-round audio stability | DEVICE_VERIFY_REQUIRED | 任务书 §21/§26 明确不在本轮 |
| DEVICE_UI_LATENCY | DEVICE_VERIFY_REQUIRED | 任务书 §20 明确不在本轮 |
| NETWORK_TOTAL_DEADLINE / TCP real loopback / 各段 latency | DEVICE_VERIFY_REQUIRED | 任务书 §22/§29 明确不得宣称 |
| **上游自动 OTA 风险** | RISK_RECORDED | `ota_url = https://api.tenclass.net/xiaozhi/ota/`；联网状态下若上游下发更高版本，候选会被 OTA 替换。本轮已复核未被替换；后续任何联网测试前后都必须复核 otadata + ota_0 sha256 |
| Dual-slot OTA | UNAVAILABLE | app 9,271,760 B > ota_1 4,194,304 B（任务书 §3 结论，本轮不解决） |
| ReminderCore / TimeAuthority / InteractionArbiter 主动提醒 | NOT IN SCOPE（C03） | 本候选 GC_DISCARDED + COMPILED_NOT_WIRED，**不触发是预期**，不得判 FAIL |

## 17. Scope deviations

| 项 | 说明 |
|---|---|
| 是否修改产品源码 / CMake / sdkconfig / partition CSV / managed_components / Manifest | **全部 NONE** |
| 是否重新 build | **NONE**（未生成新 xiaozhi.bin） |
| 是否整片擦除 | **否**（仅写 ota_0 0x200000 起的 app 镜像） |
| 是否写 NVS / bootloader / partition table / otadata | **否**（写 NVS 的只有设备固件自身：配网与学习 outbox） |
| 是否修改 NVS 里的 `learning_cfg`（让学习后端可达） | **否**（§1.2/§15 禁止；如实记为 NOT_EXERCISED） |
| 额外做的（不越界、只读或任务书要求范围内） | ① 增加了 **NVS 侧信道取证**（IDF 官方 `nvs_tool.py` + 自写 `nvs_diff.py` / `nvs_event_delta.py`），用于补偿串口观测窗口限制；② 增加了 **联网后候选完整性复核**（§12 末），用于排除上游自动 OTA 覆盖；③ 拔插掉电测试（§25 建议项）由用户执行 |
| 未按建议执行 | 未采集 connect/request/disconnect latency（主观测通道受限） |

## 18. Final result

```
A05-DEVICE REVIEW_READY

Candidate:
claw4-v53-m0-a05-2c8f58f

Candidate SHA256:
c035e1c09ebe472f5f14b490c93844aa3e298cd11b4e30f7fe1055e5b9278d36

D0 PREFLIGHT:
PASS

D1 FLASH:
PASS

D2 BRING-UP:
PASS  (含 3 项如实限制：app_main() 之后串口日志不可得 / Learning backend NOT_EXERCISED / 云语音闭环未验证)

Boot:
PASS

UI:
PASS

Audio/Voice:
PASS(基础) / 云语音闭环 DEVICE_VERIFY_REQUIRED

Network:
PASS(Wi-Fi + HTTPS OTA 往返 + 在线) / Learning backend NOT_EXERCISED

Learning Runtime:
PASS(运行时级：新增 4 事件 + 1 会话并落盘)

NVS Regression:
PASS

Reminder:
NOT IN SCOPE / C03

Source Changes:
NONE

Rebuild:
NONE

Flash Scope:
ota_0 ONLY @0x200000

Device Still Runs Frozen Candidate (post-network):
VERIFIED (otadata unchanged / ota_0 sha256 == frozen / ota_1 head unchanged)
```

**Evidence 目录**：`docs/project_management/evidence/a05-device/`
（文本证据：`D0-PREFLIGHT-GATE.md`、`device-info.txt`、`flash-id.txt`、`candidate-sha256.txt`、
`partition-decoded.txt`、`otadata-decoded.txt`、`recovery-path.txt`、`preflash-backup-manifest.txt`、
`flash-log.txt`、`monitor-log.txt`、`fatal-scan.txt`、`network-smoke.txt`、`nvs-persistence.txt`、
`device-smoke-matrix.md`；含凭据二进制/解析产物 **仅本地** 且已 gitignore）
