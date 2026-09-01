# Claw4 实机到手后的 Codex Bring-up 提示词

## 使用目的

这份 Prompt 用于 Metalio Claw 4 实机到手后的第一轮硬件 Bring-up、平台核验、基线测试和风险识别。

目标不是立刻开发学习业务，而是先回答：

1. 这台具体 Claw4 实机到底是什么硬件版本？
2. CPU、PSRAM、Flash、屏幕、触摸、音频、摄像头、SD、电源、网络实际工作状态如何？
3. 官方仓库与这台实机是否完全匹配？
4. 哪些能力可以直接复用？
5. 哪些能力存在 SKU / revision 差异？
6. 哪些问题必须先解决，才能开始学习终端 MVP 开发？

这份 Prompt 应与项目根目录的 `AGENTS.md` 一起使用。

---

# 角色

你现在是本项目的：

- ESP32-P4 / ESP32-C5 嵌入式调试工程师
- Metalio Claw 4 平台适配工程师
- 硬件 Bring-up 工程师
- 固件性能测试工程师
- 设备可靠性验证工程师

你的首要任务：

**验证实机，而不是假设实机。**

你不得根据：

- 电商页面
- README 描述
- 旧版截图
- 第三方测评

直接认定当前设备硬件规格。

所有硬件事实必须尽可能来自：

1. 实机启动日志
2. ESP-IDF API
3. 官方仓库配置
4. 原理图 / BOM / Board Config
5. 实际运行测试

---

# 一、开始前必须阅读

开始任何操作之前，先读取：

```text
AGENTS.md
README.md
docs/
sdkconfig
sdkconfig.defaults
partition table
CMakeLists.txt
idf_component.yml
board config
BSP 相关目录
```

然后重点查看官方 Metalio Claw4 仓库中以下相关实现：

```text
display
touch
audio
camera
network
wifi
esp-hosted
sd/mmc
storage
power
battery
usb
ota
lvgl
mcp
openclaw
espclaw
```

如果目录名称不同：

以仓库实际结构为准。

不要自行假设路径。

---

# 二、第一原则：先保留官方可运行基线

在执行任何修改之前：

必须建立官方基线。

要求：

1. 记录当前 Git commit
2. 记录 branch
3. 记录 submodule 状态
4. 记录 ESP-IDF 版本
5. 记录 toolchain 版本
6. 记录 Python 环境
7. 记录 sdkconfig hash
8. 记录 partition table
9. 尝试官方原始工程完整编译
10. 如果条件允许，先烧录官方原始固件并确认可以正常启动

建议创建 Git tag：

```text
bringup-baseline
```

禁止：

- 先大规模重构再测试
- 一上来修改 partition table
- 一上来升级 ESP-IDF
- 一上来替换 BSP
- 一上来修改屏幕驱动
- 一上来改 PSRAM 参数

---

# 三、输出文件

整个 Bring-up 阶段至少生成以下文档：

```text
docs/BRINGUP_REPORT.md
docs/HARDWARE_FACTS.md
docs/BOARD_REVISION.md
docs/PERFORMANCE_BASELINE.md
docs/PERIPHERAL_MATRIX.md
docs/KNOWN_ISSUES.md
docs/DEVICE_LOG_REFERENCE.md
```

如果需要新增测试程序：

建议放：

```text
tools/bringup/
firmware/tests/bringup/
```

不要把临时测试代码散落在业务模块中。

---

# 四、TASK B001：确认芯片与硬件版本

首先输出完整芯片信息。

使用 ESP-IDF API、启动日志或官方 API 获取：

```text
ESP32-P4 chip model
chip revision
number of cores
CPU frequency
supported features
silicon revision
```

如果存在 ESP32-C5：

确认：

```text
C5 firmware version
C5 chip revision
C5 connection mode
ESP-Hosted version
transport interface
Wi-Fi capabilities
BLE capabilities
```

必须明确区分：

```text
主控 P4
网络协处理器 C5
```

不要把 C5 网络能力写成 P4 原生能力。

输出：

```text
docs/BOARD_REVISION.md
```

至少包含：

| 项目 | 实测值 | 来源 | 是否确认 |
|---|---|---|---|
| P4 Revision | | | |
| P4 CPU Freq | | | |
| C5 Revision | | | |
| Board Revision | | | |
| SKU | | | |
| Firmware Version | | | |

如果无法确认 Board Revision：

标记：

```text
HARDWARE_VERIFY_REQUIRED
```

---

# 五、TASK B002：内存与 Flash 体检

启动后打印：

```text
Internal SRAM total
Internal SRAM free
Internal SRAM largest block

PSRAM total
PSRAM free
PSRAM largest block

Flash size
Flash mode
Flash frequency

PSRAM type
PSRAM frequency
PSRAM mode
```

至少使用官方 heap API 获取：

```text
heap_caps_get_total_size()
heap_caps_get_free_size()
heap_caps_get_largest_free_block()
```

分别检查：

```text
MALLOC_CAP_INTERNAL
MALLOC_CAP_SPIRAM
MALLOC_CAP_8BIT
```

记录：

启动完成后

空闲 UI

进入设置页

播放音频时

摄像头开启时

网络传输时

多个功能并发时

的 heap 数据。

输出：

```text
docs/PERFORMANCE_BASELINE.md
```

---

# 六、TASK B003：Partition Table 核验

解析当前实际 partition table。

记录：

```text
Name
Type
Subtype
Offset
Size
用途
```

重点确认：

```text
factory
ota_0
ota_1
nvs
phy_init
spiffs/littlefs/fat
coredump
其他自定义分区
```

特别检查：

OpenClaw / ESPClaw 双模式是否：

1. 真正使用 ota_0 / ota_1
2. 使用独立 app partition
3. 使用不同数据分区
4. 通过 boot partition 切换

不得根据命名猜。

必须根据：

代码 + partition CSV + boot log

确认。

在未得到我明确许可前：

**禁止修改 partition table。**

---

# 七、TASK B004：显示系统核验

确认：

```text
屏幕控制器
分辨率
颜色格式
刷新方式
MIPI-DSI / RGB / SPI 等接口
Frame Buffer 数量
LVGL 版本
LVGL draw buffer 配置
DMA
旋转
Backlight
```

实测：

```text
720×720 是否正确
触摸是否与显示方向一致
动画是否流畅
页面切换是否稳定
是否有 tearing
是否有花屏
是否有闪屏
是否有内存泄漏
```

制作简单测试页面：

```text
纯色
渐变
文本
大图
按钮
列表滚动
局部动画
全屏页面切换
```

记录：

```text
平均 FPS
最低 FPS
CPU 使用趋势
PSRAM 使用变化
Touch latency
```

不要为了跑分临时修改生产配置。

---

# 八、TASK B005：Touch 核验

确认：

```text
Touch Controller
I2C/SPI 总线
中断方式
坐标范围
旋转映射
多点支持
```

测试：

- 四角点击
- 中心点击
- 快速连续点击
- 长按
- 滑动
- 慢速拖动
- 屏幕边缘
- UI 切换期间输入

目标：

```text
P95 Touch feedback < 100ms
```

若触摸方向或坐标异常：

先检查：

```text
rotation
mirror
swap_xy
display transform
```

不要直接在业务页面写魔法偏移值。

---

# 九、TASK B006：音频链路核验

确认实际硬件：

```text
Mic 数量
Mic 类型
Codec
I2S Port
I2S format
Sample Rate
Bit Depth
Speaker
PA / Amplifier
AEC capability
```

分别测试：

## 录音

至少：

```text
16 kHz mono
16 bit PCM
```

如官方支持更高采样率：

额外记录。

录制：

5 秒语音。

保存到：

```text
SD
或
RAM 临时缓冲
```

检查：

- 是否有明显底噪
- 爆音
- 丢帧
- DC offset
- 声道异常

## 播放

测试：

- PCM
- WAV
- 官方已有支持格式

逐步音量：

```text
20%
50%
80%
100%
```

观察：

- 失真
- 爆音
- 发热
- 重启
- brownout

## 同时录放

如果官方支持 AEC：

验证。

如果没有真实 AEC：

不要写成支持。

输出：

```text
docs/PERIPHERAL_MATRIX.md
```

---

# 十、TASK B007：摄像头核验

确认：

```text
Camera sensor 型号
接口类型
最大分辨率
支持 Pixel Format
JPEG 是否硬件/驱动支持
Frame Buffer 配置
PSRAM 占用
```

测试分辨率建议：

```text
320×240
640×480
800×600
1280×720
更高分辨率仅在硬件明确支持时测试
```

每档测试：

```text
初始化时间
首帧时间
平均帧率
PSRAM 占用
largest block
是否掉帧
是否花屏
```

再测试：

```text
Camera preview
+
LVGL UI
+
Wi-Fi
```

以及：

```text
Camera capture
+
JPEG upload
```

注意：

Bring-up 阶段只验证能力。

不要实现持续监控。

---

# 十一、TASK B008：SD / Storage 核验

检查：

```text
microSD interface
SDMMC / SPI
bus width
mount path
filesystem
max tested size
hot-plug support
```

测试：

1. 无卡启动
2. 插卡启动
3. 读取
4. 写入
5. 删除
6. 连续写文件
7. 断电恢复
8. 满盘处理

测试文件：

```text
1 KB
1 MB
10 MB
```

记录实际吞吐。

如果不支持热插拔：

明确写入文档。

不要把 SD 卡设为 MVP 的强依赖。

---

# 十二、TASK B009：Wi-Fi / C5 / ESP-Hosted 核验

重点验证双芯片网络架构。

确认：

```text
P4 ↔ C5 transport
ESP-Hosted mode
SPI/SDIO/UART 等实际接口
hosted firmware version
Wi-Fi mode
2.4 GHz
5 GHz（若实际支持）
BLE（若实际支持）
```

测试：

1. Wi-Fi 扫描
2. 连接 2.4G
3. 如支持则连接 5G
4. DHCP
5. DNS
6. HTTPS
7. WebSocket
8. 断网重连
9. 路由器重启
10. AP 切换
11. 信号弱场景

记录：

```text
连接耗时
重连耗时
RSSI
丢包
HTTP latency
WebSocket reconnect count
```

目标参考：

```text
Wi-Fi 恢复 < 5s
```

如果达不到：

记录真实结果，不要造假。

---

# 十三、TASK B010：USB 核验

确认 USB 能力：

```text
USB 口类型
OTG / Device / Host
USB Serial
JTAG
烧录
供电
数据
```

检查：

- PC 是否识别
- 串口日志是否稳定
- 烧录是否稳定
- 是否可同时供电和调试
- USB 断连后恢复
- reset / boot 行为

如果存在多个 Type-C：

必须明确每个口功能。

---

# 十四、TASK B011：电源 / 电池核验

确认：

```text
Battery capacity
Battery connector
Charging IC
Fuel Gauge
Power path
USB charging
Wireless charging（如果当前 SKU 实际存在）
```

不要根据商品描述直接认定电池容量。

实测：

```text
电池电压
SOC
USB 供电状态
charging state
放电状态
```

场景：

```text
Idle
Screen 30%
Screen 100%
Wi-Fi active
Audio playback
Camera active
Camera + Wi-Fi
Full workload
```

如果无法测真实电流：

至少记录：

- 电压
- SOC 下降趋势
- 温升
- 是否 brownout
- 是否异常重启

同时测试：

低电量处理。

---

# 十五、TASK B012：按键 / 传感器 / GPS / 其他外围

枚举所有实际外围设备。

包括但不限于：

```text
Buttons
LED
Vibration
IMU
GPS
RTC
Light Sensor
Battery Gauge
Wireless charging
4G module
```

每个模块标记：

```text
PRESENT
NOT_PRESENT
OPTIONAL_SKU
NOT_TESTED
FAILED
```

尤其注意：

电商页面存在不同 SKU。

所以：

**当前设备没有的模块，不等于整个 Claw4 平台都没有。**

当前设备有的模块，也不等于所有 SKU 都标配。

---

# 十六、TASK B013：系统日志基线

建立统一启动日志。

建议包含：

```text
=== CLAW4 BRINGUP ===

Firmware:
Git Commit:
Build Date:
ESP-IDF:
Board:
SKU:

P4:
Revision:
CPU:
Cores:

C5:
Revision:
Firmware:

Memory:
Internal:
PSRAM:
Flash:

Display:
Touch:

Audio:

Camera:

Storage:

Network:

Power:

Partitions:

Free Heap:

=====================
```

输出：

```text
docs/DEVICE_LOG_REFERENCE.md
```

方便以后判断设备版本和远程排错。

---

# 十七、TASK B014：稳定性测试

Bring-up 通过后：

进行长时间测试。

至少：

## 1 小时快速稳定测试

循环：

```text
UI
Wi-Fi
audio
sleep/wake
```

## 8 小时基础测试

保持：

```text
Wi-Fi connected
UI idle
heartbeat
```

## 24 小时目标测试

条件允许时执行。

要求：

```text
No crash
No watchdog
No brownout
No heap exhaustion
No progressive memory leak
```

每 60 秒记录：

```text
free heap
largest block
PSRAM
Wi-Fi state
uptime
```

---

# 十八、TASK B015：并发压力测试

组合测试：

### A

```text
LVGL
+
Wi-Fi
```

### B

```text
LVGL
+
Audio playback
```

### C

```text
LVGL
+
Camera
```

### D

```text
LVGL
+
Camera
+
Wi-Fi upload
```

### E

```text
LVGL
+
Audio
+
Wi-Fi
```

### F

```text
LVGL
+
Camera
+
Audio
+
Wi-Fi
```

记录：

```text
FPS
free heap
largest block
task stack
API latency
frame drops
audio underrun
camera failures
```

目标不是追求极限分数。

目标是确定：

**学习终端的安全性能边界。**

---

# 十九、性能预算结论

完成测试后，必须给出：

```text
SAFE
CAUTION
NOT_RECOMMENDED
```

三档。

例如：

```text
SAFE:
720×720 LVGL
Wi-Fi
Task Sync
TTS playback

CAUTION:
Camera preview + UI
Large PNG decoding

NOT_RECOMMENDED:
Continuous high-res camera
Complex full-screen alpha animation
Camera + heavy animation + audio + upload
```

但不能直接照抄以上示例。

必须以实测为准。

---

# 二十、MVP 开发准入条件

只有以下项目全部通过：

```text
官方 baseline 可编译
官方 baseline 可烧录
屏幕正常
Touch 正常
Wi-Fi 正常
HTTPS 正常
基本音频正常
PSRAM 稳定
Flash / Partition 已确认
无明显 brownout
1 小时无 crash
```

才可以进入：

**学习终端 MVP 开发。**

摄像头：

可以不是 MVP 准入条件。

GPS：

不是 MVP 准入条件。

4G：

不是 MVP 准入条件。

---

# 二十一、Bring-up 阶段禁止事项

禁止：

1. 开发复杂业务 UI
2. 接入真实 LLM
3. 接入家长端业务
4. 重构官方 BSP
5. 修改 partition table
6. 改 ESP-IDF 主版本
7. 持续开启摄像头
8. 做数字人
9. 加大型动画
10. 以“能跑”为理由跳过硬件验证
11. 用第三方参数覆盖实测结果
12. 把临时测试 hack 带入业务代码

---

# 二十二、硬件事实分级

所有硬件结论必须标记：

## CONFIRMED

通过：

```text
实机
日志
寄存器/API
原理图
官方当前代码
```

至少一种可靠方式确认。

## LIKELY

有较强资料支持，但未实测。

## UNKNOWN

当前无法确认。

## SKU_DEPENDENT

不同版本可能不同。

## FAILED

当前实测失败。

不要把：

LIKELY

写成：

CONFIRMED。

---

# 二十三、最终 BRINGUP_REPORT.md 必须回答

最终报告必须清晰回答：

## 1. 这台设备是什么版本？

包括：

```text
Board
P4 revision
C5 revision
CPU freq
PSRAM
Flash
Display
Touch
Audio
Camera
SD
Power
SKU
```

## 2. 哪些模块工作正常？

## 3. 哪些模块存在问题？

## 4. 哪些模块当前未验证？

## 5. 官方仓库是否与实机完全匹配？

## 6. 是否需要修改 BSP？

默认答案应尽量是：

**不需要。**

只有存在真实硬件适配问题时才建议修改。

## 7. 设备适合承担哪些学习终端任务？

例如：

```text
Task UI
Pomodoro
Voice interaction
Network sync
Audio
Camera capture
```

## 8. 哪些功能不建议放设备端？

例如：

```text
LLM
ASR heavy model
VLM
OCR
complex AI
```

必须根据性能实测得出。

## 9. 是否允许进入 MVP 开发？

最终给：

```text
GO
GO WITH CONDITIONS
NO-GO
```

并说明理由。

---

# 二十四、完成后停止

完成以下任务：

```text
B001 ~ B015
```

后：

停止继续开发。

不要自行进入学习业务层。

输出：

```text
docs/BRINGUP_REPORT.md
```

并给我总结：

1. 当前硬件版本
2. 实测性能
3. 已通过模块
4. 未通过模块
5. 风险
6. 是否可以开始 MVP
7. 建议下一批任务

---

# 二十五、第一次实际执行指令

现在开始执行 Bring-up。

第一轮只执行：

```text
B001
B002
B003
B004
B005
B009
B013
```

也就是优先确认：

```text
芯片
板卡版本
CPU
内存
PSRAM
Flash
Partition
显示
触摸
Wi-Fi / C5
统一启动日志
```

暂时不要测试：

```text
Camera
Audio
GPS
4G
Wireless charging
```

第一轮完成后停止，并生成：

```text
docs/BRINGUP_STAGE1_REPORT.md
```

如果当前：

没有连接到 Claw4 实机

或

无法访问串口

或

没有完整仓库

不要猜。

立即告诉我缺少：

```text
设备串口
USB连接
本地仓库路径
ESP-IDF环境
烧录权限
```

然后停止。
