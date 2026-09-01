# WB-001：补齐 Claw4 平台实现映射

## 调度信息

- 负责人：WorkBuddy
- 复检人：Codex
- 优先级：P0
- 当前状态：`CHANGES_REQUIRED`
- 前置任务：AUD-001、BLD-001 已验收
- 目标分支：`workbuddy/wb-001-platform-map`

## 目标

基于当前官方源码快照和已经通过的主机基线，生成 `docs/CLAW4_PLATFORM_MAP.md`，准确回答 ESP-IDF、芯片配置、内存、Flash、分区、LVGL、显示、触摸、网络、音频、摄像头、SD、电源和 OTA 的实现位置、配置证据、复用边界和实机待验证项。

本任务只做证据映射，不修改官方源码、不写业务代码、不进行真机刷写。

## 开始前必须读取

按顺序完整读取：

1. `AGENTS.md`
2. `docs/project_management/TASK_BOARD.md`
3. `docs/project_management/WORKBUDDY_GIT_SYNC.md`
4. 本任务包
5. `docs/project_management/reports/CODEX_REVIEW_WB-001_2026-09-01.md`
6. `docs/project_management/reports/CODEX_REVIEW_WB-001_ROUND2_2026-09-01.md`
7. `项目总规划/AGENTS.md`
8. `项目总规划/CLAW4_BRINGUP_PROMPT.md`
9. `docs/CLAW4_AUDIT.md`
10. `docs/HARDWARE_ASSUMPTIONS.md`
11. `docs/BUILD.md`
12. `docs/CLAW4_主机准备情况报告_2026-09-01.md`
13. `vendor/MetalioClaw4/README.md`、`sdkconfig`、分区表及相关源码

## 允许修改

- `docs/CLAW4_PLATFORM_MAP.md`
- `docs/project_management/reports/WB-001_REPORT.md`

除上述两个文件外不允许修改。任务看板和 Codex 报告由 Codex 维护。

## 必须覆盖的映射

对每个子系统给出：

- 状态：`SOURCE_CONFIRMED`、`CONFIG_CONFIRMED`、`LIKELY` 或 `DEVICE_VERIFY_REQUIRED`；
- 关键配置项及当前值；
- 主要实现文件和入口函数/类；
- 与 Metalio BSP、ESP-IDF 或托管组件的关系；
- MVP 复用策略；
- 禁止修改或需用户授权的边界；
- 真机验证方法和预期证据。

子系统至少包括：

1. ESP-IDF 版本、target、P4 revision 和 CPU 配置；
2. internal RAM、PSRAM、Flash；
3. partition table、`ota_0`、`ota_1`、OpenClaw/ESPClaw 相关分区；
4. LVGL、Display、屏驱 SKU；
5. Touch；
6. ESP32-C5、ESP-Hosted、Wi-Fi；
7. Audio codec、麦克风、扬声器、AEC/VAD/Wake Word；
8. Camera；
9. SD / Storage / USB MSC 关联；
10. Power、Battery、Gauge；
11. OTA 和恢复边界；
12. 启动日志中应采集的验证字段。

## 必须明确记录的特殊项

- 不能把源码/配置存在写成真机工作正常。
- `sdkconfig` 中 Flash mode 选择项与字符串值若不一致，原样记录证据并标注风险，不修改。
- 记录 `partitions/v1/32m_dual.csv` 的实际大小和用途；不得把 `ota_0`/`ota_1` 当作对称 A/B OTA。
- 记录当前 `xiaozhi.bin` 大小与 `ota_1` 容量不匹配的风险。
- 没有串口和实机证据的项目统一标记 `DEVICE_VERIFY_REQUIRED`。
- 不根据电商描述推断硬件。

## 建议的只读核查命令

```powershell
git -C .\vendor\MetalioClaw4 status --short --branch
git -C .\vendor\MetalioClaw4 rev-parse HEAD
rg -n "IDF_TARGET|ESP32P4|PSRAM|FLASH|PARTITION_TABLE|LVGL" .\vendor\MetalioClaw4\sdkconfig
Get-Content -LiteralPath .\vendor\MetalioClaw4\partitions\v1\32m_dual.csv
rg -n "NV3051F|FL7707N|touch|esp_hosted|wifi|camera|audio|BQ27220|SD|OTA" .\vendor\MetalioClaw4\main .\vendor\MetalioClaw4\components
```

命令可按需要拆分，但不得运行格式化、构建、下载、安装、刷写或自动修复命令。

## 交付物结构

`docs/CLAW4_PLATFORM_MAP.md` 至少包含：

1. 结论摘要；
2. 版本与配置基线；
3. 子系统映射总表；
4. 各子系统的实现路径与证据；
5. 可复用、需封装、禁止修改三类边界；
6. `DEVICE_VERIFY_REQUIRED` 清单；
7. Bring-up Stage 1 的证据采集建议；
8. 风险和未决问题。

任务报告使用 `docs/project_management/templates/WORKBUDDY_REPORT_TEMPLATE.md`，保存为 `docs/project_management/reports/WB-001_REPORT.md`。

## 验收标准

- [ ] 两个交付文件均存在，且没有修改其他文件。
- [ ] 12 类平台信息全部覆盖。
- [ ] 每个重要结论能追溯到具体文件、配置项或已验证报告。
- [ ] 源码事实、配置事实、推测、实机事实分级清楚。
- [ ] Flash mode、非对称 OTA、固件尺寸风险均明确记录。
- [ ] 未把任何未连接实机的能力写成 `CONFIRMED`。
- [ ] Git diff 不包含 `vendor/`、`toolchains/`、构建产物或日志。
- [ ] 报告中附带只读核查命令和结果摘要。

## 停止条件

遇到以下情况立即停止并提交 `BLOCKED` 报告：

- 需要修改官方源码才能继续；
- 证据互相冲突且无法由当前源码解释；
- 需要实机、串口或刷写才能得出结论；
- 根仓库或任务分支状态异常，可能覆盖他人变更；
- 任务范围需要扩展到架构或业务实现。

## 完成后的明确指令

按 `docs/project_management/WORKBUDDY_GIT_SYNC.md` 的交付流程提交并推送上述两个文件后停止。不要开始 `WB-002`、真机 Bring-up 或任何 MVP 代码。等待 Codex 复检。
