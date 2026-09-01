# Codex 用户硬件证据接收记录

- 日期：2026-09-01
- 证据项：`USER-HW-EVIDENCE-001`
- 记录人：Codex
- 结论：`ACCEPTED`（已收到设备照片与一次受控 COM3 打开授权）
- 原始照片位置：`E:\workbuddy\学习习惯培育AI-device-evidence\USER-HW-EVIDENCE-001\photos\`（仓库外，不提交 Git）

## 1. 用户授权边界

用户于 2026-09-01 明确回复“可以打开 COM3”。该授权仅用于下一任务 `WB-HW-002`：

- 允许以 115200 8N1、DTR/RTS 均为 false **打开 COM3 一次**；
- 已知并接受该次打开可能触发一次 `CHIP_USB_UART_RESET`；
- 仅允许读取并保存启动日志，不得向串口写入任何字节；
- 不授权第二次打开或失败重试；如首次采集失败，WorkBuddy 必须停止并报告；
- 不授权 COM4/COM5/COM6、AT、JTAG、Flash 读取/写入/擦除、刷写、分区、Bootloader 或 OTA 操作。

## 2. 原始照片索引

| 文件 | 大小 (B) | 尺寸 | SHA-256 |
| --- | ---: | --- | --- |
| `photo-01-front-display.jpg` | 441,487 | 849 × 1132 | `aa06baaafb045693ca18a4e2b21c0bfe5b9ae5181d57a107a8828a9f029c83ac` |
| `photo-02-rear-camera-branding.jpg` | 338,623 | 797 × 1063 | `3faa7b648b555f86aa71d5eed50672e01967beeaeff9785b6bb3d17c4dcde35e` |
| `photo-03-bottom-usbc-openings.jpg` | 347,616 | 773 × 1031 | `2aafed4c134b50155878a7fe71281db141af60426623376e84f1c6d6ed10877d` |
| `photo-04-side-connectors.jpg` | 249,513 | 752 × 1003 | `08056ad67ebdde5c5c9984da2c0333bb02611c2942cfa4fe3b20e11a40f6fda0` |

## 3. 可见事实

| 照片 | 可确认事实 | 不能据此确认 |
| --- | --- | --- |
| 正面 | 屏幕已点亮，显示翻页时钟和中文日期/星期界面；底部 USB-C 线已连接 | 屏幕分辨率、面板/驱动型号、触摸型号及触摸功能 |
| 背面 | 可见 `CLOUD ZAO` 标识；左上角存在摄像头模组外观 | 摄像头传感器型号、像素、连通性或成像功能 |
| 底边 | 可见已连接的 USB-C 接口、左侧孔阵列、若干小圆孔和右侧长条开口 | 各开孔的扬声器、麦克风、卡槽或其他具体功能 |
| 侧边 | 可见两组内凹多针连接器开口 | 引脚定义、总线类型、用途及电气状态 |

上述结论统一标记为 `USER_PHOTO_CONFIRMED`。照片中没有可辨识的 SKU、序列号、板卡 revision 或板卡丝印标签，因此这些字段仍为 `USER_EVIDENCE_REQUIRED`，不得由商品外观、品牌标识或源码默认配置推断。

## 4. 调度结论

- `BLK-USER-EVIDENCE-001` 的“照片 + 一次 COM3 重启确认”条件已满足。
- 发布 `WB-HW-002`，由 WorkBuddy 完成一次受控启动日志采集和物理外观证据整理。
- `BLK-FLASH-AUTH-001` 保持有效；本记录不构成任何 Flash 或刷写授权。
