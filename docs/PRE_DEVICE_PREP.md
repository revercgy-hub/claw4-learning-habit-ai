# Claw4 到货前准备清单

## 已完成

- [x] 获取官方 MetalioClaw4 源码快照。
- [x] 锁定官方源码 Commit、推荐 ESP-IDF 版本、目标芯片与当前分区配置。
- [x] 完成源码层面的平台审计和硬件事实台账。
- [x] 准备无设备可运行的主机预检脚本。

## 仍需准备

- [ ] 安装 ESP-IDF **v5.5.4**（含 ESP32-P4 工具链和 Python 环境）。
- [ ] 运行 `tools/bringup/host-preflight.ps1`，确认 Git、Python、`idf.py` 和 `IDF_PATH`。
- [ ] 准备一条确认可传数据的 USB-C 线，以及稳定的电源适配器。
- [ ] 确认 Windows 能访问设备管理器和 COM 端口；P4 应识别为 `USB JTAG/serial debug unit`。
- [ ] 准备可连接的 2.4 GHz Wi-Fi 测试网络，以及可访问 HTTPS 的网络环境。
- [ ] 若计划验证 SD，准备一张非关键数据、FAT 格式的 microSD 卡。

## 到货首日顺序

1. 拍照记录外包装、SKU、板号、接口与随机配件；不要仅凭商品页判断能力。
2. 连接 USB，记录所有枚举端口和未改动官方固件的启动日志。
3. 在官方源码目录执行一次无修改构建；如条件合适，先刷官方基线并确认能启动。
4. 仅执行 Bring-up Stage 1：B001、B002、B003、B004、B005、B009、B013。
5. 将所有结论写为 `CONFIRMED_DEVICE / SKU_DEPENDENT / UNKNOWN / FAILED`，生成真实的 `BRINGUP_STAGE1_REPORT.md`。
6. 第一轮结束后停止；根据结果决定是否执行音频、摄像头、存储、电源和压力测试。

## 不应提前做的事

- 修改 BSP、屏幕时序、PSRAM 参数、`sdkconfig` 或 partition table。
- 烧录未经核对来源的二进制文件。
- 直接开发学习业务、家长端、云端 AI、摄像头常驻或复杂动画。
