# 参考项目吸收：两个已成功部署在 Claw4 上的开源项目

**来源**（均为 AlayaElla 的公开仓库，2026-09-19 只读查阅）：
- 固件：<https://github.com/AlayaElla/MetalioClaw4-AgentUI>（ESP32-P4 触屏 Agent UI）
- PC 端：<https://github.com/AlayaElla/CodexRemote>（Windows 桥接，把设备接到 PC 上的 Codex）

**为什么值得看**：两者**与本项目同源** —— 上游都是 `78/xiaozhi-esp32`（他们经 `CloudZao/MetalioClaw4` 分支），
且**已在 Metalio Claw4（ESP32-P4）真机部署运行**。所以他们的**构建/烧录/分区/启动结构**是可以直接对照的实证，
不是纸面方案。

> 声明：以下只基于**公开文档与分区表文件**。我**没有**在他们的设备上验证，也没有跑过他们的脚本；
> 引用他们"成功部署"是仓库自述 + 文档齐全度判断，不等于第三方独立核实。

---

## 1. 事实对照表

| 项 | 他们（AgentUI / CodexRemote） | 我们（claw4-a05-build-m0） |
|---|---|---|
| 芯片 | ESP32-P4 | ESP32-P4（v1.3） |
| **ESP-IDF** | **v6.0.2**（文档明确"不要用其他版本替代"） | **5.5.4**（`ESP_IDF_VERSION=5.5`，见 WB-APP-FIRST-L3 记录） |
| 上游 | `78/xiaozhi-esp32`（经 CloudZao/MetalioClaw4） | 同源（pinned `vendor/MetalioClaw4` @ `ca3aa3fa`） |
| 源码组织 | `main/` + `components/`（本地组件：`codex_remote`、`json`、`uart-uhci`、`ui_dispatcher`）+ `patches/` + `external_apps/` | `firmware/` + `integration/metalio_claw4/{device,host_glue,patches,candidates}` |
| **分区表** | `partitions/v1/32m.csv`：`factory` app **@0x200000，14 MiB**（单槽） | `partitions/v1/32m_dual.csv`：`ota_0` @0x200000 **9 MiB** + `ota_1` @0xB00000 **4 MiB**（双槽） |
| 构建 | `idf.py build`，或 `scripts/package-esp32.ps1 -IdfPath <idf>`（打包时**检查 app 是否 ≤14 MiB**） | `tools/dev/run-a05-cp2-build.ps1`（隔离树 + 输入冻结 + P0 PATH 守卫） |
| 烧录（全量） | `scripts/flash-esp32.ps1 -Port COMx` → **写 0x0**（覆盖分区表与数据分区） | 任务书禁止；只允许 app-only 写 `ota_0 @0x200000` |
| 烧录（仅 app） | `scripts/flash-esp32-preserve-settings.ps1` → **先校验 app 位于 0x200000 且 ≤14 MiB**，再 esptool | D1：我们自己做等价校验（刷前重算 sha256 + 刷后全量回读） |
| 试跑守卫 | 两个烧录脚本都有 **`-DryRun`**（不写设备） | D0 只读预检（芯片/分区表/otadata/备份 + MISMATCH ⇒ HARD STOP） |
| 设备↔PC 通道 | **局域网直连**：UDP 8766 发现 + **TCP 8765** 控制；两端共享 **设备认证 Token** | 局域网直连学习后端（设备 NVS `learning_cfg.base_url`），HMAC 设备签名 |
| 后台服务形态 | 桥接做成**独立组件 `components/codex_remote`**（不挂在某个 UI 页面上） | `LearningRuntime` 唯一实例化点是**学习页** `learning_screen.cc:607` |
| 状态可观测 | UI 明确有"准备中 / 提交中 / **待同步**"等状态 | 修复前无（假 Synced）；本次已加 `SyncOutcome::Blocked` |
| 敏感数据纪律 | 文档明确"不要把含设备 Token / Wi-Fi 凭据 / NVS / 完整 flash dump 的文件提交仓库" | §34 脱敏（同一条纪律） |

---

## 2. 🎯 对我们最有价值的三点（按价值排序）

### 2.1 「`ota_0` 只剩 1.75%」这个硬门禁，可能是分区布局自己造成的 —— **有实证替代布局**

他们的完整 32MB 布局（`partitions/v1/32m.csv`，逐行原文）：

```
# Name,         Type, SubType,      Offset,   Size, Flags
nvsfactory,     data, nvs,                ,   200K,
nvs,            data, nvs,                ,   840K,
phy_init,       data, phy,                ,     4K,
model,          data, spiffs,             ,   956K,
factory,        app,  factory,    0x200000,    14M,     <-- 单槽 app，14 MiB
resources,      data, spiffs,             ,    15M,
factory_test,   data, spiffs,             ,   600K,
```

**数字对比**（把 app 槽位算清楚）：

| | app 槽位 | 我们当前 app 尺寸 | 余量 |
|---|---|---|---|
| 我们（双槽） | `ota_0` = 9,437,184 B（9 MiB） | 9,271,760 B | **165,424 B（1.75%）** |
| 他们（单槽） | `factory` = 14 MiB = **14,680,064 B** | 9,271,760 B | **≈5.1 MiB** |

**关键洞察**：9 MiB + 4 MiB = 13 MiB，加上 `otadata` 正好约等于他们的 14 MiB。
**空间总量一样，"9+4" 与 "14" 只是切法不同。** 我们的布局把 app 槽砍到 9 MiB，换来了一个 `ota_1`（4 MiB）。

**而那个 `ota_1` 对我们毫无用处**：本候选 app 8.84 MiB > `ota_1` 4 MiB ⇒ **双槽 OTA 事实上不可用**（已在
A05-BUILD 封档报告里记录为 `DUAL-SLOT OTA UNAVAILABLE`）。也就是说我们**当前是"两头都亏"**：
付出了 4 MiB 的槽位代价，却没有换来可用的 OTA 能力。

⇒ **单槽 `factory` 14 MiB 对我们是一个严格的改进**：白拿 ≈5.1 MiB 余量，代价只是一个**我们本来就拿不到**的回滚能力。

**但必须先想清两笔真实代价（不能只说好处）**：
1. **改分区表 = 必须全量刷写**（他们那条也是写 `0x0`）⇒ **会擦掉 NVS**：Wi-Fi 凭据、`learning_cfg`
   （`base_url`/`device_secret`）、`mqtt` 凭据全没了，需要重新配网 + 重新 provisioning。
2. **厂商云 OTA 路径会彻底失去写入目标**：本候选 `application.cc:179` 会在 `HasNewVersion()` 时自动升级，
   `ota_url` 指向公网 xiaozhi 云。单槽布局下没有第二个 app 槽可写 ⇒ 这条链路从"事实上不可用"变成"结构上不可能"。
   （**如果未来要接厂商云 OTA，就不能用单槽。**）

⇒ 结论：**这是一个架构级决策，必须交 Codex**。我的建议是**把它作为"尺寸门禁的唯一外科解法"正式提请**，
并附上"代价 1/2 + 反悔成本（改回双槽同样要全量刷写一次）"。

### 2.2 启动耦合（我们的方案 D）有现成参考实现

他们把设备↔PC 的桥接做成**独立组件 `components/codex_remote`**（配套 `ui_dispatcher`），
而不是挂在某个 UI 页面里 —— 从 README 的用户流程看，设备端"找不到电脑/连接失败/已断开"都有独立状态，
说明这个桥接的生命周期**不依赖用户是否打开过某个页面**。

这正是我们缺的：我们的 `LearningRuntime`（含每 15 s 同步 worker）**唯一实例化点是学习页**
⇒ 复位后同步不自愈。他们的结构证明了"后台服务做成组件、启动路径独立于 UI"是可行且已在 Claw4 上跑通的。

**后续可挖**：`components/codex_remote/` 里它是如何被启动的（谁在启动序列里调它），可作为方案 D 的参考实现。
本次未挖（只读了两处文档），需要时再取。

### 2.3 两条可直接借用的工程纪律（我们已经在做，得到印证）

- **烧录前先校验"镜像能不能装进目标分区"**：他们的 app-only 脚本会**先确认 app 位于 `0x200000` 且 ≤14 MiB**
  再调 esptool。我们目前只在刷前重算 sha256 + 靠 `write_flash` 边界，**没有显式做"尺寸 vs 槽位"断言**
  ⇒ 建议在夹具里补一条（正好也是我们的硬门禁项）。
- **`-DryRun` 默认先试**、以及**"打包成功 ≠ 真机验证完成"**、**"不要把 Token/凭据/NVS/flash dump 提交仓库"**：
  我们的 D0/D1 与 §34 脱敏是同一套纪律，**说明这套纪律是行业共识，不是 Codex 多余的要求**。

---

## 3. ⚠️ 不能直接抄的部分（重要）

- **ESP-IDF 大版本不同**：他们 **v6.0.2**，我们 **5.5.4**。API / Kconfig / 组件管理器行为都有差异，
  **不能把他们的代码搬过来**；能对齐的是**结构、分区策略、脚本流程与纪律**。
- 他们是**独立固件**（自带 `main/`、`sdkconfig`、上游 fork），我们是**在 pinned 上游上打补丁层**
  （`integration/metalio_claw4/patches/`）。两条路线不同，不能混用构建流程。
- 他们的 `partitions/v1/32m.csv` 与我们 `32m_dual.csv` 是**不同布局**，文件名差异（`32m` vs `32m_dual`）
  本身就点明了单槽/双槽的区别。

## 4. 给 Codex 的整合建议（新增裁定项）

| # | 建议 | 依据 |
|---|---|---|
| A | 把"**分区改为单槽 `factory` 14 MiB**"作为解除尺寸门禁的方案评估项；附代价（全量刷写 ⇒ NVS 全丢 + 厂商云 OTA 结构上不可行）与反悔成本 | 已部署项目的真实布局 + 我们 `ota_1` 实际不可用 |
| B | 在D 的实现里**参考"后台服务做成独立组件、不挂 UI 页"** | 他们的 `codex_remote` 组件 + 我们"复位后同步不自愈"的实测故障 |
| C | 在刷写夹具里补一条**显式断言：app 尺寸 ≤ 目标 app 槽位** | 他们的 app-only 脚本就是这个流程，我们目前缺 |
| D | 若评估后**不**改分区，则接受"每次新增代码都要过 165 KB 余量门禁"的约束，并把该约束写进 C03 的准入条件 | 双槽布局的必然结果 |

## 5. 来源

- AgentUI 仓库首页与用法（含"Codex → 菜单 → 连接 → 局域网 → Token"流程）：<https://github.com/AlayaElla/MetalioClaw4-AgentUI>
- AgentUI 开发与烧录文档（IDF v6.0.2、目录结构、`package-esp32.ps1` / `flash-esp32.ps1` / `flash-esp32-preserve-settings.ps1` / `monitor-esp32.ps1`、资源边界、许可证）：<https://github.com/AlayaElla/MetalioClaw4-AgentUI/blob/main/docs/development.md>
- AgentUI 分区表原文：<https://raw.githubusercontent.com/AlayaElla/MetalioClaw4-AgentUI/main/partitions/v1/32m.csv>
- CodexRemote 首页（Windows 11 x64 + .NET 9、虚拟麦克风驱动、TCP 8765 / UDP 8766、Token 鉴权、故障排查）：<https://github.com/AlayaElla/CodexRemote>
