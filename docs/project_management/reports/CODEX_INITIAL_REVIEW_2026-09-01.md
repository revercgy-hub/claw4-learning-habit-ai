# Codex 项目初始复检报告

- 日期：2026-09-01
- 复检范围：项目规则、主机环境、官方源码基线、现有报告一致性、Git 边界、当前调度门禁
- 结论：**主机软件层通过；平台映射待补；真机 Bring-up 阻塞；MVP 实现保持 HOLD**

## 1. 已确认事实

| 检查项 | 结论 | 证据 |
| --- | --- | --- |
| Metalio 官方源码 | `main` @ `ca3aa3f`，工作树无已跟踪修改 | `vendor/MetalioClaw4` Git 状态与日志 |
| ESP-IDF | v5.5.4 | IDF 工具实际输出 |
| P4 编译器 | RISC-V GCC 14.2.0 | 编译器实际输出 |
| 构建工具 | CMake 3.30.2、Ninja 1.12.1 | 工具实际输出 |
| IDF Python | Python 3.12.13，requirements satisfied | Python 与依赖检查实际输出 |
| 官方基线产物 | `xiaozhi.bin` 9,036,192 bytes；Bootloader 20,416 bytes；partition table 3,072 bytes | `E:\b` 文件检查 |
| WorkBuddy 独立复核 | 主机软件层 READY，与 Codex 实测一致 | `docs/CLAW4_报告复核_2026-09-01.md` |
| 实机连接 | 未检测到串口 | `Get-CimInstance Win32_SerialPort` 返回 `NO_SERIAL_PORTS_DETECTED` |
| 项目根 Git | 复检前不存在 | 根目录 `git status` 返回非 Git 仓库 |
| 第三方 Git | Metalio 与 ESP-IDF 各有独立 Git 历史 | `.git` 目录定位结果 |

## 2. 报告冲突处理

`docs/系统检查报告_2026-09-01.md` 在 14:44 记录“工具链未安装”。该结论已经被后续安装、成功构建、`docs/CLAW4_主机准备情况报告_2026-09-01.md` 以及本次工具实测取代。

处理决定：

- 保留旧报告作为时间点证据；
- 在旧报告顶部标记 `SUPERSEDED`；
- 当前状态以本报告和主机准备情况报告为准；
- WorkBuddy 不得依据旧报告重新安装工具链。

## 3. 阶段与门禁判断

### G0 主机基线：PASSED

工具链、Python 依赖和基线构建产物存在，允许继续源码级平台映射。

### G1 平台证据：INCOMPLETE

已有 `CLAW4_AUDIT.md` 和 `HARDWARE_ASSUMPTIONS.md`，但总规划要求的 `CLAW4_PLATFORM_MAP.md` 尚不存在。当前应先完成 `WB-001`，再由 Codex 复检。

### G2 真机 Stage 1：BLOCKED

当前没有串口设备，不能执行 B001/B002/B003/B004/B005/B009/B013 的实机验证，也不能把硬件能力标为实机确认。

### G3 MVP 开发准入：HOLD

在 Bring-up 门禁通过前，不建立学习业务代码骨架，不进入 Home/Focus/Event Queue 实现。

## 4. Git 边界决定

项目根目录需要独立 Git 历史，不能把项目管理和业务变更提交到 Metalio 官方仓库。根仓库应：

- 纳入规划、脚本、业务源码和可审查报告；
- 排除 `vendor/`、`toolchains/`、本地构建、设备日志、密钥和 WorkBuddy memory；
- 在未提供项目远端 URL 前只进行本地提交；
- 禁止向 `https://github.com/CloudZao/MetalioClaw4.git` 推送本项目内容。

## 5. 当前调度决定

唯一 `READY` 任务：`WB-001`，补齐 `docs/CLAW4_PLATFORM_MAP.md`。

原因：

- 不依赖真机，可立即执行；
- 是产品总规划中 TASK 002 的缺失交付物；
- 为后续架构和实机验证提供准确路径；
- 范围可限制为只读审计和文档，不破坏官方基线。

完成后 WorkBuddy 必须停止，由 Codex 复检后再决定是否发布 `WB-002`。

## 6. 当前风险

1. 非对称 OTA 分区与当前应用尺寸存在明确不匹配风险，禁止先改分区表。
2. `sdkconfig` 的 Flash mode 相关值存在不一致迹象，需要记录但不能在映射任务中修复。
3. 真机 SKU 和外围设备只能由实机证据确认。
4. 项目远端尚未配置，因此当前只能完成本地 Git 同步。
