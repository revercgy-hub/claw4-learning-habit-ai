# Codex 复检报告：WB-HW-002 Round 2

- 日期：2026-09-01
- 复检对象：远端 `workbuddy/wb-hw-002-controlled-boot` @ `9fe38c318624afd9317303b85e7033f7df7d824f`
- 初次实施提交：`63281b23c85433e184660517e8ecaeddabff94c7`
- Codex 复检同步提交：`20a617c54609545337ffb865f6cb535c391ae67f`
- 复检人：Codex
- 结论：`ACCEPTED`
- G2 真机 Stage 1：`INCOMPLETE`（本结论只验收 WB-HW-002）

## 1. Git 与范围

- 修订提交 `9fe38c3` 是复检同步提交 `20a617c` 的直接后继。
- 相对 `origin/main` 仍只有原任务四个允许文件。
- `git diff --check origin/main...origin/workbuddy/wb-hw-002-controlled-boot` 无输出。
- 四个本地交付文件的工作树 blob、index blob 与远端任务分支 blob 完全一致。
- 原始日志、照片和过程记录均在仓库外，未进入 Git。

## 2. CR 项复检

### CR-WBHW002-01：PASS

- “扬声器、麦克风、SIM/SD、Pogo Pin、磁吸、单摄、金属材质”等照片推断已从四份交付物删除。
- 物理文档统一改为小圆孔阵列、细长开口、圆形凸起、内凹多针连接器开口和颜色/反光等纯形态事实。
- 所有功能、材料、总线、引脚和传感器型号仍标 `UNKNOWN`。

### CR-WBHW002-02：PASS

- `Initialize WiFi board` 仅表述为 P4 应用日志执行到该行；Wi-Fi 初始化/连接、实际网络通道和 4G 状态均为 `UNKNOWN`。
- 两轮 compile time/ELF hash 只记录为运行镜像差异；更新来源、方式、时间、授权和启动分区均为 `UNKNOWN`，不再声称发生过刷新。
- UUID 只记录本次值；生成方式、持久性和身份语义均为 `UNKNOWN`。
- 稳定性结论收敛为“90.17 秒内未观察到第二次复位/启动签名”，未扩展到显示、触摸、网络或业务功能。

### CR-WBHW002-03：PASS WITH PROCESS_LIMITATION

- 报告占位符和“待校验/预期输出”已清除，初次交付提交与实际 Git 复验结果已补齐。
- `precheck_pnp.txt` 的前置过滤查询未命中、后续精确 InstanceId/Win32_SerialPort 查询成功的差异已经如实解释。
- 新增仓库外 `capture_method_record.md`：3,366 B，SHA-256 `83615a9bb8e209f3c25741a8cbe3b1bb25ff9ac1027968a3feb1fbdc3abd403c`。
- WorkBuddy 明确声明该文件是事后方法记录，不是原始执行脚本；没有伪造原始命令，也没有重新打开 COM3。
- 因原始脚本未保存，`Open()=1` 和零 `Write*` 的代码级证据仍只能依赖 `capture_log.txt` 与实施声明。本任务接受该过程限制；未来串口任务必须在操作前先把确切脚本保存到证据目录并计算哈希。

### CR-WBHW002-04：PASS

- 修订只改原四个交付文件，提交信息符合要求，使用普通快进推送。
- 三份原采集证据的大小、时间和哈希保持不变；没有新增串口或硬件采集。

## 3. 最终确认事实

- COM3 PnP 身份：Espressif `VID_303A&PID_1001&MI_00`，USB JTAG/serial debug unit。
- 本次原始日志中恰好一组 ROM/reset/boot 签名：`CHIP_USB_UART_RESET` 后正常 `SPI_FAST_FLASH_BOOT`。
- 90.17 秒采集内没有第二次复位/启动签名或下载模式文本。
- 日志确认 P4 eco2、chip rev v1.3、双核 360 MHz、32 MB PSRAM @ 200 MHz、GD Flash/qio、固件内 `metalio-claw-4` 标签和执行至 Wi-Fi board 初始化行。
- 屏驱、触摸、Flash 容量、C5连接、4G 状态、partition/OTA 实际布局和照片中各开孔/连接器功能仍未确认。

## 4. 验收与后续门禁

- `WB-HW-002` 标记 `ACCEPTED`。
- 允许发布 `WB-002` 架构任务；该任务是开发前架构设计，只输出文档，不写产品源码。
- G2/G3 尚未通过，MVP 业务实现、固件修改和真机写入继续 `HOLD`。
- `BLK-FLASH-AUTH-001` 保持有效。
