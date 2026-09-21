# V6 M0 设备候选证据

2026-09-22，Codex，分支 codex/v6-foundation。

## 候选 m0.1

独立 Claw4 Board overlay；上游 XiaoZhi v2.5.0、IDF v6.1 均固定 SHA。上游应用、状态机、音频服务及协议源码在 CRLF 归一化后逐字节一致。M0 单独诊断入口不启动云端协议或 OTA。实现屏幕、触摸、双通道从机 I2S、外部音频模块初始化、C5 SDIO 网络初始化；Camera/SD/电源键尚未完成。

构建入口 tools/v6/build_device.py，实际构建树 E:/v6/s1，第二次构建成功，日志 out/v6-device-build-02.txt。设备 AEC、P4 rev1.x、Flash 32MB DIO 40MHz 已生效。应用 3,043,216 字节；详细哈希见 integration/v6/m0-candidate.json。首次构建 C++ 类型错误已修正，原日志保留。

验证：tools/v6 下 unittest discover 共 12 项 PASS；freeze_candidate.py PASS；flash_plan.py PASS；esptool image-info 校验有效，IDF v6.1 / app 6.0.0-m0.1 / P4 最大修订 1.99。

设备只读取证：COM7，ESP32-P4 rev1.3，32MB Flash。安全启动/Flash 加密关闭。当前实际分区表位于 0x9000。完整备份位于忽略目录 out/v6-device-private/pre-v6-full-flash.bin，33,554,432 字节，SHA256 b77343691c97359eaedba4d7d8353ef6df55b10ae2f5f9a13059943c04bb9413。备份含私密数据，不提交。可从该完整镜像恢复原布局与固件，恢复写回尚未实测。

计划仅写 0x2000 bootloader、0x9000 分区表、0x10f000 speech models、0x200000 factory app、0x1000000 assets。所有分区边界保持原值，仅 resources 标签改为 assets。保留 nvsfactory、nvs、phy_init、factory_test。不写 C5 固件或 eFuse。刷写前记录状态：NOT_YET_FLASHED；后续追加实测证据，不把构建通过写成硬件验收。
