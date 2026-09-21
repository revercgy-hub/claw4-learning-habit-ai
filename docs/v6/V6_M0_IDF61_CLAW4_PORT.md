# V6-M0 Board Port 执行设计

状态：设计/源码核对；尚未实现或编译新 Board。Codex 负责。

## 端口边界与顺序

新 identity `claw4-learning-v6`，在全新 XiaoZhi v2.5.0 工作树中增加独立板目录；同步 config.json → Kconfig → CMake → 单一 DECLARE_BOARD 注册链。项目仓库保存源/精确补丁与摘要，第三方源码和构建产物留在忽略目录。禁止重放旧 `project-ca3aa3fa.patch`，不改别的 P4 板引脚来伪装 Claw4。

1. 完整准备 IDF6.1 submodules、P4 工具链与 Python 环境，保存版本和 hash。按 rev1.x 选择核对 esp-sr=2.4.7；不把工具包现存 5.5 环境当 6.1。
2. 对照 `main/boards/common/board.h` 和 `espressif/esp32-p4-function-ev-board/` 学习新接口，硬件参数只取 Metalio 与本机证据。
3. 移植供电/I2C/扩展器、C5 SDIO，再接屏幕/触摸。启动失败必须分层定位，不带入整个旧 Home/应用框架。
4. I2S mic/speaker 接 upstream AudioCodec/AudioService；核对旧 BTAudioCodec 对外设上电的依赖后确定是否复用底层，实现不依赖旧 Bluetooth Screen。随后验证唤醒、录放与 AEC/防回采。
5. 接摄像头与 SD；没有摄像头证据不能编造型号。主动拍照只存合成场景，不接业务上传。
6. 解析组件锁、构建产物 manifest、实际镜像尺寸、分区边界检查齐全后，准备具体设备候选与可恢复操作清单。

## 已核对的参考参数（不是本轮硬件检测）

来源：Metalio `ca3aa3fa` 的 `main/boards/metalio-claw-4/config.h` 与板实现。

| 功能 | 源码参数 | 迁移关注点 |
| --- | --- | --- |
| I2C | SDA7 / SCL8 | 单一总线所有权、扩展器上电顺序 |
| Display | 720×720、RST3、背光52、DSI 2 lanes、LDO3 2500mV | NV3051F/FL7707N 两种实现；确认实际屏；IDF6 FourCC API |
| Touch | GT911、INT33 | reset/address 时序、LVGL 线程、方向校准 |
| Audio | 16kHz、BCLK12、WS10、DOUT9、DIN11 | 旧 BTAudioCodecDuplex；不能套 ES8311 Codec 假设 |
| SD | CLK43/CMD44/D0..3=39..42、LDO4 | 原注释要求核对布线；与 Wi-Fi 供电/host slot 冲突检查 |
| Button | boot35 | 不用其他 P4 板的 GPIO0 |
| C5 | ESP-Hosted over SDIO | 准确 pin/host/复位/固件协议待核对，拒绝 H2/C6 默认值 |

## 硬件验收与恢复

每项报告记录候选 hash、配置 hash、工具链、设备 revision、步骤、结果、日志路径：冷启动/重启、显示颜色方向、五点触摸、C5 连接/重连、mic 录音、speaker 播放、唤醒、主动 JPEG、SD 读写/缺卡。所有项当前 NOT_RUN。

写设备前重新只读取证设备和分区，匹配私密备份与 hash；生成完整 flash layout、写入清单、回退镜像和命令供审查。不能沿用旧 ota_0 或另一批 factory 地址。附件“允许频繁刷机”的历史文字不是本轮具体设备操作授权；本轮先完成可审查候选，届时按根规则处理。eFuse/永久安全设置不在范围。

M0 PASS 必须同时有可重复 build 和基础硬件实证；Host core build 或上游板 build 都不能替代 Claw4 build。
