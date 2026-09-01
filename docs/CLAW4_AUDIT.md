# Metalio Claw4 官方仓库审计

> 审计日期：2026-08-31
> 范围：官方 `CloudZao/MetalioClaw4` 当前源码快照；不包含任何实机结论。

## 基线快照

| 项目 | 值 | 状态 |
| --- | --- | --- |
| 本地源码 | `vendor/MetalioClaw4` | 已获取 |
| 远端 | `https://github.com/CloudZao/MetalioClaw4.git` | 已获取 |
| 分支 | `main` | 已记录 |
| Commit | `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950` | 已记录 |
| 标签 | `latest` | 已记录 |
| 子模块 | 仓库未声明 `.gitmodules` | 已确认 |
| 工作树 | 未修改官方源码 | 已确认 |
| 官方基线编译 | 未执行 | 阻塞：本机未安装 Python / ESP-IDF |

## 仓库结构

| 路径 | 职责 | 对学习终端的处理 |
| --- | --- | --- |
| `main/` | 应用、界面、板卡适配、音频、协议、OTA | 作为主继承点；业务层应在其外建立清晰边界 |
| `main/boards/metalio-claw-4/` | Claw4 板级实现、屏幕、触摸、总线和引脚 | 保留，除实机不匹配外不修改 |
| `main/display/` | LVGL 适配与现有各类界面 | 可复用显示抽象，不直接耦合学习业务 |
| `main/audio/` | 编解码、唤醒词和音频处理 | 后续语音阶段复用；MVP 首阶段不依赖 |
| `main/protocols/` | WebSocket / MQTT 协议 | 可参考或封装；学习同步须另建幂等事件协议 |
| `components/usb_device_uac/` | 本地 USB UAC 组件 | 保留官方依赖 |
| `partitions/` | v1 / v2 分区表方案 | 仅审计，禁止在实机前修改 |
| `esp_claw_bin/` | ESPClaw 双系统相关二进制与说明 | 仅作为现有双系统基线参考 |
| `sd_images/` | SD 资源说明与资源 | 非 MVP 强依赖 |
| `wakeword/` | 可选自定义唤醒词模型 | 后续语音阶段再评估 |

## 平台映射（源码配置，不等同于实机事实）

| 模块 | 源码中的实现/配置 | 实机状态 |
| --- | --- | --- |
| 主控 | `sdkconfig` 目标为 `esp32p4` | HARDWARE_VERIFY_REQUIRED |
| 网络协处理器 | ESP-Hosted 配置目标为 `esp32c5`，SDIO 4-bit、40 MHz | HARDWARE_VERIFY_REQUIRED |
| CPU | 默认 360 MHz | HARDWARE_VERIFY_REQUIRED |
| Flash | `sdkconfig` 设为 32 MB、40 MHz | HARDWARE_VERIFY_REQUIRED |
| PSRAM | 已启用，HEX、200 MHz | HARDWARE_VERIFY_REQUIRED |
| 显示 | 720×720、2-lane MIPI-DSI；NV3051F 为当前默认，FL7707N 为可选 | SKU_DEPENDENT |
| 触摸 | GT911，I2C，GPIO33 中断 | HARDWARE_VERIFY_REQUIRED |
| 音频 | 16 kHz 输入/输出，`BTAudioCodecDuplex` 适配 | HARDWARE_VERIFY_REQUIRED |
| SD | SDMMC 4-bit 接口 | HARDWARE_VERIFY_REQUIRED |
| 电池 | 有 BQ27220 电量计代码 | HARDWARE_VERIFY_REQUIRED |
| 摄像头 | 存在相机界面与测试页 | HARDWARE_VERIFY_REQUIRED |
| OTA | 当前 `sdkconfig` 指向 `partitions/v1/32m_dual.csv` | 实机启动日志与当前刷机镜像待确认 |

## 关键配置与实现位置

- 构建项目：根目录 `CMakeLists.txt`，项目版本 `2.0.51`。
- 依赖与版本：`main/idf_component.yml` 与 `dependencies.lock`。
- ESP-IDF：官方 README 指定 **v5.5.4**；清单最低要求为 `>=5.5.2`。开发基线以 README 的 v5.5.4 为准。
- 板卡配置：`main/boards/metalio-claw-4/config.h`、`config.json` 与 `metalio-claw-4.cc`。
- UI：LVGL `~9.3.0`、`esp_lvgl_port ~2.6.0`（由组件清单声明）。
- P4 调试：官方 README 指定系统描述为 `USB JTAG/serial debug unit` 的端口。
- P4 与 C5：源码配置为 ESP-Hosted over SDIO；不能写成 P4 原生 Wi-Fi。
- 当前分区：`partitions/v1/32m_dual.csv` 中 `ota_0` 为 9 MB、`ota_1` 为 4 MB；同时有 `emote`、`system`、`storage` 等数据分区。二者在双系统中的实际职责须用启动日志与烧录清单验证。

## 可复用、需适配与禁止修改

### 可复用

- 板级初始化、显示、触摸、网络、音频、存储、电源与 OTA 的硬件抽象。
- 官方已存在的工厂/压力测试界面，作为 Bring-up 的起点。
- USB 调试与现有 WebSocket/MQTT 实现的连接管理经验。

### 需要在 MVP 中新增或封装

- 独立于 LVGL 和 BSP 的 `Task`、`StudySession`、计时和奖励领域模型。
- 设备事件队列、离线任务缓存、重试和后端幂等同步。
- 学习终端 Home / Focus / Done / Offline UI。
- 仅面向家庭后端的设备认证与 HTTPS/WSS API 客户端。

### 禁止在实机核验前修改

- `sdkconfig`、partition CSV、屏幕时序/驱动选择、PSRAM 参数、ESP-IDF 主版本和官方 BSP。

## 当前风险与下一步

1. 本机尚无 Python 及 ESP-IDF，故尚不能证明官方基线可编译。
2. 当前源码支持至少两种屏幕驱动，具体设备屏幕必须以实机与启动日志核验。
3. 双系统分区存在不对称 OTA 槽位；在核验刷机包、启动日志和代码前，不可将其当作通用 A/B OTA。
4. 真机到货前优先完成开发环境安装和预检；到货后先跑 Bring-up Stage 1，再决定是否进入学习 MVP。
