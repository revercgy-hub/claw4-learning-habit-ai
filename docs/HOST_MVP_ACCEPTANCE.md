# Claw4 Host-MVP 验收清单（HOST_MVP_ACCEPTANCE）

- 工作流：WB-STREAM-002（CP0~CP8 主机侧 MVP 闭环连续开发）
- 分支：`workbuddy/domain-offline-stream`
- 日期：2026-09-02
- 证据输出目录：`out/host-tests/`（C++）、`out/cp8/`（多轮稳定性与扫描）、`backend/`（pytest 产物在进程内）

> 本清单**严格区分** `HOST_VERIFIED`（在 Windows 主机真实编译/执行验证）、
> `HARDWARE_VERIFY_REQUIRED`（只能在真机上验证，本流未做且不宣称完成）与
> 未实现项。本流**未**宣称 MVP 真机闭环或 Release 完成。

## 1. 验收总览

| 层 | 结论 | 证据 |
| --- | --- | --- |
| 领域状态机 reducer | `HOST_VERIFIED` | C++ host 单测 27/27，真实编译链接运行 |
| transactional outbox | `HOST_VERIFIED` | 21/21；故障注入/重启/ACK/死信；5 轮稳定 |
| 应用协调器 | `HOST_VERIFIED` | 20/20（commit-then-publish/缓存/同步策略/退避） |
| UI Presenter（四页） | `HOST_VERIFIED`（纯映射） | 28/28；零 LVGL/硬件头 |
| 持久化家庭后端 | `HOST_VERIFIED` | backend pytest 62/62（SQLite 本地文件，PostgreSQL 配置静态提供） |
| 家长 PWA | `HOST_VERIFIED`（构建级） | typecheck/lint/vitest 30/build/audit 全 PASS |
| 主机端 MVP 闭环 | `HOST_VERIFIED`（in-process E2E） | e2e 编排 PASS ×5 轮 |
| 真机/设备/固件/网络 | `HARDWARE_VERIFY_REQUIRED` | 见 §3 |
| 浏览器级 PWA 真机预览 | 未实现（本流不宣称） | 仅 production build + 组件测试 |

## 2. 主机验收证据（HOST_VERIFIED）

### 2.1 C++ 主机全量（本机真实 compile/link/run）

- 工具链：w64devkit v2.9.1（MinGW-w64，GCC 16.2.0），`E:\workbuddy\toolchains\`（仓库外未提交）
- 编译器版本：`g++ (GCC) 16.2.0`；P4 交叉编译器 `riscv32-esp-elf-g++ 14.2.0`（仅 -fsyntax-only 接口契约）
- 结果：**连续 5 轮 0 失败**（`out/cp8/cpp_5runs.txt`：ROUND1~5 EXIT=0）
- case 总数：domain 27 + outbox 21 + coordinator 20 + presenter 28 = **96**（4 个测试程序各自 exit 0）
- P4 接口契约：`verify-interface-contracts.ps1` exit 0（16 头 + contract_tests 交叉编译）
- 依赖扫描：learning_domain / ui 禁止 include 扫描 PASS（零硬件头）

### 2.2 Backend（FastAPI + SQLAlchemy）

- pytest 全量 **62/62**（旧设备契约 28 + 家庭后端 30 + E2E 闭环 2 + schema drift 2）
- **连续 5 轮 0 失败**（`out/cp8/backend_5runs.txt`：ROUND1~5 EXIT=0）
- `pip check`：No broken requirements found
- 数据库：本地 SQLite（pytest 会话级临时文件）；PostgreSQL 仅 Compose/Dockerfile 静态配置（本机无 Docker，容器未运行——不宣称）
- 家长 API：Dashboard/今日任务 CRUD/学习记录/设备列表，逐请求家长归属校验
- 事件投影：Task/StudySession read-model 与事件落库、ACK 同事务；响应丢失重发不重复统计

### 2.3 家长 PWA

- typecheck / lint / vitest / production build **连续 3 轮 0 失败**（`out/cp8/pwa_3runs.txt`）
- vitest：30/30（4 文件）；`npm audit --omit=dev`：0 vulnerabilities
- 服务端/构建仅绑 127.0.0.1；SW 只缓存静态壳，`/api/*` network-only

### 2.4 主机 MVP E2E 闭环

- 编排：`tools/dev/run-host-mvp-e2e.ps1`（C++ gate → backend pytest → PWA typecheck/test/build）
- **连续 5 轮 0 失败**（`out/cp8/e2e_5runs.txt`）
- 全链路场景（test_host_loop）：家长建任务 → 合成设备注册/配对/认证/拉取 → 事件同步（含一次响应丢失重发 duplicate + ACK 收敛）→ 家长 Dashboard/学习记录恰一次完成事实 → 第二家庭全隔离
- 边界声明：Python in-process（FastAPI TestClient）与真实 C++ host 测试证明；**不是** C++ HTTP/TLS 客户端、真机或浏览器 E2E

## 3. HARDWARE_VERIFY_REQUIRED（真机验证清单，本流未做）

以下只能在真机（Metalio Claw 4）验证，**本流保持 `HARDWARE_VERIFY_REQUIRED`**：

| 项 | 状态 |
| --- | --- |
| 屏幕/触摸/面板驱动（NV3051F 或其他） | `HARDWARE_VERIFY_REQUIRED`（设备实机确认） |
| Flash 容量与分区布局（含 OTA 槽位） | `HARDWARE_VERIFY_REQUIRED`（`BLK-FLASH-AUTH-001` 门禁） |
| 网络通道（Wi-Fi/4G 模组型号与驱动） | `HARDWARE_VERIFY_REQUIRED` |
| 音频/麦克风/摄像头 | `HARDWARE_VERIFY_REQUIRED` |
| NVS/文件系统适配器（outbox 落盘） | `HARDWARE_VERIFY_REQUIRED`（本流仅平台无关核心 + fake） |
| 设备 HTTP/TLS/CA 校验路径 | `HARDWARE_VERIFY_REQUIRED` |
| 电池/电源状态、外观 SKU | `HARDWARE_VERIFY_REQUIRED` |
| LVGL 真机页面渲染 | 未实现（本流 presenter 与 LVGL 解耦，页面逻辑 HOST_VERIFIED） |

## 4. 未实现项（不宣称完成）

- 真实设备固件构建/刷写、OTA、Secure Boot/Flash Encryption
- 浏览器内 PWA 与真实后端联动的真机/浏览器 E2E
- C++ 设备侧 HTTP/TLS 客户端、NVS 适配器、LVGL 适配层
- 生产级家长认证（当前为显式标注的单家庭 dev-session 桩）
- Voice/Camera/AI/4G/GPS/激励体系等非 MVP 功能

## 5. 门禁核对

- 硬件/串口/Flash/分区/Bootloader/OTA 操作：**0**
- vendor/BSP/固件修改：**0**
- 真实儿童数据/凭据/密钥/生产外部服务：**0**（全部合成 + dev-session 桩；`change-me` 等占位无真实值）
- force push / rebase / reset / 合并 main / 修改看板·任务包·Codex 报告·AGENTS.md：**0**
- 测试与手工服务仅 127.0.0.1 / in-process；构建产物、venv、node_modules、本地 DB 均未提交
