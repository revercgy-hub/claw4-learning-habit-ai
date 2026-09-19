# WB-A05 IDF6 迁移影响评估（IDF-6.0.2-MIGRATION-001）

**性质**：只读评估 + 预备性侦察，**不改变任何冻结基线**
**日期**：2026-09-19
**当前基线**：Candidate `claw4-v53-m0-a05-2c8f58f`｜ESP-IDF **5.5.4**｜上游 pin `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950`
**触发**：用户判断"可以调整到 v6.0.2，不必订死 5.5.4；新 IDF 下小智也有更新，之前的 bug 应该能解决"
**评估结论（一句话）**：**方向正确，但不能作为"改版本号"执行。**

---

## §0 结论摘要

| 问题 | 结论 |
|---|---|
| 用户方向对不对？ | **对。** 上游 `78/xiaozhi-esp32` 主线**已迁移到 IDF 6.0.2**，我们的 pin 属 legacy 只维护分支 |
| "新 IDF 能解决旧 bug"这个判断成立吗？ | **部分成立，且是有针对性的**：上游明确列出加固项含**音频流水线并发、MQTT/UDP 包校验**——正是稳定性类缺陷 |
| 能直接改版本号重建吗？ | **不能。** 有 1 项**阻塞级**硬约束（P4 芯片改版选择），不改**根本起不来** |
| 它会解决我们当前的阻塞吗？ | **不会。** 当前阻塞是 ACK 队列 livelock，属**我们自己的代码**，已修复并通过门禁（`7ac29ad`） |
| 它会缓解"余量只剩 1.75%"吗？ | **大概率相反 —— 会让体积更大。** 见 §2.2 |
| 迁移面大吗？ | **比预期小得多。** 我们的业务层（`main/learning` 87 文件）**几乎完全 IDF 无关** |
| 建议 | **分阶段**：先在 5.5.4 上把 ACK 修复落到真机（闭合 T1、拿到已知良好参照点）→ 并行做**有界 spike**（实测体积）→ 再决定 |

---

## §1 为什么这个方向是对的

### 1.1 上游主线已经走过去了（原文引用）

`78/xiaozhi-esp32` README：

> 项目主线现已迁移到 **ESP-IDF v6.0 或以上版本，首选稳定版为 v6.0.2**；此前的 157 个发布变体已在 ESP-IDF v6.0.1 上通过构建验证。当前矩阵包含 171 个变体，其中 170 个支持 IDF 6.0.x……
> **ESP-IDF v5.5 仅保留用于文档明确标注的旧版板卡**

> 加固了**音频流水线并发、MQTT/UDP 数据包校验和发布矩阵选择逻辑**。MQTT 和 BluFi 加密已迁移到 PSA Crypto。
> **ESP32-P4 Rev1 和 Rev3 均支持 IDF 6**（配 ESP-SR 2.4.7）

**这是本次评估里最有分量的一条。** 我们钉在 `ca3aa3fa`（5.5 时代 commit）+ IDF 5.5.4，等于**只继承 bug、永远拿不到修复**。用户"新 IDF 下小智也能有更新"的判断，方向上是准确的。

### 1.2 我们与上游的关系也要同时纠正

我们是**补丁层**（`integration/metalio_claw4/patches/`，2 个补丁叠在 pin 死的上游上），不是 fork 主线。参考项目 `AlayaElla/MetalioClaw4-AgentUI` 是**完整 fork 主线**（自带 `main/`、`scripts/build.py`、`components/`）。**两者的维护成本结构完全不同** —— 我们每跟一次上游，都要重做补丁层。

---

## §2 四个必须先知道的硬事实（都不是推测）

### 2.1 🚫 阻塞级：P4 芯片改版选择（不改根本起不来）

IDF v6.0 release notes（原文）：

> **Default revision of ESP32-P4 is changed to v3.0.** Applications for < 3.0 chips whose sdkconfig has no `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` will build an **incompatible binary**.

我们的设备 D0-A 实测：**ESP32-P4 rev v1.3**（< 3.0）。⇒ 迁移**必须**显式加 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`，否则**编出来的固件在我们的板子上不可用**。这条单独就否掉了"只改 IDF 版本号然后重建"的做法。

### 2.2 ⚠️ 体积：IDF 6.0 让二进制**更大**，而我们的余量只剩 161.5 KiB

IDF v6.0 迁移说明（原文，闪存占用净增）：

| 组件 | 影响 |
|---|---|
| `esp_http_client`（MbedTLS 4.x + PSA 迁移） | **+37 KB（约 +5.76%）** |
| `esp_http_server` | +41 KB（约 +4.97%） |
| `https_server` | +27 KB（约 +3.08%） |

我们两条**都在用**：学习同步走 `esp_http_client`，OTA 走 `esp_https_ota`。

当前实测账本：

```
app (xiaozhi.bin)  = 9,271,760 B
ota_0 槽位         = 9,437,184 B
余量               =   165,424 B  = 161.5 KiB  (1.75%)
```

**单 esp_http_client 一项就可能吃掉余量的 ~23%。** 反方向的减项也存在（Picolibc 取代 Newlib 可能省体积；不用 2D-DMA 可省约 10 KB；LCD 改 FourCC），但**净效果无法推断，只能实测**。

⇒ **这是 go/no-go 的第一个判据，也是 spike 的核心产出。**

### 2.3 ⚠️ 补丁层必然失效，需要重做

`project-ca3aa3fa.patch` 是**针对 commit `ca3aa3fa` 的四文件集成变化取证**；`a05-0001-cmake-source-registration.patch` 改的是 `main/CMakeLists.txt`。上游 6.0 主线的 `main/CMakeLists.txt`、board 机制（`config.json` + `scripts/build.py`）都已变化 ⇒ **两个补丁都不能直接套用，必须对新上游重新编排**。

### 2.4 ⚠️ 工具链与依赖图全换

| 项 | 现状（实测） | IDF 6.0.2 需要 |
|---|---|---|
| 交叉编译器 | `riscv32-esp-elf-gcc 14.2.0`（esp-14.2.0_20260121） | 新版（需重装工具链） |
| Python env | `idf5.5_py3.12_env`（Python 3.12.14） | 新建 idf6.0 env |
| cJSON | IDF 内置（我们 **30 个文件**引用 `cJSON.h`） | **移出 IDF → managed component** |
| `wifi_provisioning` | （`main` 内 0 命中） | 移出 → 外部组件 `network_provisioning` |
| `esp_wifi_remote` / `esp_hosted` | `^1.6.4` / 已启用（C5 over SDIO） | 需按 6.0 兼容版本升级（Kconfig 有重命名） |
| 旧 `esptool chip_id` | 我们 `tools/dev` 脚本 **0 命中**（幸运） | 已改名 `chip-id`（手工取证命令需注意） |

另：默认 C 库由 Newlib 换成 **Picolibc**；部分警告升级为**构建阻断错误**；`psa_crypto_init()` 必须在任何加密操作前调用。

---

## §3 迁移面实测（本地源码扫描，非估计）

| IDF6 变更 | 命中位置 | 归属 | 判定 |
|---|---|---|---|
| legacy `driver/i2s.h`（已删除） | `managed_components/espressif__esp_codec_dev/platform/audio_codec_data_i2s.c`（+1 个 test_app） | **托管组件** | **低**：换 IDF6 兼容版组件即可 |
| legacy `driver/rmt.h`（已删除） | tinyusb 的 `led_strip_rmt_dev_idf4.c` | 组件内 **idf4 兼容死代码** | **无** |
| legacy `driver/temperature_sensor.h`（已删除） | `main/boards/metalio-claw-4/metalio-claw-4.cc` | **板级文件** | **低**：见 §4，参考端口已存在 |
| 直接调用 `mbedtls/*` | `main/learning/metalio_claw4/device/ports/metalio_hmac_signer.cpp`（**业务代码仅此 1 处**） | **我们的代码** | **中**：需迁 PSA Crypto（`psa_mac_*`） |
| cJSON 移出 IDF | 30 个文件引用 | 头文件名不变、API 兼容 | **低**：只变依赖图 |
| `esp_lcd` 颜色格式改 FourCC | `bits_per_pixel` 4 处 / `color_space` 2 处 | 板级/显示 | **中** |
| **我们业务层 `main/learning`（87 文件）** | **legacy 驱动 0 命中；mbedtls 仅 1 处** | 我们 | ✅ **几乎完全 IDF 无关** |

**一句话**：迁移的难度**集中在板级与上游层**，而那一层**已经有现成答案**（见 §4）；我们自己写的 108 个文件里，真正要改的只有 **1 个 HMAC 签名文件**。

---

## §4 🎯 关键减负：我们的板级端口已经存在（且已在 IDF 6 上跑通）

参考项目 `AlayaElla/MetalioClaw4-AgentUI` 的 `main/boards/` **只有两个目录：`common` 和 `metalio-claw-4`**。也就是说它是在**上游 6.0 主线**上加了**我们这块板的端口**。而 `78/xiaozhi-esp32` 上游的 47 个 board 目录里**没有** metalio —— 确认这是他们的自建端口。

`main/boards/metalio-claw-4/` 共 **13 个文件**：

```
config.h / config.json                       板级配置（config.json 仅 143 B）
metalio-claw-4.cc              38,633 B     ← 主板级实现（含温度传感器，已是 IDF6 写法）
esp_lcd_fl7707n.c/.h           12,552 / 5,993 B   LCD 面板驱动 A
esp_lcd_nv3051f.c/.h           16,586 / 6,545 B   LCD 面板驱动 B
mipi_dsi_power_control.c/.h     4,606 /   887 B   MIPI DSI 供电时序
sc7a20_motion.cc/.h            16,447 / 3,519 B   加速度计
validate_wifi_config.cmake      1,681 B
```

同时他们的 `sdkconfig.defaults.esp32p4`（892 B）给出了 **C5 over SDIO 的 Wi-Fi remote 完整配置**（我已在证据文档中逐行留存）：SDIO Slot 1 / 4-bit / 40 MHz / 显式 GPIO 引脚（CMD 50, CLK 51, D0 49, D1 34, D2 31, D3 53, RESET 54）—— 与我们的硬件形态一致。

**⇒ 板级端口不是"要从头写"，是"可复用/可对照"。这实质性降低了迁移风险。**

---

## §5 成本与风险

### 5.1 成本（可量化部分）

| 项 | 量 |
|---|---|
| IDF 6.0.2 官方归档（含 submodules） | **1,914,371,137 B（1.91 GB）** —— 已确认可从 `dl.espressif.com` 下载（HTTP 200） |
| 新工具链 | 另需 1–2 GB（交叉编译器 + python env） |
| 磁盘 | E: 剩余 **591.2 GB**，充足 |
| 网络 | 已验证可达；但沙箱代理**允许 fetch 拒绝 push**（`git push` 走不通） |

### 5.2 风险分层

| 风险 | 级别 | 说明 |
|---|---|---|
| P4 rev 选择漏配 ⇒ 固件不可用 | **高（但可完全消除）** | §2.1，显式加 flag + 刷机后必须复验能起来 |
| 体积超 `ota_0` ⇒ 编不出来 | **高** | §2.2，必须实测；与分区决策耦合（§7） |
| TLS/OTA 路径运行期回归 | **中高** | MbedTLS v4 是破坏性变更；**必须做真实 TLS/OTA 往返测试**，不是"能编译就算过" |
| PSA 迁移破坏我们的 HMAC 签名 ⇒ 学习后端鉴权失败 | **中** | 仅 1 文件，但**直接决定 T1 能否通过**，属关键路径 |
| 补丁层重做引入漂移 | **中** | 需重新取证 + 反向回滚校验 |
| Picolibc 换 libc ⇒ printf/stdio 行为变化 | **中** | 需长时运行 + 日志行为验证 |
| 全量刷写丢 NVS（若同时改分区） | **中** | 见 §7 |
| 冻结基线作废 | **确定发生** | A05-BUILD 封档（`420d0df`）将变为**历史参照**，不再是当前基线 |

---

## §6 推荐路线（分阶段 + go/no-go 判据）

### 阶段 0（现在，零风险）：预备 —— 本次已做
- ✅ 评估与迁移面实测（本文）
- ⏳ IDF 6.0.2 归档下载（1.91 GB，纯落盘，**不绑定任何决策**，可随时删除）

### 阶段 1（进行中，**不与迁移纠缠**）：在 5.5.4 上闭合 T1
先把 ACK 修复重建 + 刷机 + 复跑 T1。**理由**：迁移期间需要**一个已知良好的参照点**来归因故障。没有它，任何异常都无法区分"是迁移引入的"还是"本来就有"。
> 5.5.4 在这条线上的角色是**验证载体，不是终点** —— 不应再为它做长期投入。

### 阶段 2（有界 spike，建议并行启动）：只回答 4 个数字
在**独立目录**做（不碰 `E:\a05c\s`、不碰 manifest）：

1. 装 IDF 6.0.2 + 工具链（`install.bat` / `idf_tools.py install`）
2. 以上游 6.0 主线 + `metalio-claw-4` 端口 + 加 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`，**构建一次**
3. **测出 `xiaozhi.bin` 实际字节数** ⇒ 对比 `ota_0` 的 9,437,184 B
4. 测出 RAM / heap / task stack 的前后差值

**go/no-go 判据**：
- ✅ 若 app ≤ 9.4 MB 且余量 ≥ 约 100 KiB ⇒ 迁移可行，进阶段 3
- ❌ 若 app 超 `ota_0` ⇒ **必须**先解决分区（§7），否则迁了也刷不进去

### 阶段 3（需正式授权）：迁移实施
重编排补丁层 → 重建 sdkconfig → 重建依赖锁 → PSA 迁移（HMAC）→ 全量回归（Host 门禁 + 真机 TLS/OTA/学习同步）→ 新 Candidate 冻结 → D0/D1/D2 + T1 复跑。

---

## §7 ⚠️ 与分区决策的强耦合（两个轴不能分着定）

IDF 版本与分区布局**互相决定**，必须一起拍：

| 组合 | 效果 |
|---|---|
| **5.5.4 + 双槽**（现状） | 余量 161.5 KiB；`ota_1` 4 MiB 装不下 8.84 MiB 的 app ⇒ **双槽 OTA 早已不可用**，白付 4 MiB 代价 |
| **6.0.2 + 双槽** | 体积**可能更大** ⇒ 161.5 KiB 余量有被吃穿的风险 ⇒ **风险最高** |
| **6.0.2 + 单槽 factory 14 MiB** | 余量升到 ~5.1 MiB ⇒ 6.0.2 的体积增长**不再是问题** ⇒ **风险最低** |
| 5.5.4 + 单槽 | 余量宽松，但停在 legacy 分支（拿不到上游修复） |

**倾向建议**：若决定迁移，**应同时采用单槽 `factory` 14 MiB**（参考项目已验证该布局在 IDF 6.0.2 上真机可用）。
**但两笔代价必须一并接受**：① 改分区 ⇒ **必须全量刷 `0x0` ⇒ NVS 全丢**（Wi-Fi 凭据 / `learning_cfg` / MQTT 凭据需重配 + 重新 provisioning）；② 单槽下**厂商云 OTA 结构上不可能**（本候选 `application.cc:179` 会在 `HasNewVersion()` 时自动升级）。

---

## §8 给 Codex 的裁定项

| # | 问题 | 建议 |
|---|---|---|
| 1 | 是否认可"5.5.4 是验证载体、6.0.2 是目标"的双轨定位？ | 认可；5.5.4 只用于闭合 T1 与提供参照点 |
| 2 | 是否批准阶段 2 的**有界 spike**（独立目录、不影响冻结基线）？ | 批准；产出体积/RAM 四个数字后再决定 |
| 3 | 迁移是否**必须**与分区变更（单槽 14 MiB）打包？ | 建议打包；否则 6.0.2 的体积增长单独构成超限风险 |
| 4 | 是否接受两笔代价：**NVS 全丢 + 放弃厂商云 OTA**？ | 需明确表态 |
| 5 | 我们的 2 个补丁对新上游重编排，与新 Candidate 冻结合并为**一个任务**还是拆两个？ | 建议合并（一次基线切换） |
| 6 | `metalio-claw-4` 板级端口（参考项目 13 文件）是**直接复用**还是**仅作对照重写**？ | 建议先对照、差异明确后再复用 |
| 7 | A05-BUILD 封档（`420d0df`）在迁移后如何定位？ | 定位为**历史参照基线**，当前基线随新 Candidate 转移 |

---

## §9 本次范围声明

- ✅ **未修改**任何产品源码 / CMake / sdkconfig / partition / 设备 NVS
- ✅ **未** rebuild、**未** reflash、**未** 改动 `E:\a05c\s` 与 `a05_build_input_manifest.json`
- ✅ 仅新增：本文档 + IDF 6.0.2 归档下载（`E:\workbuddy\_idf6_staging\`，纯落盘）
- ⚠️ 冻结基线（A05-BUILD `420d0df`、Candidate `claw4-v53-m0-a05-2c8f58f`）**本次未受影响**

## §10 证据索引

| 文件 | 内容 |
|---|---|
| `evidence/a05-device/T1-REFERENCE-PROJECTS.md` | 参考项目吸收：分区布局对比 / 启动耦合 / 刷写守卫 |
| `evidence/a05-device/T1-PRIOR-ART-RESEARCH.md` | 业界先例：poison message / 低水位 ACK / RFC 6578 基线重同步 |
| `reports/WB_A05_DEVICE_T1_001_FIX_IMPL.md` | ACK 队列修复实施（门禁 30/30） |
| `reports/WB_A05_DEVICE_T1_001_NEXT_STEPS.md` | 交接：push 命令 / 冷构建命令 / 机制①设计草案 |
| 本文 | IDF6 迁移评估 |
