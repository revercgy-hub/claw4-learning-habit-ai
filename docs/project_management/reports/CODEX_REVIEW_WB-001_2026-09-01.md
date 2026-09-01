# Codex 复检报告：WB-001 Claw4 平台实现映射

- 日期：2026-09-01
- 复检对象：`workbuddy/wb-001-platform-map` @ `dbdc691`
- 复检人：Codex
- 结论：`CHANGES_REQUIRED`
- 范围结论：PASS，仅新增任务允许的两个文件

## 1. 已通过项

- 12 类平台子系统均已覆盖。
- ESP-IDF、CPU、PSRAM、Flash、C5/ESP-Hosted、摄像头和分区配置均能追溯到源码或 `sdkconfig`。
- Flash mode 冲突、非对称 `ota_0`/`ota_1`、应用固件无法放入 `ota_1` 等风险记录正确。
- 未把未连接实机的硬件能力标记成实机已确认。
- Git diff 仅包含 `docs/CLAW4_PLATFORM_MAP.md` 与 `WB-001_REPORT.md`，未触碰官方源码、工具链、构建产物或日志。

## 2. 必须修订的问题

### CR-WB001-01：显示摘要把默认面板写成 16bpp/RGB565

当前摘要写“720×720、RGB565 16bpp”，但默认宏选择 NV3051F；`metalio-claw-4.cc` 明确默认 NV3051F DPI 为 RGB888、`bits_per_pixel = 24`。FL7707N 备选路径才是 `bits_per_pixel = 16`。

修订要求：

- 摘要、总表和显示章节统一写明“NV3051F 默认：RGB888/24bpp；FL7707N 备选：16bpp”。
- `config.h` 的 `LCD_BIT_PER_PIXEL (16)` 与默认 NV3051F 初始化值存在差异时，作为源码内部不一致单独记录，不得用它覆盖当前初始化路径。
- 实际面板 SKU 继续保持 `DEVICE_VERIFY_REQUIRED`。

### CR-WB001-02：LVGL 缓冲区的单位和有效路径解释错误

`lcd_display.cc` 的 MIPI 配置确实写了 `buffer_size = width * height * 50`，但 `lvgl_port_display_cfg_t.buffer_size` 的单位是像素，不是字节。更关键的是当前调用 `lvgl_port_add_disp_dsi(... avoid_tearing=true)`；ESP32-P4 防撕裂路径会把局部变量 `buffer_size` 覆盖为 `hres * vres`，并取得两个 DPI panel frame buffer。因此“约 25.9MB、需要 PSRAM”的计算和推断不成立。

修订要求：

- 原样记录 `width * height * 50` 是可疑配置值。
- 解释在当前 P4 + `avoid_tearing=true` 路径中，有效 buffer size 被覆盖为 720×720 像素并复用两个 panel frame buffer。
- 不推算未经源码证明的实际 PSRAM 占用；把真实分配量与运行稳定性列为实机/运行时验证项。

### CR-WB001-03：当前充电控制器实现遗漏 CX25601N

板级构造函数调用 `InitializeCx25601n()`，`ichg_ma` 500/1000mA 配置实际传给 `cx25601n_set_ichg_ma()`。BQ27220 是电量计。SY6970、ADC battery monitor 和 AXP2101 是通用目录中的其他实现，不能替代当前板级活跃路径的描述。

修订要求：

- Power 总表和章节明确区分：CX25601N 充电控制、BQ27220 电量计、USB charge status GPIO。
- SY6970/ADC/AXP2101 只列为仓库通用或其他板卡实现。
- 芯片是否与实机一致继续标记 `DEVICE_VERIFY_REQUIRED`。

### CR-WB001-04：4G 类型名与实际实现需要消歧

`DualNetworkBoard` 的枚举名仍是 `ML307`，默认值 1 选择 4G；但 `METALIO_CLAW_4` 使用五参数构造函数，`InitializeCurrentBoard()` 在该分支实际实例化 `Nt26Board`，ML307 实例化代码已注释。

修订要求：

- 写成“设置值 1 / legacy `NetworkType::ML307` 表示 4G，Claw4 当前源码实际实例化 `Nt26Board`”。
- 不写成“当前默认使用 ML307 模组”。
- 实机模组与实际启动网络仍为 `DEVICE_VERIFY_REQUIRED`。

## 3. Git 元数据修订

并发切换分支导致 WorkBuddy 报告中的 `ea5b691` 不再是当前任务分支提交。正确的任务分支提交为 `dbdc691`，父提交为项目基线 `0f97af4`。报告中的提交号应改成“以任务分支 HEAD 为准”，或在下一次提交说明中记录父提交和前一提交，避免把包含自身内容的提交号写成固定值。

异常独立根提交已保存在 `backup/wb-001-orphan-race`，不影响当前任务分支和文件内容。

## 4. WorkBuddy 修订边界

只允许修改：

- `docs/CLAW4_PLATFORM_MAP.md`
- `docs/project_management/reports/WB-001_REPORT.md`

不得修改看板、Codex 复检报告、官方源码、配置、分区表或其他任务文件。完成 CR-WB001-01～04 后提交并停止，等待 Codex 二次复检。

## 5. 二次验收条件

- 四项内容问题均修订并有对应源码证据。
- 报告的自检、风险和验证结果与修订后的平台映射一致。
- 分支相对 `main` 仍只修改两个允许文件。
- `git diff --check main...HEAD` 无错误。
- 不新增任何实机已确认结论。
