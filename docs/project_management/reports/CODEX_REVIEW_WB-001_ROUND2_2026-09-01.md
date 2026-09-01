# Codex 二次复检报告：WB-001 Claw4 平台实现映射

- 日期：2026-09-01
- 复检对象：远端 `workbuddy/wb-001-platform-map` @ `a829765432b69b12ce4bf6ff0e06dc9458372d57`
- 前序任务分支提交：`4a04b3cd70fdc645f6793bd56a6919abe4ac2313`
- 复检人：Codex
- 结论：`CHANGES_REQUIRED`
- 范围结论：`PASS`

## 1. Git 与范围复检

- `origin/workbuddy/wb-001-platform-map` 的远端 HEAD 与 WorkBuddy 回执一致，为 `a829765`。
- `a829765` 是 `4a04b3c` 的普通后继提交，提交范围只修改：
  - `docs/CLAW4_PLATFORM_MAP.md`
  - `docs/project_management/reports/WB-001_REPORT.md`
- `git diff --check origin/main...origin/workbuddy/wb-001-platform-map` 无输出。
- 本地主工作区出现的两个未跟踪交付文件，其 blob ID 与远端两文件完全一致；为避免受本地分支 ref 异常影响，本轮结论只依据远端 Git 对象。

## 2. 已通过的技术修订

### CR-WB001-01：显示 bpp 消歧 — PASS

- 默认 NV3051F 已写为 RGB888/24bpp，源码实际赋值位于 `metalio-claw-4.cc:277`、`:312`。
- FL7707N 已写为 16bpp，实际赋值位于 `metalio-claw-4.cc:392`。
- `config.h:36` 的 `LCD_BIT_PER_PIXEL (16)` 与默认 NV3051F 初始化 24bpp 的内部不一致已单独记录。
- 实机屏幕 SKU 继续保持 `DEVICE_VERIFY_REQUIRED`。

### CR-WB001-02：LVGL buffer 单位与防撕裂路径 — PASS（结论）

- 已明确 `lvgl_port_display_cfg_t.buffer_size` 的单位是像素。
- 已保留 `width * height * 50` 为可疑原始配置值，不再推算约 25.9MB PSRAM 占用。
- 已正确说明 P4 + `avoid_tearing=true` 时，有效 `buffer_size` 变为 `hres * vres`，并取得两个 DPI panel frame buffer。
- 真实分配量和运行稳定性已列为运行时/实机验证项。

### CR-WB001-03：电源实现区分 — PASS

- 已区分 CX25601N 充电控制、BQ27220 电量计及 USB charge status GPIO。
- `InitializeCx25601n()`、`cx25601n_set_ichg_ma()` 与板级构造调用均有对应源码证据。
- SY6970、ADC battery monitor、AXP2101 已降为通用或其他板卡实现。

### CR-WB001-04：4G 枚举与实际实例化消歧 — PASS

- 已明确设置值 1 / legacy `NetworkType::ML307` 表示 4G 槽位。
- 已明确当前源码实际构造 `Nt26Board`，未再写成当前默认使用 ML307 模组。
- 实机模组和启动网络路径继续保持 `DEVICE_VERIFY_REQUIRED`。

## 3. 必须修订：证据行号可追溯性

WB-001 的核心交付是可复查的平台证据映射，因此重要结论引用必须落到实际源码语句。

### CR-WB001-05：修正三组源码行号

只修订平台映射和 WorkBuddy 报告中的下列引用，不改变已经通过的技术结论：

1. 当前多处把 LVGL 防撕裂覆盖逻辑写为 `esp_lvgl_port_disp.c:256-262`，但该区间只是私有函数入口和局部变量声明。官方快照 `ca3aa3fa` 中：
   - 防撕裂分支完整区间为 `esp_lvgl_port_disp.c:317-325`；
   - ESP32-P4 的 `buffer_size = hres * vres` 与 `esp_lcd_dpi_panel_get_frame_buffer(..., 2, ...)` 位于 `:323-324`。
2. 当前把 FL7707N 16bpp 的实现证据写为 `metalio-claw-4.cc:355-376`，该区间没有包含实际 `bits_per_pixel` 赋值。应分别引用：
   - `:356`：`LCD_COLOR_PIXEL_FORMAT_RGB888`；
   - `:392`：`.bits_per_pixel = 16`。
3. BQ27220 的实际 `Begin(i2c_bus_)` 调用位于 `metalio-claw-4.cc:609`。若保留构造阶段上下文，可写成 `:606-610`，不得只用 `:606` 表示实际调用。

## 4. WorkBuddy 修订边界

只允许修改：

- `docs/CLAW4_PLATFORM_MAP.md`
- `docs/project_management/reports/WB-001_REPORT.md`

禁止修改任务看板、同步指令、Codex 报告、官方源码或任何其他文件。禁止 force push、rebase、reset、修改 `main` 或扩展到 `WB-002`。

完成后普通 push 到 `workbuddy/wb-001-platform-map`，返回本地/远端一致的提交号并停止。

## 5. 复检命令证据

```text
git fetch --prune origin
git rev-parse origin/main
git rev-parse origin/workbuddy/wb-001-platform-map
git ls-remote --heads origin main workbuddy/wb-001-platform-map
git diff --check origin/main...origin/workbuddy/wb-001-platform-map
git diff --name-status origin/main...origin/workbuddy/wb-001-platform-map
git diff 4a04b3c..a829765 -- docs/CLAW4_PLATFORM_MAP.md docs/project_management/reports/WB-001_REPORT.md
git -C vendor/MetalioClaw4 rev-parse HEAD
rg -n -C 8 "buffer_size|avoid_tearing|esp_lcd_dpi_panel_get_frame_buffer" vendor/MetalioClaw4/main/display/lcd_display.cc vendor/MetalioClaw4/managed_components/espressif__esp_lvgl_port/src/lvgl9/esp_lvgl_port_disp.c
rg -n -C 5 "bits_per_pixel|pixel_format|InitializeCx25601n|Bq27220Gauge|Nt26Board" vendor/MetalioClaw4/main/boards
```

当前官方源码仍为 `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950`，工作区干净。
