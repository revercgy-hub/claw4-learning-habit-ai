# Claw4 平台实现映射 (WB-001)

- 任务: WB-001 CLAW4_PLATFORM_MAP
- 分支: `workbuddy/wb-001-platform-map`
- 日期: 2026-09-01
- 依据: 官方源码快照 `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950` + 已验收的主机基线 (BLD-001) + 只读核查
- **重要声明: 本映射是"源码/配置证据映射"，不是实机事实。所有未连接实机验证的项统一标记 `DEVICE_VERIFY_REQUIRED`。**

## 1. 结论摘要

| 项 | 结论 |
| --- | --- |
| 主控 SoC | ESP32-P4 (RISC-V, 双核, 默认 360 MHz) — `CONFIG_CONFIRMED` (sdkconfig) |
| 网络协处理器 | ESP32-C5 经 ESP-Hosted SDIO 4-bit 40MHz — `CONFIG_CONFIRMED` (sdkconfig) |
| PSRAM | 已启用, HEX 模式, 200 MHz — `CONFIG_CONFIRMED` |
| Flash | 32 MB, 40 MHz — `CONFIG_CONFIRMED` (注意 Flash mode 不一致风险) |
| 分区表 | `partitions/v1/32m_dual.csv`, 非对称 OTA (ota_0=9M / ota_1=4M) — `CONFIG_CONFIRMED` |
| 双系统 | ota_0 = xingzhi 主固件; ota_1 = ESPClaw edge_agent — `SOURCE_CONFIRMED` (esp_claw_bin/README.md) |
| 显示 | 720×720, 2-lane MIPI-DSI, RGB565 16bpp; NV3051F 默认 / FL7707N 可选 — `SOURCE_CONFIRMED`, 实机 `DEVICE_VERIFY_REQUIRED` |
| 触摸 | GT911, I2C (SDA=7, SCL=8), INT=GPIO33 — `SOURCE_CONFIRMED`, 实机 `DEVICE_VERIFY_REQUIRED` |
| 音频 | 16kHz, BTAudioCodecDuplex (I2S), esp-sr 唤醒词 — `SOURCE_CONFIRMED`, 实机 `DEVICE_VERIFY_REQUIRED` |
| 摄像头 | OV2710 (MIPI CSI, 1080p@25fps 配置) — `CONFIG_CONFIRMED`, 实机 `DEVICE_VERIFY_REQUIRED` |
| SD | SDMMC 4-bit (slot 0), 热插拔未确认 — `SOURCE_CONFIRMED`, 实机 `DEVICE_VERIFY_REQUIRED` |
| 电源 | BQ27220 电量计 + 充电设置 (ichg_ma), SY6970/ADC 备选 — `SOURCE_CONFIRMED`, 实机 `DEVICE_VERIFY_REQUIRED` |
| OTA | esp_ota 双槽 (跳过 factory; 用 next_update_partition) — `SOURCE_CONFIRMED`, 实机 `DEVICE_VERIFY_REQUIRED` |

## 2. 版本与配置基线

| 项目 | 值 | 状态 |
| --- | --- | --- |
| 官方源码 Commit | `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950` (main, tag latest) | SOURCE_CONFIRMED |
| 项目版本 | `2.0.51` (CMakeLists `PROJECT_VER`) | SOURCE_CONFIRMED |
| ESP-IDF | v5.5.4 (E:\i 镜像实测 `ESP-IDF v5.5.4`) | CONFIG_CONFIRMED |
| 工具链 | riscv32-esp-elf-gcc 14.2.0 (esp-14.2.0_20260121) | CONFIG_CONFIRMED |
| IDF Python | 3.12.13 (claw4-idf-tools) | CONFIG_CONFIRMED |
| 构建目标 | `esp32p4` | CONFIG_CONFIRMED |
| LVGL | `lvgl__lvgl` 9.3.0 (lv_version.h) | SOURCE_CONFIRMED |
| esp_lvgl_port | 2.6.3 (idf_component.yml) | SOURCE_CONFIRMED |
| esp_lvgl_adapter | ^0.5.0 (声明) | SOURCE_CONFIRMED |
| 分区表 | `partitions/v1/32m_dual.csv`, offset 0x9000 | CONFIG_CONFIRMED |
| 基线构建 | `E:\b\xiaozhi.bin` 9,036,192 B (≈8.62 MiB) 编译链接通过 | CONFIG_CONFIRMED |

## 3. 子系统映射总表

| # | 子系统 | 状态 | 关键配置/实现 | 主要文件 |
| --- | --- | --- | --- | --- |
| 1 | ESP-IDF/Target/CPU | CONFIG_CONFIRMED | esp32p4, RISC-V, 双核 360MHz, P4 rev_min 0 | sdkconfig |
| 2 | RAM/PSRAM/Flash | CONFIG_CONFIRMED | PSRAM HEX 200MHz; Flash 32MB 40MHz; mode 不一致待实机 | sdkconfig |
| 3 | Partition/OTA 分区 | CONFIG_CONFIRMED | 32m_dual; ota_0=9M, ota_1=4M; 非对称 | partitions/v1/32m_dual.csv |
| 4 | LVGL/Display | SOURCE_CONFIRMED | LVGL 9.3.0; 720×720 MIPI-DSI 2-lane; NV3051F/FL7707N | main/display/, boards/metalio-claw-4/ |
| 5 | Touch | SOURCE_CONFIRMED | GT911 I2C; INT=GPIO33; touch_feed ISR | boards/.../metalio-claw-4.cc, touch_feed.cc |
| 6 | C5/ESP-Hosted/Wi-Fi | CONFIG_CONFIRMED | ESP-Hosted SDIO slot 1, 4-bit 40MHz, CP=esp32c5 | sdkconfig, managed_components/esp_hosted |
| 7 | Audio | SOURCE_CONFIRMED | 16kHz; BTAudioCodecDuplex; esp-sr; AEC 默认关 | main/audio/, boards/common/bt_audio_codec.* |
| 8 | Camera | CONFIG_CONFIRMED | OV2710 MIPI CSI 1080p25fps (配置); camera_screen 存在 | sdkconfig, boards/common/esp32_camera.* |
| 9 | SD/Storage | SOURCE_CONFIRMED | SDMMC 4-bit slot 0; 热插拔未确认; FAT/SPIFFS 分区 | boards/.../config.h, SdCardManager.hpp |
| 10 | Power/Battery | SOURCE_CONFIRMED | BQ27220 电量计; ichg_ma 充电设置; boot 低电检查 | bq27220_gauge.*, metalio-claw-4.cc |
| 11 | OTA | SOURCE_CONFIRMED | esp_ota 双槽; 跳过 factory; 无签名/回滚策略确认 | main/ota.cc |
| 12 | 启动日志验证字段 | DEVICE_VERIFY_REQUIRED | 见 §7 采集清单 | system_info.*, 实机日志 |

## 4. 各子系统实现路径与证据

### 4.1 ESP-IDF 版本、Target、P4 Revision 与 CPU 配置

**状态: CONFIG_CONFIRMED**

- `sdkconfig:573-577` — `CONFIG_IDF_TARGET_ARCH_RISCV=y`, `CONFIG_IDF_TARGET="esp32p4"`, `CONFIG_IDF_TARGET_ESP32P4=y`
- `sdkconfig:1675-1676` — `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_360=y`, `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=360`
- `sdkconfig:1421-1424` — `CONFIG_ESP32P4_REV_MIN_0=y` (rev 最小 0, 最大 199 → 未锁定具体 revision)
- IDF v5.5.4 实测确认 (`idf.py --version` → `ESP-IDF v5.5.4`)

> ⚠️ 未实机验证: 实际硅片 revision、实际可用 CPU 频率。**不硬编码 480MHz** (项目总规 §十五)。

### 4.2 Internal RAM、PSRAM、Flash

**状态: CONFIG_CONFIRMED (配置); 容量/实际性能 DEVICE_VERIFY_REQUIRED**

- PSRAM: `sdkconfig:1622-1647` — `CONFIG_SPIRAM=y`, `MODE_HEX=y`, `SPEED_200M=y`, `USE_MALLOC=y`, `MALLOC_ALWAYSINTERNAL=4096`, `TRY_ALLOCATE_WIFI_LWIP=y`, `MALLOC_RESERVE_INTERNAL=65536`
  - XIP 特性: `SPIRAM_FETCH_INSTRUCTIONS=y`, `SPIRAM_RODATA=y`, `SPIRAM_XIP_FROM_PSRAM=y`
- Flash: `sdkconfig:746-749` — `CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y`, `FLASHSIZE="32MB"`; `739` `FLASHFREQ_40M=y` / `FLASHFREQ="40m"`
- ⚠️ **Flash mode 不一致 (风险, 只记录不修改)**: `sdkconfig:731` `CONFIG_ESPTOOLPY_FLASHMODE_QIO=y` 但 `:736` `CONFIG_ESPTOOLPY_FLASHMODE="dio"`。看板已知风险, 真机 Flash 异常时为首个排查点。
- Internal RAM 总量无 sdkconfig 直接项 (P4 内建), 需实机 `heap_caps_get_total_size(MALLOC_CAP_INTERNAL)` 确认。

### 4.3 Partition Table / ota_0 / ota_1 / ESPClaw

**状态: CONFIG_CONFIRMED**

`partitions/v1/32m_dual.csv` 全文 (offset/size):

| Name | Type | SubType | Offset | Size | 用途 |
| --- | --- | --- | --- | --- | --- |
| nvsfactory | data | nvs | 0xA000 | 200K | 出厂 NVS |
| nvs | data | nvs | — | 840K | 应用 NVS |
| otadata | data | ota | — | 8K | OTA 状态 |
| phy_init | data | phy | — | 4K | PHY 校准 |
| model | data | spiffs | — | 956K | 模型 |
| **ota_0** | app | ota_0 | 0x200000 | **9M** | xingzhi 主固件槽 |
| **ota_1** | app | ota_1 | — | **4M** | ESPClaw edge_agent 槽 |
| resources | data | spiffs | — | 4M | 资源 |
| factory_test | data | spiffs | — | 600K | 工厂测试 |
| emote | data | spiffs | — | 4M | 表情资源 |
| system | data | fat | — | 1M | 系统数据 |
| storage | data | fat | — | 7M | 用户存储 |
| coredump | data | coredump | — | 64K | 崩溃转储 |

- `sdkconfig:767-771` — `CONFIG_PARTITION_TABLE_CUSTOM=y`, `CUSTOM_FILENAME="partitions/v1/32m_dual.csv"`, `OFFSET=0x9000`
- **双系统证据** (`esp_claw_bin/README.md`): `edge_agent_0xb00000.bin` → `ota_1` (ESPClaw 应用); 主固件 xingzhi 烧到 `ota_0`; 附带 `emote_assets_0x1396000` / `system_0x1796000` / `storage_0x1896000` 数据分区镜像。
- ⚠️ **非对称 OTA**: ota_0=9M / ota_1=4M, 不是通用 A/B 双槽。`xiaozhi.bin` ≈8.62 MiB 放得进 ota_0, **放不进 ota_1** (溢出 0x49e1a0, BUILD.md 记录)。
- ⚠️ 真机到货前**禁止修改 partition table** (AGENTS.md 硬门禁)。

### 4.4 LVGL / Display / 屏驱 SKU

**状态: SOURCE_CONFIRMED (实现); 实机 SKU DEVICE_VERIFY_REQUIRED**

- LVGL: `managed_components/lvgl__lvgl/lv_version.h` → **9.3.0**; `esp_lvgl_port` 2.6.3; `esp_lvgl_adapter ^0.5.0` (main/idf_component.yml)
- 分辨率/总线: `boards/metalio-claw-4/config.h` — `DISPLAY_WIDTH/HEIGHT 720`, `LCD_BIT_PER_PIXEL 16`, `LCD_MIPI_DSI_LANE_NUM 2`, `MIPI_DSI_PHY_PWR_LDO_CHAN 3 / 2500mV`, `DISPLAY_BACKLIGHT_PIN GPIO52`
- 屏驱切换宏: `metalio-claw-4.cc:24-37` — `METALIO_CLAW_4_USE_FL7707N` 默认 0 → **NV3051F** (36MHz DPI, RGB888, 24bpp); 置 1 → **FL7707N** (48MHz DPI, 16bpp)
- 实现文件: `boards/metalio-claw-4/esp_lcd_nv3051f.c/h`, `esp_lcd_fl7707n.c/h`; `main/display/` (lcd_display, lvgl_display, emote_display, oled_display 等)
- LVGL draw buffer: `lcd_display.cc` 中 `buffer_size = width*height*50` 且 `double_buffer=true` 的配置存在 (约 720*720*50≈ 25.9MB 需 PSRAM 支持) — 精确取值需进一步读码确认, 实机为最终依据。

### 4.5 Touch

**状态: SOURCE_CONFIRMED (实现); 实机 DEVICE_VERIFY_REQUIRED**

- `config.h:55-56` — `TOUCH_INT_PIN GPIO_NUM_33` (GT911 INT 低有效)
- `config.h:33-34` — I2C: SDA=GPIO7, SCL=GPIO8 (GT911 与 TCA9555 IO 扩展同总线; metalio-claw-4.cc:80 有总线抢用注释)
- 实现: `esp_lcd_touch_gt911` (managed); `main/display/touch_feed.cc` — ISR 注册 (`TouchIrqCb`), I2C 读取失败退避逻辑
- 屏上工厂测试: `main/display/screen/test_screen/touch_panel_test.cc`

### 4.6 ESP32-C5 / ESP-Hosted / Wi-Fi

**状态: CONFIG_CONFIRMED**

- `sdkconfig:3041` `CONFIG_ESP_HOSTED_ENABLED=y`; `:3046` `CONFIG_ESP_HOSTED_CP_TARGET_ESP32C5=y`; `:3054` `IDF_SLAVE_TARGET="esp32c5"`
- 传输: `:3062` `CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y` (SPI/UART 均 not set); `:3075-3076` `SDIO_SLOT_1=y`, `SDIO_SLOT=1`; `:3073` `SDIO_OPTIMIZATION_RX_STREAMING_MODE=y`
- 组件: `managed_components/espressif__esp_hosted`
- ⚠️ **不是 P4 原生 Wi-Fi**: Wi-Fi 能力全部来自 C5 协处理器 (AUD-001 已确认, 不得写成 P4 原生)。
- ⚠️ 双网络板: `class METALIO_CLAW_4 : public DualNetworkBoard` (metalio-claw-4.cc:127), DualNetworkBoard 支持 WiFi(C5)/4G(ML307/NT26) 切换, 默认 `ML307`(network_type 默认) — 实机实际网络路径需启动日志确认。

### 4.7 Audio / Codec / Mic / SPK / AEC/VAD/Wake Word

**状态: SOURCE_CONFIRMED (实现); 实机 DEVICE_VERIFY_REQUIRED**

- 采样率: `config.h:6-7` — `AUDIO_INPUT_SAMPLE_RATE 16000`, `AUDIO_OUTPUT_SAMPLE_RATE 16000`
- 接口: `config.h:16-19` — I2S: MIC WS=GPIO10, DIN=GPIO11; SPK DOUT=GPIO9, BCLK=GPIO12
- Codec: `metalio-claw-4.cc:768-772` — `GetAudioCodec()` 返回 `BTAudioCodecDuplex` (boards/common/bt_audio_codec.cc/h), 16kHz 全双工
- 组件: `main/audio/` (audio_codec, audio_service, processors/, codecs/ 含 es8311/8374/8388/8389/box/dummy/no); `esp-sr` (managed, 唤醒词/VAD/AEC 基础)
- ⚠️ AEC: `application.cc:55` 注释明确 "打断(AEC)默认关; 开机后由 NVS 恢复" — **AEC 默认关闭**。
- ⚠️ 麦克风/扬声器/Codec 具体型号: HARDWARE_ASSUMPTIONS 台账 = UNKNOWN, 需实机。

### 4.8 Camera

**状态: CONFIG_CONFIRMED (配置); 实机 DEVICE_VERIFY_REQUIRED**

- `sdkconfig:2937-2956` — `CONFIG_CAMERA_OV2710=y`; `OV2710_MIPI_RAW10_1920X1080_25FPS=y`; `OV2710_MIPI_DEFAULT_FMT_RAW10_1920X1080_25FPS=y`
- 组件: `managed_components/espressif__esp_cam_sensor`; 实现 `boards/common/esp32_camera.cc/h`, `boards/common/camera.h`
- UI: `main/display/screen/camera_screen/`; 工厂测试 `camera_test.cc`
- ⚠️ 传感器实际型号/能力未实机确认 (台账 UNKNOWN); MVP 阶段摄像头默认关闭 (项目总规 §十六)。

### 4.9 SD / Storage / USB MSC

**状态: SOURCE_CONFIRMED (接口); 热插拔/容量 DEVICE_VERIFY_REQUIRED**

- `config.h:58-66` — SDMMC slot 0, 4-bit: CLK=GPIO43, CMD=GPIO44, D0-D3=39/40/41/42; `SDMMC_LDO_CHAN_ID 4`
- 管理: `boards/common/SdCardManager.hpp`; 分区: `storage` (FAT 7M), `system` (FAT 1M)
- USB: `config.h:68-70` — USB OTG FS PHY0: DM=GPIO24, DP=GPIO25; `usb_virtual_disk.cc` (虚拟 U 盘 MSC)
- ⚠️ 热插拔能力未确认 (台账 UNKNOWN)。SD 卡不作为 MVP 强依赖。

### 4.10 Power / Battery / Gauge

**状态: SOURCE_CONFIRMED (实现); 实机 DEVICE_VERIFY_REQUIRED**

- 电量计: `boards/common/bq27220_gauge.cc/h`; `metalio-claw-4.cc:606` 将 BQ27220 绑定到 i2c_bus
- 充电: `metalio-claw-4.cc:551-573` — `charge` settings `ichg_ma` (默认 1000); boot 低电检查 `Boot battery check: level=%d, charging=%s`
- 其他实现: `boards/common/` 下 `sy6970.cc` (充电 IC), `adc_battery_monitor.cc` (ADC 备选), `axp2101.cc` (PMIC 备选), `power_save_timer.cc`
- USB 充电状态: `config.h:9` `USB_CHG_STA_PIN GPIO_NUM_53`
- ⚠️ 电池容量/充电 IC 型号: 需实机 (台账 HARDWARE_VERIFY_REQUIRED), 不根据电商描述推断。

### 4.11 OTA / 恢复边界

**状态: SOURCE_CONFIRMED (实现); 实机 DEVICE_VERIFY_REQUIRED**

- `main/ota.cc`: `esp_ota_get_running_partition()`; 若 label=="factory" 则跳过 (`:277-279`); 否则 `esp_ota_get_next_update_partition(NULL)` 写升级
- OTA URL 默认: `https://api.tenclass.net/xiaozhi/ota/` (可被 settings `ota_url` 覆盖)
- ⚠️ 无签名 OTA / Secure Boot / Flash Encryption 证据 (项目总规 §二十二 为正式版规划, 当前 sdkconfig 未见使能配置)
- ⚠️ 非对称槽位 + xiaozhi.bin 超 ota_1 容量 → 双系统 OTA 能力存疑, 实机前不承诺、不改分区。

### 4.12 启动日志应采集的验证字段 (BRINGUP_STAGE1 输入)

**状态: DEVICE_VERIFY_REQUIRED** (采集项来自 BRINGUP_PROMPT B001/B002/B013 与 system_info.*)

见 §7 清单。

## 5. 可复用 / 需封装 / 禁止修改边界

### 5.1 可复用 (MVP 直接继承)

- 板级初始化、显示/触摸/LVGL 适配 (`main/display/`, `boards/metalio-claw-4/`)
- ESP-Hosted 网络栈 + WebSocket/MQTT 协议 (`main/protocols/`)
- 音频服务与 esp-sr 唤醒词 (`main/audio/`, esp-sr)
- 存储 (SdCardManager, FAT/SPIFFS 分区), 电量计 (Bq27220Gauge)
- OTA 双槽基础设施 (`main/ota.cc`)

### 5.2 需封装/新增 (MVP 业务层, 不直接动 BSP)

- `Task`/`StudySession`/计时/奖励领域模型 (独立于 LVGL 与 BSP, 可单测)
- 设备事件队列、离线缓存、重试、幂等同步 (Local Event Queue)
- 学习终端 Home/Focus/Done/Offline UI
- 面向家庭后端的认证 + HTTPS/WSS API 客户端

### 5.3 禁止修改 / 需用户授权 (硬门禁)

- `sdkconfig`、partition CSV、屏幕驱动选择/时序、PSRAM 参数、ESP-IDF 主版本、官方 BSP — 真机核验前禁止修改
- 刷写/擦除真机 Flash、覆盖出厂固件、改 partition table / OTA 策略 — 需用户明确授权 (AGENTS.md §1)
- 本任务 WB-001 只做证据映射, 未修改任何官方源码

## 6. DEVICE_VERIFY_REQUIRED 清单

| # | 项 | 验证方法 (真机) |
| --- | --- | --- |
| 1 | P4 硅片 revision / 实际 CPU 频率 | `esp_chip_info()` + 启动日志 |
| 2 | PSRAM 实际容量/频率/可用性 | heap_caps API + 启动日志 |
| 3 | Flash 实际容量 / mode 一致性 (QIO vs dio) | `esp_flash_get_size()` + 启动日志 + 稳定性 |
| 4 | 屏幕实际 SKU (NV3051F vs FL7707N) | 面板日志/显示测试 |
| 5 | 触摸实际控制器与坐标方向 | GT911 初始化 + 四角触摸 |
| 6 | C5 固件版本 / ESP-Hosted 状态 / Wi-Fi 能力 | Hosted 日志 + 扫描/连接 |
| 7 | 实际网络路径 (C5-WiFi vs 4G-ML307/NT26) | 启动日志 + 连接测试 |
| 8 | 音频硬件 (Mic/SPK/Codec 型号), AEC/VAD/Wake Word | 录音/播放/全双工测试 |
| 9 | 摄像头传感器型号与能力 | 传感器探测 + 分辨率测试 |
| 10 | SD 热插拔 / 吞吐 | 无卡/插卡/读写/断电测试 |
| 11 | 电池容量 / 充电 IC / SOC | BQ27220 读取 + 充放电测试 |
| 12 | 当前 boot partition / OTA 恢复路径 | boot log + 分区状态 |

## 7. Bring-up Stage 1 证据采集建议 (B001/B002/B003/B004/B005/B009/B013)

按 BRINGUP_PROMPT §二十五 首轮执行:

1. 完整启动日志: Firmware/Commit/Build/IDF/Board/SKU/P4(rev/cpu/cores)/C5(rev/fw)/Memory/Display/Touch/Audio/Camera/Storage/Network/Power/Partitions/FreeHeap
2. 芯片信息: `esp_chip_info()` 输出 (model, revision, cores, features)
3. 内存: `heap_caps_get_total_size/free_size/largest_free_block` for INTERNAL / SPIRAM / 8BIT, 多场景采样
4. 分区: `partitions/v1/32m_dual.csv` 与实机 `esp_partition_dump` / boot log 对照
5. 显示: 纯色/文本/触摸映射测试, 记录 FPS/tearing
6. 触摸: 四角+中心+滑动, P95 feedback <100ms 目标
7. 网络: Wi-Fi 扫描→连接→DHCP→HTTPS; 记录 RSSI/重连 (恢复<5s 目标)
8. 统一日志格式见 BRINGUP_PROMPT B013; 输出建议入 `docs/DEVICE_LOG_REFERENCE.md` (由 Stage 1 任务执行)

## 8. 风险与未决问题

| 风险 | 等级 | 说明 |
| --- | --- | --- |
| Flash mode 不一致 (QIO=y 但 "dio") | 高 | sdkconfig 证据冲突, 已记录不修改; 真机 Flash 异常时首个排查点 |
| 非对称 OTA + 固件超 ota_1 容量 | 高 | xiaozhi.bin 8.62MiB > ota_1 4M; 双槽 OTA 能力存疑, 实机前不承诺 |
| 屏幕 SKU 两选一 | 中 | NV3051F 默认, FL7707N 备选; 实机前不锁定 |
| AEC 默认关闭 | 中 | application.cc 注释确认; 语音交互需评估是否重开 |
| 网络路径默认 ML307(4G)? | 中 | DualNetworkBoard 默认 network_type=ML307; 实机实际路径待确认 |
| 电池/充电 IC/摄像头型号未确认 | 中 | 台账 UNKNOWN, 需实机 |
| P4 revision 未锁定 (rev_min 0) | 低 | sdkconfig 未锁定具体 revision; 实机读取为准 |
| 无签名 OTA 证据 | 中 | 正式版安全规划未在当前 sdkconfig 体现 |

## 9. 证据来源索引

- 官方源码: `vendor/MetalioClaw4` @ `ca3aa3fa`
- 分区表: `vendor/MetalioClaw4/partitions/v1/32m_dual.csv`
- 双系统: `vendor/MetalioClaw4/esp_claw_bin/README.md`
- 板卡: `vendor/MetalioClaw4/main/boards/metalio-claw-4/{config.h,config.json,metalio-claw-4.cc,esp_lcd_nv3051f.c,esp_lcd_fl7707n.c}`
- 显示/触摸: `vendor/MetalioClaw4/main/display/`
- 音频: `vendor/MetalioClaw4/main/audio/`, `boards/common/bt_audio_codec.*`
- 电源: `vendor/MetalioClaw4/main/boards/common/{bq27220_gauge,sy6970,adc_battery_monitor}.*`
- 网络: `vendor/MetalioClaw4/sdkconfig` (ESP_HOSTED), `boards/common/{dual_network_board,wifi_board,ml307_board,nt26_board}.*`
- OTA: `vendor/MetalioClaw4/main/ota.cc`
- 基线: `docs/BUILD.md`, `docs/CLAW4_主机准备情况报告_2026-09-01.md`, `docs/CLAW4_AUDIT.md`, `docs/HARDWARE_ASSUMPTIONS.md`
