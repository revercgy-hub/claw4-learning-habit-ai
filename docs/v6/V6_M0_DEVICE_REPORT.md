# V6 M0 设备候选证据

2026-09-22，Codex，分支 codex/v6-foundation。

## 候选 m0.1

独立 Claw4 Board overlay；上游 XiaoZhi v2.5.0、IDF v6.1 均固定 SHA。上游应用、状态机、音频服务及协议源码在 CRLF 归一化后逐字节一致。M0 单独诊断入口不启动云端协议或 OTA。实现屏幕、触摸、双通道从机 I2S、外部音频模块初始化、C5 SDIO 网络初始化；Camera/SD/电源键尚未完成。

构建入口 tools/v6/build_device.py，实际构建树 E:/v6/s1，第二次构建成功，日志 out/v6-device-build-02.txt。设备 AEC、P4 rev1.x、Flash 32MB DIO 40MHz 已生效。应用 3,043,216 字节；详细哈希见 integration/v6/m0-candidate.json。首次构建 C++ 类型错误已修正，原日志保留。

验证：tools/v6 下 unittest discover 共 12 项 PASS；freeze_candidate.py PASS；flash_plan.py PASS；esptool image-info 校验有效，IDF v6.1 / app 6.0.0-m0.1 / P4 最大修订 1.99。

设备只读取证：COM7，ESP32-P4 rev1.3，32MB Flash。安全启动/Flash 加密关闭。当前实际分区表位于 0x9000。完整备份位于忽略目录 out/v6-device-private/pre-v6-full-flash.bin，33,554,432 字节，SHA256 b77343691c97359eaedba4d7d8353ef6df55b10ae2f5f9a13059943c04bb9413。备份含私密数据，不提交。可从该完整镜像恢复原布局与固件，恢复写回尚未实测。

计划仅写 0x2000 bootloader、0x9000 分区表、0x10f000 speech models、0x200000 factory app、0x1000000 assets。所有分区边界保持原值，仅 resources 标签改为 assets。保留 nvsfactory、nvs、phy_init、factory_test。不写 C5 固件或 eFuse。刷写前记录状态：NOT_YET_FLASHED；后续追加实测证据，不把构建通过写成硬件验收。

## 首次实机与修正

候选提交 5604892。2026-09-22 五个区域刷写 exit=0，原日志 flash-m0-01.txt 保留在私密目录。boot-m0-01.txt 的 40 秒取证反复出现 sdio_mempool_create / buf_mp_g 断言；没有进入 Board 初始化，M0 启动 FAIL。日志识别 32MB PSRAM。

调用位置为 ESP-Hosted 启动 constructor。其默认 transport 分配器要求片内 DMA 内存；修订启用组件原生 CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM=y，使其优先使用 DMA-capable PSRAM。依赖源码不修改。重新构建日志 out/v6-device-build-03.txt；运行效果待后续实测。

## 第二轮实机

修订构建 build-03 exit=0，冻结 m0-candidate-02.json。除应用以外四个产物哈希均与首次写入一致，因此第二次仅写 factory app，flash-m0-02.txt exit=0。boot-m0-02.txt 35 秒日志确认：TCA9555、NV3051F、GT911、I2S slave 初始化完成；BOOT_READY；两次健康记录，无断言重启。模型 wn9_nihaoxiaozhi_tts 加载，AFE 为 1 麦克风 + 1 回声参考，C5 从机枚举 chip ID=12。以上只证明驱动初始化，屏幕质量、触摸事件和音频质量仍需交互实测。

同时发现 assets@0x1000000 无法 mmap，24-bit Flash 地址限制。设备 Flash ID c8:4019；Metalio AgentUI 硬件参考同样启用 IDF_EXPERIMENTAL_FEATURES 与 BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH。第三轮按原布局补齐这两个配置，保留原始失败证据。不把 NETWORK_EVENT=0 写成网络连接成功。

## M0 硬件验收矩阵（持续更新）

| 项目 | 当前证据 | 状态 |
| --- | --- | --- |
| Flash / PSRAM | 32MB Flash 读取、写入校验；启动识别 32MB PSRAM | 基础证据通过，长期压力未测 |
| 启动 | 第二轮 BOOT_READY，后续 >120s 健康记录 | 初步通过 |
| 显示 | NV3051F 初始化成功 | 观感待用户确认 |
| 触摸 | GT911 初始化成功，尚无点击事件 | 待实测 |
| C5 | SDIO 枚举 chip ID 12 | IP/网络可靠性待实测 |
| 音频 | I2S / AFE / WakeNet 初始化 | 录音、回放、唤醒质量待实测 |
| 资源分区 | 第二轮 mmap 失败 | 配置修正后待复测 |
| SD 卡 / Camera / 电源键 | 未集成 | 未完成 |
| NAS 连续语音 | M1 才接入 | HOLD |

本轮不宣称 M0 总体验收或可发布固件。完整原镜像仍保存在本地私密目录；未经恢复写回不能称为已实测回滚。

## 第三轮实机：当前候选

build-04 成功，冻结 m0-candidate-03.json。第三轮仅写 Bootloader 与 factory app；其余三份产物哈希确认未变化。flash-m0-03.txt exit=0，两个 Hash of data verified；首次五份、第二次一份亦均校验通过。组件锁与原冻结内容一致。

boot-m0-03.txt 45 秒观察：Assets applied=1、BOOT_READY、三次 HEALTH，free=28,027,003 B / PSRAM=27,878,604 B，无断言或重启。结论 BOOT_AND_ASSETS_PASS_INTERACTION_PENDING；尚未收到用户屏侧反馈，taps=0，不宣称触摸/录放音/唤醒实测通过。保留初始化阶段 system_api MAC 未就绪告警，后续 C5 枚举和扫描可运行，IP 连接及 M1 网络联调尚未验证。

工具测试 12/12 PASS；新增冻结要求检查 Flash 40MHz、ESP-Hosted PSRAM 内存池、高地址映射选项。当前设备保留第三版诊断固件；原完整镜像和全部原始串口日志留在忽略目录。脱敏证据索引见 integration/v6/m0-device-evidence.json。

## 第四轮：用户蓝屏反馈与显示修正

用户报告第三轮实际屏幕为整块纯蓝、无文字或按钮；因此此前启动/资源检查不代表显示通过。板级显示由通用 RGB565 partial-transfer MipiLcdDisplay 改为独立 Claw4Display，继承上游 LcdDisplay UI；采用与原硬件参考一致的原生 RGB888，两个 panel framebuffer、full refresh、避免撕裂。上游应用、UI 源码、音频逻辑未修改。不能仅凭本次联合改动断言蓝屏来自单个寄存器或唯一颜色转换缺陷。

build-05 的 C++ 初始化类型错误修正后，build-06 exit=0；冻结 m0-candidate-04.json。核对其余四个固件产物哈希不变，仅刷 factory app；flash-m0-04.txt exit=0。boot-m0-04.txt 记录 Native RGB888、LVGL first refresh completed、Assets applied=1。

2026-09-22 用户明确反馈“已显示文字和按钮”：DISPLAY_VISIBLE=PASS，蓝屏消除。触摸点击、音频录放与唤醒效果仍未验收。当前零旋转配置下，通用 LVGL port 调用面板不支持的 swap_xy/mirror 时有日志告警；没有要求旋转，画面已由用户确认，后续适配时再消除这类能力探测告警。

完整 35 秒串口取证随后确认 TOUCH count 最大为 5，三次启动本地录音并进入 playback queued；触摸事件链通过。此证据仍不代表麦克风和扬声器音质通过，待用户试听反馈。
