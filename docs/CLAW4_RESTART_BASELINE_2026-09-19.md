# Claw4 学习伙伴 — 项目重启基线（2026-09-19）

> **这份文档是什么**：把「项目目标 + 当前系统实况 + 外部参考项目」三块**只读事实**汇总成一份独立输入，
> 用于**重新研究开发路径**（尤其：是否/如何把基线切到 ESP-IDF 6.0.2）。
>
> **怎么用**：本文只陈述事实与已确认约束，**不含路径决定**。§6 列出待研究问题。
> 与历史文档冲突时，以本文的"实测"级证据为准；历史文档不删除，但按根 `AGENTS.md` §2 规则标 `SUPERSEDED`。
>
> **权威顺序**（沿用根 `AGENTS.md` §2）：用户最新指令 > `AGENTS.md` > `TASK_BOARD.md` > 任务包 > 验收报告 > 总规划 > 历史报告与 memory。
> ⚠️ **注意**：`docs/project_management/TASK_BOARD.md` 头部停在 2026-09-14，把 `A05-DEVICE` 记为 `HOLD`，
> 与 §2.6 的实际进展**已不一致**。看板需要刷新，本文 §2.6 是实际状态。

---

## 1. 项目目标

### 1.1 产品定位

**Metalio Claw 4 学习伙伴** —— 一台面向学龄儿童的桌面 AI 学习习惯养成终端。

- **硬件**：ESP32-P4 主控 + ESP32-C5 网络协处理器，720×720 触摸屏，双麦/扬声器，摄像头，SD，电池。
- **核心闭坏**：`学期目标分解 → 每日任务 → 计时专注（Start/Pause/Resume/Complete）→ 错题归档 → 每日/每周复盘 → 家长可见`
- **价值主张**：不是"又一个 AI 聊天玩具"，而是**有状态权威的学习任务系统**。语音/AI 是**增强层**，不是依赖。

### 1.2 三层技术路线（V4 定义，仍有效）

```text
78/xiaozhi-esp32                        ← 语音 / MCP / 协议 / 音频 / 安全 上游
      │  selective backport（禁整仓 merge）
      ▼
CloudZao/MetalioClaw4                   ← 唯一真机硬件基线（P4+C5 深度定制）
      │  stable platform + thin adapter
      ▼
Claw4 Learning Product                  ← 本项目
      ├─ Learning Core（业务真相）
      ├─ Interaction Router
      ├─ Learning MCP
      ├─ Offline / Sync / Recovery
      ├─ Learning UI（LVGL presenter）
      ├─ Backend（FastAPI）
      └─ Parent PWA
```

### 1.3 六条固定原则（V4 §1）

1. **MetalioClaw4 是唯一真机硬件基线。**
2. **XiaoZhi 只作为 Voice / MCP / Protocol / Audio / Security 上游**，不作为主仓。
3. **Learning Core 是业务真相，不依赖 XiaoZhi、LVGL、ESP-IDF**（纯 C++17，可 Host 单测）。
4. **真机尽早介入，但日常只做应用级烧录**（app-flash 仅 `ota_0`）。
5. **底层基础设施不随普通功能开发一起改动。**
6. **模拟器是效率工具，不是刷机安全的强制前置。**

### 1.4 能力分层（L0–L6 与目标 P 阶段）

| 阶段 | 内容 | 目标 |
|---|---|---|
| **L0** | Learning App Shell：Home 加 Learning 图标、进出、Mock Task + Start | P18–P21 |
| **L1** | Core Loop：真 Task + Start/Pause/Resume/Complete + Presenter/Domain/Coordinator | P22 |
| **L2** | Persistent Offline：NVS/Storage adapter、Outbox、reboot recovery | P23 |
| **L3** | Network：Backend、Auth、Today Tasks、Event sync、Parent PWA | P24 |
| **L4** | Voice Command：STT + 确定性映射 + Voice overlay | P25 |
| **L5** | Learning MCP：XiaoZhi MCP 工具 + 完成确认 | P26 |
| **L6** | AI Coach：任务拆解、讲解、日/周复盘、家长摘要 | P27 |

**当前实际位置**：L0/L1/L2/L3 的主机侧与真机侧**均已走过一轮**（见 §2.6），
但 L3 的真实后端同步往返**刚刚查出并修复了一个阻塞性缺陷**（见 §2.7）。

### 1.5 关键设计决策（已定，不建议推翻）

| 决策 | 内容 |
|---|---|
| **离线优先** | 即使 XiaoZhi / ASR / LLM / Backend / Wi-Fi 全不可用，仍必须能：缓存今日任务 → Touch Start → Focus → Pause/Resume → Complete → 本地 StudySession → Outbox → 后续同步 |
| **交互统一入口** | Touch / STT / MCP **全部**汇入同一 `Interaction Router → Command Dispatcher`，业务逻辑不得散落在触摸回调、MCP 回调或 STT handler 里 |
| **AI 不拥有状态权威** | `learning.request_complete_task` → `AwaitingConfirmation` → **用户明确确认** → `ConfirmCompleteTask` → Domain Complete。**不提供无保护的 AI 自动完成工具** |
| **Learning App 懒加载** | 不在 Board constructor / `app_main()` / 早期 boot 里做大量 Learning 初始化；从 Home 点进才 `Load()` |
| **初期用原厂 Home 原生 App** | Learning 作为 Home 网格里的原生 Screen，与其他 App（Chat/OpenClaw/Settings/Test）并列，**不改开机主链路**，保留原厂可作对照组 |
| **平台无关 Core** | Learning Domain **不允许** include Metalio / ESP-IDF / LVGL；平台差异全部走 Ports + Adapter |

### 1.6 明确不做 / 永久禁区

**MVP 阶段不开发**：持续摄像、情绪/人脸识别、本地大模型、复杂数字人、4G/GPS。

**普通开发永久禁区**（没有独立基础设施任务 + 用户明确授权，一律禁止）：

```
burn eFuse          Secure Boot          Flash Encryption
disable ROM download / disable JTAG      改 partition table
改 bootloader       重写 ota_1           erase_flash / mass erase
替换 C5 固件
```

> ⚠️ **`ota_1` 不是空槽**：它被官方用于 **ESPClaw 本地 edge_agent**。改分区表还会丢掉
> `emote` / `system` / `storage` 等分区语义，是**架构级不可逆动作**。

---

## 2. 当前系统基本情况

### 2.1 硬件事实（实测 / 源码 / 未知 三级分离）

| 项 | 结论 | 等级 | 来源 |
|---|---|---|---|
| 主控 | **ESP32-P4 rev v1.3**（实测）、RISC-V 双核、默认 360 MHz | **CONFIRMED_DEVICE** | D0-A `flash_id` |
| Flash | **32 MB / GigaDevice**（实测）、40 MHz | **CONFIRMED_DEVICE** | D0-A |
| MAC | `80:f1:b2:d2:ed:14` | **CONFIRMED_DEVICE** | D0-A |
| 串口 | **COM7 = USB-Serial/JTAG**（VID_303A）；备用 **COM6 = CH340/UART0** | **CONFIRMED_DEVICE** | 端口枚举 |
| 网络协处理器 | ESP32-C5，经 **ESP-Hosted SDIO slot 1 / 4-bit / 40 MHz** | CONFIRMED_SOURCE | sdkconfig |
| PSRAM | 已启用、HEX 模式、200 MHz | CONFIRMED_SOURCE | sdkconfig |
| 显示 | 720×720、MIPI-DSI 2-lane；**NV3051F 默认（RGB888/24bpp）**、FL7707N 备选（16bpp） | SOURCE_CONFIRMED + SKU 待定 | 板级实现 |
| 触摸 | GT911，I2C（SDA=GPIO7 / SCL=GPIO8），INT=GPIO33 | SOURCE_CONFIRMED | 板级实现 |
| 音频 | 16 kHz 全双工，I2S（MIC WS=10/DIN=11；SPK DOUT=9/BCLK=12）；AEC **默认关** | SOURCE_CONFIRMED | 板级实现 |
| 摄像头 | OV2710，MIPI CSI，1080p@25fps（配置） | CONFIRMED_SOURCE，**实际型号 UNKNOWN** | sdkconfig |
| SD | SDMMC slot 0，4-bit；**热插拔能力未确认** | SOURCE_CONFIRMED | 板级实现 |
| 电源 | CX25601N 充电控制 + **BQ27220 电量计** + USB_CHG_STA(GPIO53) | SOURCE_CONFIRMED | 板级实现 |
| 麦克风/喇叭/Codec 型号 | **UNKNOWN** | — | 无可靠证据 |
| 官方完整恢复镜像 | `Metalio_Claw4_Latest.bin`，33,120,256 B，SHA-256 前缀 `1a69e379` | SOURCE_CONFIRMED | 官方仓库 |

### 2.2 分区表与尺寸账本（当前唯一硬约束之一）

当前使用官方 `partitions/v1/32m_dual.csv`（**非对称双槽**），offset `0x9000`：

| Name | Type | SubType | Offset | Size | 用途 |
|---|---|---|---|---|---|
| nvsfactory | data | nvs | 0xA000 | 200K | 出厂 NVS |
| nvs | data | nvs | — | 840K | 应用 NVS |
| otadata | data | ota | 0x10E000 | 8K | OTA 状态 |
| phy_init | data | phy | — | 4K | PHY 校准 |
| model | data | spiffs | — | 956K | 模型（esp-sr srmodels） |
| **ota_0** | app | ota_0 | **0x200000** | **9 MiB (9,437,184 B)** | xingzhi 主固件槽 |
| **ota_1** | app | ota_1 | 0xB00000 | **4 MiB (4,194,304 B)** | **ESPClaw edge_agent（官方占用）** |
| resources | data | spiffs | — | 4M | 资源 |
| factory_test | data | spiffs | — | 600K | 工厂测试 |
| emote | data | spiffs | — | 4M | 表情资源 |
| system | data | fat | — | 1M | 系统数据 |
| storage | data | fat | — | 7M | 用户存储 |
| coredump | data | coredump | — | 64K | 崩溃转储 |

**尺寸账本（冻结候选实测）**：

```
app (xiaozhi.bin)  = 9,271,760 B   (8.84 MiB)
ota_0 槽位          = 9,437,184 B   (9.00 MiB)
余量                =   165,424 B   (161.5 KiB / 1.75%)   ← 极紧
ota_1 槽位          = 4,194,304 B   (4.00 MiB)  <  app  ⇒ 装不下
```

**两个结构性结论**：
1. **双槽 OTA 事实上不可用** —— app（8.84 MiB）放不进 `ota_1`（4 MiB）。官方设计里 `ota_1` 本来就给了 ESPClaw edge_agent，不是给 xiaozhi 做 A/B 的。
2. 因此当前是**两头都亏**：付了 4 MiB 的槽位代价，却没换到任何可用的回滚能力。

**参考对照（另一条同源项目的事实，供研究，非本项目现状）**：
同源项目 `AlayaElla/MetalioClaw4-AgentUI` 真机在跑的是**单槽** `factory app @0x200000 size 14M`
（+ `nvsfactory 200K / nvs 840K / phy_init 4K / model spiffs 956K / resources spiffs 15M / factory_test 600K`）。
`9 + 4 + otadata ≈ 14` ⇒ **空间总量相同，只是切法不同**。若采单槽，余量可从 165 KB 升到约 5.1 MiB。
**代价两笔**：① 改分区表 ⇒ 必须**全量刷 `0x0`** ⇒ **NVS 全丢**（Wi-Fi 凭据 / 学习配置 / MQTT 凭据需重配 + 重新 provisioning）；
② 单槽下**厂商云 OTA 结构上不可能**（`application.cc:179` 会在 `HasNewVersion()` 时自动升级，但无处可写）。

### 2.3 软件架构与仓库结构

**上游关系（重要）**：本项目**不是 fork**，而是**补丁层 + 叠加源码**。

```
pin 死的上游：  CloudZao/MetalioClaw4 @ ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950
                （本地只读：E:/workbuddy/学习习惯培育AI/vendor/MetalioClaw4）
      +
补丁层：        integration/metalio_claw4/patches/
                ├─ project-ca3aa3fa.patch                  （历史 #1–#6 + AF3/APP2 四文件集成取证）
                ├─ a05-0001-cmake-source-registration.patch（CMake SOURCES 追加 5 个实现单元）
                └─ integration_manifest.md                  （应用顺序不可颠倒）
      +
业务源码：      firmware/main/**        （108 文件，平台无关 C++17）
                integration/metalio_claw4/**（设备胶水、补丁、候选清单）
      ↓ 按 allowlist 映射复制
隔离构建树：    E:\a05c\s  （镜像树） → 构建输出 E:\a05c\b
```

**业务代码分层**：

| 目录 | 职责 | 平台耦合 |
|---|---|---|
| `firmware/main/learning_domain/` | 领域模型 / reducer（业务真相） | **零** |
| `firmware/main/application/` | `AppCoordinator`（同步编排） | **零** |
| `firmware/main/sync/` | Outbox / wire codec / 后端会话 / 传输 | **零** |
| `firmware/main/interaction/` | Command / Intent / Router / Dispatcher / STT mapper | **零** |
| `firmware/main/mcp/` | Learning MCP Host | **零** |
| `firmware/main/ui/` | presenter（纯映射） | **零** |
| `firmware/main/ports/` | Port 接口定义 | **零** |
| `firmware/main/time/`、`reminder/`、`telemetry/`、`assistant/` | 时间权威 / 提醒 / 遥测 / 助手 | **零** |
| `firmware/tests/` | host / unit / contracts / fakes | — |
| `integration/metalio_claw4/device/**` | **设备 TU**（FreeRTOS / IDF） | **高（不进 Host Gate）** |
| `integration/metalio_claw4/host_glue/` | 主机胶水 | 低 |

> ⚠️ `integration/metalio_claw4/device/**` 是 IDF/FreeRTOS TU，**不被 Host 门禁编译**。
> 团队已固化一条架构约定：并发/所有权规则**不写在设备 TU 里**，抽成极小纯 C++ helper 放 `firmware/main/sync/`；
> 全仓唯一锁顺序 = `state mutex → SessionLeaseHolder mutex`。

**后端与前端**（同仓库内）：`backend/`（FastAPI）、`frontend/`（家长 PWA）、`deploy/`、`docker-compose.yml`。

### 2.4 构建与工具链（现状）

| 项 | 值 |
|---|---|
| ESP-IDF | **v5.5.4**（`E:/workbuddy/esp-idf-5.5.4-ascii`） |
| tools | `E:/workbuddy/claw4-idf-tools`（`IDF_TOOLS_PATH`） |
| Python env | `python_env/idf5.5_py3.12_env`（**Python 3.12.14**） |
| 交叉编译器 | `riscv32-esp-elf-gcc **14.2.0**`（`esp-14.2.0_20260121`） |
| native 编译器 | `E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe` |
| 目标 | `esp32p4`；LVGL 9.3.0；esp_lvgl_port 2.6.3 |
| 隔离树 | `E:\a05c\s`（源码）/ `E:\a05c\b`（构建产物）——**目录名必须短** |

**构建 recipe 要点**（必须记住的坑）：
- `ESP_IDF_VERSION` **必须写 `5.5`**，写 `5.5.4` 会丢 `esp_wifi_remote` Kconfig 变体 ⇒ **丢 Wi-Fi 符号**。
- 必须剥离 `PYTHONPATH` 与 `CODEBUDDY_SAFE_DELETE_*`；PATH 前置 4 项。
- **Windows CreateProcess 命令行上限 32,767**：本项目长命令有 379 段 `-I`、其中 212 段带源码树前缀
  ⇒ **树路径每 +1 字符 ≈ 命令行 +212**。报"构建莫名失败"**先量 `len(command)`**。

### 2.5 门禁体系（当前质量保障）

| 门禁 | 内容 | 现状 |
|---|---|---|
| Host 全量门禁 | `tools/dev/verify-host-cpp-tests.ps1` | **30 suites PASS** + interface contracts |
| 交叉语法门禁 | `tools/dev/verify-interface-contracts.ps1` | 由 Host 门禁第 5 步自动调用 |
| 构建 runner | `tools/dev/run-a05-cp2-build.ps1` | 退出码 `0/2/3/4/5/n`；**含 P0 PATH 守卫 + 源码树绑定守卫 + 构建后验证** |
| 构建输入冻结 | `tools/dev/verify-build-inputs.py` + `integration/metalio_claw4/a05_build_input_manifest.json` | manifest 是**生成产物**，手改会被 `--write` 静默覆盖 |
| 分区校验 | `tools/dev/verify-partition-table.py` | 分区必须**从生成物反解**，不能读源码 |

> ⚠️ 跑门禁时 PATH **必须同时含两套工具链**并传 `-CrossCompilerPath`；只放 w64devkit 会让第 5 步 **假 FAIL**。

### 2.6 已完成里程碑（实际状态）

| 里程碑 | 结果 | 提交 |
|---|---|---|
| **A05-BUILD**（M0 候选冻结） | ✅ Codex 已正式验收，**已封档** | `420d0df` |
| **A05-DEVICE**（D0/D1/D2 实机 bring-up） | ✅ 全 PASS，已推送 | `d5e64c5` |
| **A05-DEVICE-T1**（学习后端真实同步往返） | ❌ **NOT PASSED** → 分类 `ACK_PIPELINE_FAIL` → 根因已定位并**已修复**（Host 门禁 30/30），**尚未冷构建/重刷/复测** | `1a3d3fe` / `b10dfde` / `7ac29ad` |

**冻结候选**：`claw4-v53-m0-a05-2c8f58f`
```
source   = 2c8f58f53506402918284c695100f007798433d8
app      = 9,271,760 B
sha256   = c035e1c09ebe472f5f14b490c93844aa3e298cd11b4e30f7fe1055e5b9278d36
elf      = 80,102,764 B（sha256 前缀 fba68d58…）
身份铁证 = 日志 "App version 2.0.51 / Compile time Sep 15 2026 17:59:12 / ELF fba68d580…"
```

**实机验证结论（D0/D1/D2）**：
- D0 只读预检 7 项全 PASS：实机分区表在 **0x9000**，13 项与冻结 CSV **逐项一致**，且候选 `partition-table.bin` 与实机 **逐字节相同**；`otadata` 为 `seq=1 / state=VALID` ⇒ 启动槽位 = `ota_0`。
- D1 刷写：只写 `ota_0 @0x200000`，`Hash of data verified.`，刷后全量回读 sha256 == 冻结值。
- D2：无 panic / WDT / assert / brownout；UI 正常、触摸正常、能进学习页、有开机音。

### 2.7 已知缺陷与风险台账

| # | 项 | 状态 | 说明 |
|---|---|---|---|
| 1 | **ACK 队列永久卡死**（`ACK_PIPELINE_FAIL`） | **已修复，待冷构建验证** | 队首存在被服务端永久拒绝的行时，outbox 永不清理、每轮原样重发，**UI 还显示"已同步"**（静默假成功）。根因：`coordinator.cpp:298 scopeBatchToSent()` 遇首条非 Accepted/Duplicate 即 `break` |
| 2 | **后台同步只在学习页被打开过之后才存在** | **未修** | `LearningRuntime::Init()`（内含 15 s 同步 worker）**全树唯一调用点** = `learning_screen.cc:607` ⇒ 复位后同步**不自愈** |
| 3 | 业务非法事件仍不自愈 | **未修** | 修复只覆盖"服务端 ack 领先"的场景；稳定 4xx（ack 永不动）⇒ 不再静默但**仍不自愈** ⇒ 需补**有界尝试 + 隔离区（DLQ 语义）** |
| 4 | `ota_0` 余量仅 165,424 B | **未解** | 任何新增代码都要过这道尺寸门禁（见 §2.2） |
| 5 | USB-Serial/JTAG 控制台观测受限 | **环境固有限制** | 每次复位后 **~1.1 s 掉线重枚举**，且**重开串口自身会复位芯片** ⇒ 主机侧只能看到每次运行**前 ~1.4 s**，`app_main()` 之后日志拿不到。**解法：改用 NVS 侧信道取证**（已沉淀为 skill） |
| 6 | 网络总时限（NETWORK_TOTAL_DEADLINE） | DEVICE_VERIFY_REQUIRED | 未测 |
| 7 | 严格 UI latency / 20 轮音频稳定性 / 云语音完整闭环 | DEVICE_VERIFY_REQUIRED | 均未测 |
| 8 | 真实时间源 / `/today` / 首页离线徽标解除 | 属 C01，未开始 | `isOffline()` 含 `!time_synced`，M0 未接时间同步 ⇒ 离线徽标是**预期**，不是缺陷 |
| 9 | `sdkconfig` Flash mode 证据冲突 | 只记录 | `FLASHMODE_QIO=y` 但 `FLASHMODE="dio"`；真机 Flash 异常时首个排查点 |
| 10 | 上游自动 OTA 路径 | **必须持续守护** | 候选 `application.cc:179` 在 `HasNewVersion()` 时自动升级，`ota_url` 指公网 `api.tenclass.net`。**设备联网后必须复核**：otadata 未变 + `ota_0` 全量 sha256 == 冻结 + `ota_1` 头未变 |

### 2.8 工作区与提交状态

| 项 | 值 |
|---|---|
| 本地仓库 | `E:\claw4-a05-build-m0`（E 盘根，**不可放 `E:\workbuddy`**，见下） |
| 远端 | `https://github.com/revercgy-hub/claw4-learning-habit-ai.git` |
| 当前分支 | `workbuddy-a05-build-m0`（**注意：本地分支名不带 `/`**） |
| 远端分支 | `workbuddy/a05-build-m0`（tip = `d5e64c5`） |
| 工作树 | clean |
| **未推送的提交** | `1a3d3fe`、`b10dfde`、`7ac29ad`、`7892f30`、`2135fa5`、`6d26014` |

> ⚠️ **两个操作限制（实测，不是猜测）**：
> - **推送**：本机执行环境的网络代理**允许 fetch、拒绝 push**（`CONNECT tunnel failed, response 502`）；
>   绕代理则无路由 ⇒ **push 必须由用户在普通终端执行**。
> - **冷构建**：runner 的 P0 守卫会检测到宿主 PATH 失同步（rc=4）并 fail-closed（**不进入构建、无半成品**）
>   ⇒ **冷构建也必须由用户在普通终端执行**：
>   `powershell -File E:\claw4-a05-build-m0\tools\dev\run-a05-cp2-build.ps1 -Src E:\a05c\s -Build E:\a05c\b2 -Fresh`

### 2.9 学习后端与协议（T1 使用的真实后端）

| 项 | 值 |
|---|---|
| 后端实体 | `E:\workbuddy\claw4-l1-ready\backend`（FastAPI，`app/main.py`） |
| 启动 | `uvicorn app.main:app --host 127.0.0.1 --port 8000`，DB = `sqlite:///./.claw4_host_mvp.db` |
| LAN 暴露 | relay `tools/dev/run-device-backend-relay.py --bind 192.168.3.26 --port 18765 --target-port 8000`（只转发 `/api/v1/*`） |
| 设备侧配置 | 设备 NVS 里 `learning_cfg.base_url = http://192.168.3.26:18765`（**改它需要写 NVS，属禁止项**） |
| 端点 | `/api/v1/devices/challenge`、`/devices/auth`、`/children/{id}/tasks/today`、`/events/batch` |
| 鉴权 | `device_id` + `device_secret`（设备侧 HMAC 签名；**这是唯一直接调用 mbedTLS 的业务文件**：`metalio_signer.cpp`） |
| ACK 语义 | **严格连续序号**：只接受 `sequence == last_acked + 1`；`(device_id, sequence)` **全局唯一**（被他 event_id 占用 ⇒ 逐事件 `conflict`(409)） |
| 依赖版本 | fastapi 0.141.1 / uvicorn 0.52.4 / sqlalchemy 2.0.52（Py3.13） |

> 💡 **运维经验**：**起服务不到 2 分钟设备自己就会重试上来**（每 15 s 一轮）⇒
> **ICMP 扫网段找不到设备，不要用扫描法判断设备是否在线。**

---

## 3. 主要参考 GitHub 项目

### 3.1 `78/xiaozhi-esp32` —— 上游主线（**已迁到 IDF 6.0.2**）

| 项 | 事实 |
|---|---|
| 定位 | 开源 AI 语音聊天机器人固件（MCP 协议），本项目**语音/协议/音频/安全**的上游 |
| **ESP-IDF** | **主线已迁移到 v6.0 及以上，首选稳定版 v6.0.2**；**v5.5 仅保留用于文档明确标注的旧版板卡** |
| 版本矩阵 | 171 个变体，其中 170 个支持 IDF 6.0.x；ESP32-S31 变体需 IDF 6.1+ |
| 构建方式 | `python scripts/build.py <board> [--language|--wake-word|--zip]`，读 `main/boards/<board>/config.json` → `idf.py reconfigure/build/merge-bin` |
| 板卡数 | 47 个 board 目录（`main/boards/`） |
| 默认分区 | `partitions/v2/16m.csv`（16MB） |
| 项目版本 | 2.4.2（根 `CMakeLists.txt`） |
| 上游明确加固项 | **音频流水线并发、MQTT/UDP 数据包校验、发布矩阵选择逻辑**；MQTT 与 BluFi 加密已迁 **PSA Crypto** |
| P4 支持 | **ESP32-P4 Rev1 与 Rev3 均在 IDF 6 + ESP-SR 2.4.7 下受支持** |

> 🎯 **这是"要不要迁 IDF 6"的核心论据**：上游主线已经过去了，我们钉在 `ca3aa3fa` + 5.5.4，
> 等于**只继承 bug、永远拿不到修复**。上游点名加固的"并发 + 包校验"正是稳定性类缺陷。

### 3.2 `CloudZao/MetalioClaw4` —— 我们的 pin（5.5 时代血统）

| 项 | 事实 |
|---|---|
| 我们的 pin | `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950`（tag `latest`） |
| 关系 | xiaozhi-esp32 的**深度定制 fork**，为 `metalio-claw-4` 板定制 |
| 深度定制区（`KEEP_METALIO`，不回推不替换） | 蓝牙音频三模式 codec、`secondary_screen/`、`usb_extend_screen/`、`components/usb_device_uac`、`boards/metalio-claw-4`、`esp_claw_bin/`、`xingzhi-assets/`、`factory-test-assets/` |
| 本地位置 | `E:/workbuddy/学习习惯培育AI/vendor/MetalioClaw4`（**只读基线**，禁止向其 origin 推送本项目变更） |
| 官方恢复镜像 | `Metalio_Claw4_Latest.bin`，33,120,256 B，`1a69e379…` |

### 3.3 `AlayaElla/MetalioClaw4-AgentUI` —— **同源、已成功部署在 Claw4 上**（IDF 6.0.2）

> 这一个对"重置路径"的价值最高：**它在同一块硬件上、用 IDF 6.0.2 真跑起来了。**

| 项 | 事实 |
|---|---|
| 形态 | **完整 fork 上游主线**（不是补丁层）：自带 `main/`、`components/`、`external_apps/`、`scripts/`、`patches/`、`sdkconfig`、`dependencies.lock` |
| ESP-IDF | **v6.0.2** |
| 根 CMake | `project(agent)`、`PROJECT_VER "0.2.0"`、`EXTRA_COMPONENT_DIRS components`、引用 `main/boards/metalio-claw-4/validate_wifi_config.cmake` |
| **板级端口（关键资产）** | `main/boards/` 只有 `common` + **`metalio-claw-4`**（13 文件）：`config.h`、`config.json`、`metalio-claw-4.cc`（38,633 B）、`esp_lcd_fl7707n.c/h`、`esp_lcd_nv3051f.c/h`、`mipi_dsi_power_control.c/h`、`sc7a20_motion.cc/h`、`validate_wifi_config.cmake` |
| 说明 | 上游 47 个 board 里**没有** metalio ⇒ 这是他们自建的、**我们这块板的 IDF6 端口** |
| Wi-Fi 配置参考 | `sdkconfig.defaults.esp32p4`（892 B）给出完整 **C5 over SDIO** 配置：Slot 1 / 4-bit / 40 MHz / 显式引脚（CMD 50、CLK 51、D0 49、D1 34、D2 31、D3 53、RESET 54） |
| 分区 | **单槽** `factory app @0x200000 size 14M`（+ `nvsfactory 200K/nvs 840K/phy_init 4K/model spiffs 956K/resources spiffs 15M/factory_test 600K`） |
| 他们打的补丁（`patches/`） | `elf-loader-esp32p4-executable-segment`、`esp-ml307-multi-response-headers`、`esp-ml307-tcp-receive-task-lifetime`、`esp-video-select-sensor-format`、`esp-cam-sensor-ov2710-720x720` |
| 可借用的工程纪律 | app-only 刷写脚本**先断言 app 位于 0x200000 且 ≤14 MiB** 才调 esptool；`-DryRun` 先试；"打包成功 ≠ 真机验证" |

### 3.4 `AlayaElla/CodexRemote` —— PC 端桥（配套 3.3）

| 项 | 事实 |
|---|---|
| 形态 | Windows x64 客户端（需 .NET 9 + 虚拟麦克风驱动） |
| 通道 | **局域网直连**：TCP **8765** 控制 + UDP **8766** 发现 + **两端共享设备 Token** |
| 设备侧形态 | 做成**独立组件**（`components/codex_remote` + `ui_dispatcher`），**不挂任何 UI 页面** |
| 与我们的关系 | 通道思路与本项目"学习后端走 LAN 直连"一致；其**启动解耦**形态正是我们缺陷 #2 缺的东西 |

### 3.5 横向对比（决定路径时的关键差异）

| 维度 | 本项目（现状） | 3.3 AgentUI（已部署） |
|---|---|---|
| 与上游关系 | **补丁层**（叠在 pin 死的 commit 上） | **完整 fork 主线** |
| ESP-IDF | 5.5.4 | **6.0.2** |
| app 槽位 | `ota_0` 9 MiB（余 165 KB） | `factory` **14 MiB**（余 ≈5.1 MiB） |
| 双槽 OTA | 名义有、**实际不可用** | 单槽（不提供 OTA） |
| 板级端口 | 复用上游 `boards/metalio-claw-4`（5.5 写法） | **自建 13 文件 IDF6 端口** |
| 后台服务启动 | 挂在学习页（复位后不自愈） | **独立组件，不挂 UI** |
| 代码可迁移性 | — | ⚠️ **不能直接搬代码**（IDF 大版本不同）；只能对齐**结构 / 分区策略 / 脚本流程 / 纪律** |

---

## 4. 为什么现在要重置路径（IDF 6 决策的关键事实）

### 4.1 触发

用户判断："ESP-IDF 大版本可以调整到 v6.0.2，不要订死 5.5.4；新 IDF 下小智也能有更新，之前的 bug 应该都能解决。"

### 4.2 三条决定性事实（已核验，非推测）

**① 🚫 阻塞级 —— P4 芯片改版选择，不改根本起不来**

IDF v6.0 release notes 原文：

> **Default revision of ESP32-P4 is changed to v3.0.** Applications for < 3.0 chips whose sdkconfig
> has no `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` will build an **incompatible binary**.

本机设备实测 **ESP32-P4 rev v1.3**（< 3.0）⇒ 迁移**必须**显式加
`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`。**单这一条就否掉"改个版本号然后重建"。**

**② ⚠️ 体积会变大（与直觉相反）**

IDF v6.0 迁移说明给出的净增（MbedTLS v4.x + PSA Crypto 迁移）：

| 组件 | 影响 |
|---|---|
| `esp_http_client` | **+37 KB（约 +5.76%）** ← 我们在用（学习同步） |
| `esp_http_server` | +41 KB（约 +4.97%） |
| `https_server` | +27 KB（约 +3.08%） |

当前余量只有 **161.5 KiB** ⇒ **单 http_client 一项就可能吃掉约 23%**。
反向减项也存在（Picolibc 取代 Newlib；不用 2D-DMA 可省约 10 KB）⇒ **净效果只能实测。**

> ⇒ 这与"迁 IDF 来解决尺寸问题"的期望**相反**。尺寸问题的正解是**分区布局**，不是 IDF 版本。

**③ ⚠️ 补丁层必然失效**

`project-ca3aa3fa.patch` 是针对**具体上游 commit** 的四文件集成取证；`a05-0001-cmake-source-registration.patch`
改的是 `main/CMakeLists.txt`。IDF 6 时代的上游 `main/CMakeLists.txt`、board 机制（`config.json` + `scripts/build.py`）
都已变化 ⇒ **两个补丁都不能直接套用，必须对新上游重新编排。**

### 4.3 V4 原规划里 IDF 6 的定位（本次要推翻的那一条）

V4 文档明确写过：

> `docs/CLAW4_学习伙伴_总体设计架构_V4.md` §12：**ESP-IDF 6.1 新定位 = Compatibility Track — BUILD / RESEARCH ONLY**；
> "在 Learning L0/L1 真机稳定前，不把 6.1 作为主基线。"
> `CLAW4_学习伙伴_任务规划_V4.md` §21：`experimental/idf61-compat` 分支，仅做 compile / link / binary size / warnings。
> §22 永久禁区含 **"IDF 主版本切换"**。

**⇒ 本次用户意向 = 把 IDF 6 从"旁路兼容轨"提升为"主线"**，即**有意推翻 V4 §12/§21，并解除 §22 的"IDF 主版本切换"禁令**。
这属于**基线级变更**，需要新的任务书与明确授权，不能当作普通开发任务执行。

### 4.4 迁移面实测（好消息：我们自己的代码几乎不受影响）

| IDF6 变更 | 命中位置 | 归属 | 判定 |
|---|---|---|---|
| legacy `driver/i2s.h`（已删除） | `managed_components/espressif__esp_codec_dev/platform/audio_codec_data_i2s.c` | **托管组件** | 低（换兼容版组件） |
| legacy `driver/rmt.h`（已删除） | tinyusb 的 `led_strip_rmt_dev_idf4.c` | 组件内 **idf4 兼容死代码** | 无 |
| legacy `driver/temperature_sensor.h` | `main/boards/metalio-claw-4/metalio-claw-4.cc` | **板级文件** | 低（3.3 已有 IDF6 端口） |
| 直接调用 `mbedtls/*` | `main/learning/metalio_claw4/device/ports/metalio_hmac_signer.cpp` | **我们的代码，仅此 1 处** | **中**：需迁 PSA Crypto，**直接决定学习后端鉴权能否通过** |
| cJSON 移出 IDF → managed component | 30 个文件引用 `cJSON.h` | 头文件名不变、API 兼容 | 低（只变依赖图） |
| `esp_lcd` 颜色格式改 FourCC | `bits_per_pixel` 4 处 / `color_space` 2 处 | 板级/显示 | 中 |
| **业务层 `main/learning`（87 文件）** | **legacy 驱动 0 命中；mbedtls 仅 1 处** | 我们 | ✅ **几乎完全 IDF 无关** |

**其它必改项**：`wifi_provisioning` → 外部组件 `network_provisioning`；`esp_wifi_remote` / `esp_hosted` 升版
（Kconfig 有重命名，如 `CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM` → `CONFIG_WIFI_RMT_STATIC_RX_BUFFER_NUM`）；
`esptool chip_id` → `chip-id`（我们 `tools/dev` 里 0 命中）；`psa_crypto_init()` 必须前置；
默认 C 库换 **Picolibc**（printf/stdio 行为需回归）；部分警告升级为**构建阻断错误**。

### 4.5 资源可行性（已实测）

| 项 | 值 |
|---|---|
| IDF 6.0.2 官方归档 | **1,914,371,137 B（1.91 GB）**；已下载并校验（37406 条目，CRC 无错） |
| sha256 | `2e4f32942e2eb0860b1adbab466e89568776519754c9a5df44dca0a9ef6bfaec` |
| 本地位置 | `E:\workbuddy\_idf6_staging\esp-idf-v6.0.2.zip`（**纯落盘，可删**） |
| 下载源 | `dl.espressif.com`（HTTP 200，实测 101 秒） |
| 磁盘 | E: 余 **591.2 GB** / C: 余 231.9 GB |
| 额外需要 | 新工具链 + 新 python env（约再 1–2 GB）；riscv gcc 14.2.0 **需换新版本** |

---

## 5. 文档地图（去哪找什么）

| 想知道 | 看 |
|---|---|
| 项目目标 / 架构总纲 | `项目总规划/CLAW4_学习伙伴_总体设计架构_V4.md` |
| 任务路线 / P 阶段 | `项目总规划/CLAW4_学习伙伴_任务规划_V4.md` |
| 协作规则 / 状态机 / 硬门禁 | `AGENTS.md`（根） + `项目总规划/AGENTS.md` |
| 当前调度 | `docs/project_management/TASK_BOARD.md`（⚠️ 已滞后，见 §2.6） |
| 硬件平台映射（最全的硬件事实） | `docs/CLAW4_PLATFORM_MAP.md` |
| 硬件假设台账（分级） | `docs/HARDWARE_ASSUMPTIONS.md` |
| 上游追踪策略与分类 | `docs/XIAOZHI_UPSTREAM_TRACKING.md` |
| Learning 与 Metalio 集成点 | `docs/METALIO_LEARNING_INTEGRATION_MAP_V4.md` |
| 主机架构 | `docs/ARCHITECTURE.md`、`docs/ARCHITECTURE_V5_3.md` |
| 构建/基线 | `docs/BUILD.md`、`docs/BASELINE.md`、`docs/BOARD_REVISION.md` |
| A05 构建封档 | `docs/project_management/reports/WB_A05_BUILD_001_REPORT.md`、`WB_A05_CP4_CANDIDATE_FREEZE_001.md` |
| 实机 bring-up | `docs/project_management/reports/WB_A05_DEVICE_001_REPORT.md` |
| T1 结论与修复 | `WB_A05_DEVICE_T1_001_REPORT.md`、`T1-FINDING-ACK-PIPELINE-FAIL.md`、`T1-FIX-PROPOSAL`、`T1-FIX-IMPL` |
| 同源参考项目吸收 | `docs/project_management/evidence/a05-device/T1-REFERENCE-PROJECTS.md` |
| 业界先例（毒消息 / 低水位 ACK） | `docs/project_management/evidence/a05-device/T1-PRIOR-ART-RESEARCH.md` |
| **IDF 6 迁移评估（最详细）** | `docs/project_management/reports/WB_A05_IDF6_001_ASSESSMENT.md` |
| 实机证据原始件 | `docs/project_management/evidence/a05-device/` |

---

## 6. 待研究的问题（供重新设计路径）

> 以下是**尚未决定**的问题，按影响面排序。每条都附了"判断它需要的证据"。

1. **与上游的关系要不要换形态？**
   现状是"补丁层叠在 pin 死的 commit 上"，代价是每跟一次上游都要重做补丁。
   3.3 项目选择了"完整 fork 主线"。
   → 需要：两种形态的**长期维护成本对比**（含上游 6.0 主线相对 `ca3aa3fa` 的精确 diff 规模）。

2. **分区布局要不要换？**（单槽 14 MiB vs 现双槽）
   单槽能一次解掉尺寸门禁（165 KB → ≈5.1 MiB），代价是 **NVS 全丢 + 放弃厂商云 OTA**。
   → 需要：明确"是否还需要厂商云 OTA"；以及 `ota_1` 的 ESPClaw edge_agent 是否需要保留。

3. **IDF 6 迁移与分区变更是否捆绑成一次基线切换？**
   两者互相影响（单槽能让 6.0.2 的体积增长不再是问题）。
   → 需要：捆绑 vs 拆分的**风险与回滚路径**对比。

4. **迁移后 `xiaozhi.bin` 到底多大？**
   当前只能引用 Espressif 公布的 ±37KB 影响，**不能推断净结果**。
   → 需要：一次**有界 spike**——装 6.0.2 + 上游 6.0 主线 + 3.3 的 metalio-claw-4 端口 + `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`，
     构建一次，量出 **app 字节数 / RAM / heap / task stack** 四个数字。
     **建议 go/no-go 判据**：app ≤ 9.4 MB 且余量 ≥ 约 100 KiB。

5. **当前冻结候选与 A05-BUILD 封档如何定位？**
   IDF 切换会让 `420d0df` / `claw4-v53-m0-a05-2c8f58f` 变成"历史参照基线"。
   → 需要：决定是"先保留 5.5.4 线作为验证载体跑完 T1 复测"还是"直接跳 6.0.2 重做"。
     ⚠️ 建议前者**至少**跑一次：迁移期需要**一个已知良好参照点**才能归因故障。

6. **缺陷 #2/#3 在新基线里怎么处理？**
   后台同步的启动耦合（只在学习页实例化）与"业务非法事件不自愈"。
   → 需要：是否借迁移一次性重构启动路径（3.3 的"独立组件不挂 UI"是现成参考形态）。

7. **`esptool` / 工具脚本 / 门禁需要哪些适配？**
   `chip_id` → `chip-id`；`ESP_IDF_VERSION` 的取值规则要重验（当前"必须写 5.5"是 5.x 时代的结论）。
   → 需要：新版本下 `esp_wifi_remote` Kconfig 变体与 Wi-Fi 符号的验证方法。

8. **Picolibc 与 PSA 的运行时回归清单是什么？**
   → 需要：TLS / OTA / 日志行为 / 长时运行 / 内存余量的回归项清单。

---

## 附：本机路径速查

```text
项目仓库          E:\claw4-a05-build-m0
隔离源码树        E:\a05c\s
隔离构建目录      E:\a05c\b        （新候选建议用 b2）
上游只读基线      E:\workbuddy\学习习惯培育AI\vendor\MetalioClaw4
IDF 5.5.4        E:\workbuddy\esp-idf-5.5.4-ascii
IDF tools        E:\workbuddy\claw4-idf-tools
待用 IDF 6.0.2   E:\workbuddy\_idf6_staging\esp-idf-v6.0.2.zip
学习后端          E:\workbuddy\claw4-l1-ready\backend
后端备份          E:\workbuddy\claw4-db-backup-20260913\
```

**设备**：ESP32-P4 / 32MB / MAC `80:f1:b2:d2:ed:14` / **COM7 = USB-Serial/JTAG**（备用 COM6）
**权威任务基线**：`docs/project_management/TASK_BOARD.md`（⚠️ 需刷新）
