# WB-001 WorkBuddy 实施报告

## 1. 元信息

- 任务 ID：WB-001 (CLAW4_PLATFORM_MAP)
- 结果：`REVIEW_READY`
- 分支：`workbuddy/wb-001-platform-map`
- 提交号：以任务分支 HEAD 为准（Git 元数据修订见 §6.7；首轮交付 `dbdc691`，父提交 `0f97af4` = Codex 基线）
- 执行时间：2026-09-01 15:20 (UTC+8)

## 2. 输入与范围

- 已读取输入（按任务包顺序）：
  1. `AGENTS.md`
  2. `docs/project_management/TASK_BOARD.md`
  3. `docs/project_management/tasks/WB-001_CLAW4_PLATFORM_MAP.md`
  4. `项目总规划/AGENTS.md`
  5. `项目总规划/CLAW4_BRINGUP_PROMPT.md`（1342 行全文）
  6. `docs/CLAW4_AUDIT.md`
  7. `docs/HARDWARE_ASSUMPTIONS.md`
  8. `docs/BUILD.md`
  9. `docs/CLAW4_主机准备情况报告_2026-09-01.md`
  10. `vendor/MetalioClaw4/README.md`、`sdkconfig`、`partitions/v1/32m_dual.csv`、`main/boards/metalio-claw-4/*`、`main/display/*`、`main/audio/*`、`main/ota.cc`、`main/boards/common/*`、`esp_claw_bin/README.md`、`dependencies.lock`、`managed_components/*`
- 允许修改路径：`docs/CLAW4_PLATFORM_MAP.md`、`docs/project_management/reports/WB-001_REPORT.md`
- 实际范围偏差：无

## 3. 修改文件

| 文件 | 修改目的 |
| --- | --- |
| `docs/CLAW4_PLATFORM_MAP.md` | WB-001 唯一交付物：12 类平台子系统证据映射 |
| `docs/project_management/reports/WB-001_REPORT.md` | 本实施报告 |

> 未修改任务看板、Codex 报告、vendor 官方源码、ESP-IDF、partition table、业务代码；未运行安装/下载/构建/格式化/刷写/自动修复命令。

## 4. 实现摘要

- 基于官方源码快照 `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950` 与已验收主机基线（BLD-001），完成 `docs/CLAW4_PLATFORM_MAP.md`。
- 覆盖任务包要求的全部 12 个子系统，每项给出：状态分级（`SOURCE_CONFIRMED`/`CONFIG_CONFIRMED`/`LIKELY`/`DEVICE_VERIFY_REQUIRED`）、关键配置项与当前值、主要实现文件和入口、与 BSP/IDF/托管组件的关系、MVP 复用策略、禁止修改边界、真机验证方法与预期证据。
- 明确记录了任务包要求的特殊项：
  - **Flash mode 不一致**：`sdkconfig:731` `CONFIG_ESPTOOLPY_FLASHMODE_QIO=y` 与 `:736` `CONFIG_ESPTOOLPY_FLASHMODE="dio"` 并存，原样记录并标注风险，未修改。
  - **非对称 OTA**：`partitions/v1/32m_dual.csv` 实测 ota_0=9M / ota_1=4M，未当作对称 A/B；结合 `esp_claw_bin/README.md` 确认 ota_0=xingzhi 主固件、ota_1=ESPClaw edge_agent。
  - **固件尺寸风险**：`xiaozhi.bin` 9,036,192 B（≈8.62 MiB）放不进 ota_1（4M），溢出 0x49e1a0（与 BUILD.md 一致）。
  - 所有未连接实机的能力一律标记 `DEVICE_VERIFY_REQUIRED`，未将任何源码/配置存在写成实机工作正常，未根据电商描述推断硬件。
- 额外的源码级发现（写入映射 §4）：
  - AEC 默认关闭（`application.cc:55` 注释确认，由 NVS 恢复）。
  - `METALIO_CLAW_4 : public DualNetworkBoard`，4G 槽位设置值 1 对应 legacy 枚举名 `NetworkType::ML307`，当前源码实际实例化 `Nt26Board`（CR-WB001-04 修订，见 §4.1）；C5 经 ESP-Hosted SDIO 提供 Wi-Fi；实机网络路径待确认。
  - OV2710 摄像头为 MIPI CSI 1080p@25fps 配置。

### 4.1 本轮修订（CR-WB001-01~04, Codex 复检意见）

- **CR-01（显示 bpp）**: 摘要/总表/§4.4 统一为 "NV3051F 默认：RGB888/24bpp（metalio-claw-4.cc:312 `bits_per_pixel=24`）；FL7707N 备选：16bpp（:355-376）"。`config.h:36` `LCD_BIT_PER_PIXEL(16)` 与默认初始化 24bpp 的差异作为**源码内部不一致**单独记录，不覆盖初始化路径。SKU 保持 `DEVICE_VERIFY_REQUIRED`。
- **CR-02（LVGL buffer）**: `lcd_display.cc:255` `buffer_size=width*height*50` 原样记录为可疑配置值（单位像素）；`avoid_tearing=true`（lcd_display.cc:221/279）时 `esp_lvgl_port_disp.c:256-262` 将 buffer_size 覆盖为 `hres*vres`=720×720 像素并复用 2 个 DPI panel frame buffer。**不再推算 25.9MB PSRAM 占用**；真实分配量/运行稳定性列为实机/运行时验证（§6 清单第 2 项）。
- **CR-03（电源）**: 明确区分 CX25601N 充电控制（metalio-claw-4.cc:537/556/610 `InitializeCx25601n()` → `cx25601n_set_ichg_ma(ichg_ma)`）、BQ27220 电量计（:606）、USB 充电状态 GPIO（config.h:9 `USB_CHG_STA_PIN GPIO53`）；SY6970/ADC/AXP2101 只列为仓库通用/其他板卡实现。
- **CR-04（4G 消歧）**: legacy `NetworkType::ML307` 枚举名（dual_network_board.cc:44/49）与当前实际实例化 `Nt26Board`（dual_network_board.cc:54-57）区分；不写成"当前默认使用 ML307 模组"；实机模组 `DEVICE_VERIFY_REQUIRED`。

## 5. 验收标准自检

| 验收项 | PASS / FAIL / BLOCKED | 证据 |
| --- | --- | --- |
| 两个交付文件均存在，且没有修改其他文件 | PASS | `git status` 仅两个新增文件；见 §6 |
| 12 类平台信息全部覆盖 | PASS | 映射文档 §3 总表 12 行 + §4 分节 4.1~4.12 |
| 每个重要结论能追溯到具体文件、配置项或已验证报告 | PASS | 每节含 sdkconfig 行号/文件路径/README 引用 |
| 源码事实、配置事实、推测、实机事实分级清楚 | PASS | 状态分级体系：SOURCE_CONFIRMED/CONFIG_CONFIRMED/LIKELY/DEVICE_VERIFY_REQUIRED |
| Flash mode、非对称 OTA、固件尺寸风险均明确记录 | PASS | 映射 §4.2（Flash mode）、§4.3（OTA）、§8（风险表） |
| 未把任何未连接实机的能力写成 CONFIRMED | PASS | 12 项 DEVICE_VERIFY_REQUIRED 清单 §6；无实机能力被写成 CONFIRMED |
| Git diff 不包含 vendor/、toolchains/、构建产物或日志 | PASS | diff 仅 2 个 docs 文件；见 §6 |
| 报告中附带只读核查命令和结果摘要 | PASS | 见 §6 验证命令与结果 |
| CR-WB001-01~04 在平台映射与报告中均已反映 | PASS | 见 §4.1 与映射 §4.4/§4.6/§4.10/§8 |

## 6. 验证命令与结果

> 全部为只读命令；未执行任何安装、下载、构建、格式化、刷写或自动修复命令。

### 6.1 Git 状态与基线

```
git -C vendor/MetalioClaw4 status --short --branch
=> ## main...origin/main（干净）
git -C vendor/MetalioClaw4 rev-parse HEAD
=> ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950
```

### 6.2 sdkconfig 关键证据（只读 grep）

```
CONFIG_IDF_TARGET="esp32p4"        （:575）
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=360（:1676）
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y   （:731）⚠️
CONFIG_ESPTOOLPY_FLASHMODE="dio"   （:736）⚠️ 不一致
CONFIG_ESPTOOLPY_FLASHSIZE="32MB"  （:749）
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v1/32m_dual.csv"（:768）
CONFIG_SPIRAM=y / MODE_HEX=y / SPEED_200M=y（:1622/1627/1628）
CONFIG_ESP_HOSTED_ENABLED=y / CP_TARGET_ESP32C5=y / SDIO_HOST_INTERFACE=y（:3041/3046/3062）
CONFIG_CAMERA_OV2710=y（:2937）
```

### 6.3 分区表（只读 Get-Content 等价）

```
partitions/v1/32m_dual.csv：ota_0 app 9M @0x200000；ota_1 app 4M；resources 4M；
factory_test 600K；emote 4M；system FAT 1M；storage FAT 7M；coredump 64K；nvsfactory/nvs/otadata/phy_init/model
```

### 6.4 源码检索（只读 grep/find）

```
屏幕切换宏：metalio-claw-4.cc:24-37  METALIO_CLAW_4_USE_FL7707N 默认 0 → NV3051F
GT911 触摸：config.h TOUCH_INT_PIN=GPIO33；touch_feed.cc ISR 注册
音频 codec：metalio-claw-4.cc:768-772  GetAudioCodec() → BTAudioCodecDuplex（16kHz）
电源：metalio-claw-4.cc:551-573  charge ichg_ma=1000；Bq27220Gauge 绑定 i2c_bus
OTA：ota.cc:277-279  运行于 factory 则跳过；:300  esp_ota_get_next_update_partition
LVGL：managed_components/lvgl__lvgl/lv_version.h → 9.3.0；esp_lvgl_port 2.6.3
双系统：esp_claw_bin/README.md  edge_agent→ota_1@0xb00000；emote/system/storage 数据分区
```

### 6.5 构建产物核对（只读）

```
E:\b\xiaozhi.bin 9,036,192 B；bootloader 20,416 B；partition-table 3,072 B；factory_test 614,400 B
（与 BLD-001/BUILD.md 一致，未改动）
```

### 6.6 范围确认（只读）

```
git status --short
=> ?? docs/CLAW4_PLATFORM_MAP.md
=> ?? docs/project_management/reports/WB-001_REPORT.md
（提交前仅这两个新增文件；无其他改动）
```

### 6.7 Git 提交链（Git 元数据修订，CR 复检报告 §3）

```
首轮交付提交：dbdc691  docs(WB-001): add Claw4 platform map and implementation report
父提交：      0f97af4  chore: establish Codex WorkBuddy control workflow（Codex 基线，25 文件）
异常独立根提交：5da866f（含 25 基础文件 + 2 交付文件，因 --amend --only 误操作产生）
  → 已隔离保存于 backup/wb-001-orphan-race，不影响任务分支与交付内容
本报告不固定提交号：以任务分支 HEAD 为准；提交说明记录父提交与前序提交
本轮开始前远端同步：git pull --ff-only origin workbuddy/wb-001-platform-map → HEAD 与远端一致（4a04b3cd）
```

### 6.8 本轮修订证据（只读 grep，CR-WB001-01~04）

```
metalio-claw-4.cc:25-26   NV3051F 36MHz DPI RGB888 24bpp（默认）; FL7707N 48MHz DPI RGB888 16bpp（备选）
metalio-claw-4.cc:312     .bits_per_pixel = 24（NV3051F 分支）
metalio-claw-4.cc:355-376 FL7707N 分支 RGB888, bits_per_pixel=16
config.h:36               LCD_BIT_PER_PIXEL (16) — 与 NV3051F 24bpp 初始化不一致（只记录不修改）
lcd_display.cc:255        .buffer_size = width_ * height_ * 50（可疑值, 原样记录）
lcd_display.cc:221/279    .avoid_tearing = true
esp_lvgl_port_disp.c:256-262  avoid_tearing=true → buffer_size = hres*vres; dpi_panel_get_frame_buffer(2)
metalio-claw-4.cc:537/556/610  InitializeCx25601n(); cx25601n_set_ichg_ma(ichg_ma)
dual_network_board.cc:44/49    network_type==1 → NetworkType::ML307（legacy 枚举名）
dual_network_board.cc:54-57    ML307 分支 make_unique<Nt26Board>（实际实例化）
dual_network_board.cc:69       SaveNetworkTypeToSettings(NetworkType::ML307)
```

## 7. 风险与未决项

- `DEVICE_VERIFY_REQUIRED`：全部 12 项实机验证（见映射 §6 清单），包括 P4 revision、PSRAM/Flash 实测、屏幕 SKU、GT911、C5/ESP-Hosted、网络路径、音频硬件、OV2710、SD 热插拔、电池、boot 分区。
- 其他风险：
  - Flash mode 不一致（QIO=y vs "dio"）——记录未修改，真机 Flash 异常时首个排查点。
  - 非对称 OTA + xiaozhi.bin 超 ota_1 容量——双槽 OTA 能力存疑，实机前不承诺、不改分区。
  - AEC 默认关闭；4G 槽位 legacy 枚举名 ML307 / 当前源码实际实例化 Nt26Board，实机模组与启动网络路径待确认（CR-WB001-04）。
  - config.h `LCD_BIT_PER_PIXEL(16)` 与 NV3051F 初始化 24bpp 不一致（源码内部, CR-WB001-01）。
  - LVGL `buffer_size=width*height*50` 可疑配置 + 防撕裂路径覆盖（CR-WB001-02）→ 实机显示压力测试验证。
  - P4 revision 未锁定（rev_min 0）；无签名 OTA 证据。
- 阻塞项：无（本任务不依赖真机；真机相关项已全部标记 DEVICE_VERIFY_REQUIRED 而非阻塞）。

## 8. 给 Codex 的复检重点

1. **映射文档状态分级是否严格**：确认没有任何"未连接实机"项被写成 CONFIRMED；重点抽查 §4.4 显示、§4.6 网络、§4.8 摄像头。
2. **Flash mode 不一致证据**：`sdkconfig:731`（QIO=y）vs `:736`（"dio"）是否如文档所述原样记录且未修改。
3. **非对称 OTA 表述**：ota_0=9M/ota_1=4M 及 ESPClaw 双系统描述是否与 `esp_claw_bin/README.md`、分区 CSV 一致。
4. **范围符合性**：`git diff` 仅两个交付文件，未触碰 vendor/toolchains/构建产物/看板/Codex 报告。
5. **四项修订（CR-WB001-01~04）** 是否与 Codex 复检意见一致：显示 bpp（NV3051F 24bpp 默认/FL7707N 16bpp 备选 + config.h 不一致）、LVGL buffer 单位与防撕裂覆盖路径、CX25601N/BQ27220/USB GPIO 区分、ML307 legacy 枚举名 vs Nt26Board 实际实例化。

## 9. 下一步

- 建议 Codex：复检本 diff 后决定 `ACCEPTED` / `CHANGES_REQUIRED`；若验收通过，可安排 CR-001 平台映射验收，再调度 WB-002（ARCHITECTURE.md）。
- WorkBuddy 在收到明确调度前不启动 WB-002、真机 Bring-up 或任何 MVP 代码（符合任务包"完成后的明确指令"）。
