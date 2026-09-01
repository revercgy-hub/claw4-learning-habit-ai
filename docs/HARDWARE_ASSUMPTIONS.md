# Claw4 硬件事实与假设台账

> 本文刻意区分“官方源码配置”与“当前这台实机的事实”。没有实机日志、原理图/BOM 或运行测试支撑的条目，均不得作为产品实现的硬编码依据。

## 分级规则

- **CONFIRMED_SOURCE**：当前官方源码或锁定配置明确声明。
- **CONFIRMED_DEVICE**：实机日志、ESP-IDF API、原理图/BOM 或实测已经确认。
- **SKU_DEPENDENT**：源码或资料显示可能因版本变化。
- **UNKNOWN**：当前尚无可靠证据。
- **FAILED**：实机测试已失败。

## 当前台账

| 项目 | 当前结论 | 等级 | 证据 | 真机验证方法 |
| --- | --- | --- | --- | --- |
| P4 目标芯片 | `esp32p4` | CONFIRMED_SOURCE | `sdkconfig` | 启动日志 + `esp_chip_info()` |
| P4 CPU 配置 | 360 MHz | CONFIRMED_SOURCE | `sdkconfig` | 启动日志/API |
| C5 网络协处理器 | ESP32-C5 | CONFIRMED_SOURCE | ESP-Hosted 配置 | C5 版本/Hosted 日志 |
| P4↔C5 传输 | SDIO、4-bit、40 MHz | CONFIRMED_SOURCE | `sdkconfig` | Hosted 初始化/网络测试日志 |
| Flash | 32 MB、40 MHz | CONFIRMED_SOURCE | `sdkconfig` | `esp_flash_get_size()` + 启动日志 |
| PSRAM | 已启用、HEX、200 MHz | CONFIRMED_SOURCE | `sdkconfig` | heap capability API + 启动日志 |
| 显示分辨率 | 720×720 | CONFIRMED_SOURCE | 板卡 `config.h` | 纯色/文本/触摸映射测试 |
| 屏幕驱动 | NV3051F 默认；FL7707N 可选 | SKU_DEPENDENT | 板级实现 | 面板/启动日志/实际显示 |
| 触摸控制器 | GT911 | CONFIRMED_SOURCE | 板级实现 | I2C 初始化与四角触摸测试 |
| 摄像头型号/能力 | 未确认 | UNKNOWN | 仅有相机相关代码 | 传感器探测和多档分辨率测试 |
| 麦克风/喇叭/Codec | 未确认 | UNKNOWN | 仅有 16 kHz 与音频抽象配置 | 录音、播放、全双工测试 |
| SD 卡接口与热插拔 | 接口配置存在；热插拔能力未确认 | UNKNOWN | 板级引脚配置 | 无卡/插卡/写入/断电恢复测试 |
| 电量计 | BQ27220 代码存在 | HARDWARE_VERIFY_REQUIRED | 板级实现 | 读取 SOC、电压与充电状态 |
| 4G/GPS/无线充电/其他传感器 | 未确认 | SKU_DEPENDENT | 官方工程含相关功能页面 | 单项枚举与实测 |
| 32MB 双系统分区 | 当前构建配置使用 v1 dual CSV | CONFIRMED_SOURCE | `sdkconfig` | boot log + 实际刷写清单 |

## 实机到货后应优先填充的证据

1. 未改动官方基线的启动完整日志。
2. P4 flash/monitor 使用的 COM 端口描述和 USB 枚举结果。
3. P4 revision、CPU 频率、Flash 和 PSRAM 的 API 输出。
4. C5 固件版本、ESP-Hosted 传输状态与 Wi-Fi 连接日志。
5. 显示面板/触摸初始化日志、四角触摸结果。
6. 当前 partition table 和 boot partition 日志。

在上述六项完成前，任何屏幕、引脚、内存、分区或网络实现均应标记为 `HARDWARE_VERIFY_REQUIRED`。
