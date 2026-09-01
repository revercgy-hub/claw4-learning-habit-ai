# Claw4 本地 Bring-up 准备清单

> 配套文档：`CLAW4_BRINGUP_PROMPT.md`（Bring-up 执行手册）
> 生成时间：2026-08-31
> 依据：本机实测环境 + 官方仓库 `CloudZao/MetalioClaw4` @ `ca3aa3f`（tag: latest, 2026-08-18）

---

## 0. 结论速览：本机当前状态体检

| 检查项 | 实测结果 | 状态 |
|:---|:---|:---:|
| 官方仓库 | `vendor/MetalioClaw4`，commit `ca3aa3f`，无 submodule | ✅ 已就绪 |
| 官方出厂固件 | `Metalio_Claw4_Latest.bin`（31.58 MB，完整 32MB flash 镜像） | ✅ 已就绪 |
| Git | 2.55.0 | ✅ |
| ESP-IDF | **未安装**，`IDF_PATH` 未设置 | ❌ 阻断 |
| 编译工具链 | `cmake` / `ninja` / `idf.py` / `esptool` 均无 | ❌ 阻断 |
| 串口 / 设备连接 | 未检测到任何 COM 设备 | ❌ 阻断 |
| 磁盘空间 | C:256G / D:367G / E:639G 可用 | ✅ |
| 系统 Python | 3.13.14、3.14.3（**版本偏新，有风险**） | ⚠️ 需注意 |
| 仓库所在路径 | `E:\workbuddy\学习习惯培育AI\vendor\...`（**含中文**） | ⚠️ 建议迁移 |

**一句话：固件和源码已到手，工具链和实机连接是全部缺口。按 Bring-up Prompt 第二十五节要求，五项前置（设备串口 / USB连接 / 本地仓库路径 / ESP-IDF环境 / 烧录权限）中，仓库路径已解决但路径不合规，其余四项待办。**

---

## 1. 硬约束版本矩阵（不可妥协）

官方仓库已锁死配置，以下全部来自仓库实测文件，**不是推测**：

| 项 | 锁定值 | 来源 |
|:---|:---|:---|
| ESP-IDF | **v5.5.4**（必须与 sdkconfig 匹配） | README §13.1 |
| Target | `esp32p4`（已预配置） | `sdkconfig` |
| **不可执行 `idf.py set-target`** | 会破坏预置配置 | README §13.3 |
| Flash | 32 MB / QIO / 40 MHz | `sdkconfig` |
| PSRAM | HEX 模式 / 200 MHz | `sdkconfig` |
| **分区表偏移** | **`0x9000`**（不是默认的 `0x8000`） | `sdkconfig` |
| 分区表文件 | `partitions/v1/32m_dual.csv` | `sdkconfig` |
| 芯片 revision | `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` | `sdkconfig` |
| 屏驱 | `NV3051F`（默认）/ `FL7707N`（备选宏） | README §13.4 |
| C5 网络 | ESP-Hosted SDIO Slot1、4-bit、40 MHz | README §7.2 |

> 底层约束：ESP32-C5 v1.0 自 ESP-IDF **v5.5.2** 起才支持量产；ESP32-P4 rev v3.2 需 **v5.5.3+**。所以 v5.5.4 是刚需，装 v5.4 或 v5.3 会在 C5 上直接翻车。

---

## 2. P0 阻断项（不做就跑不起来）

### P0-1 安装 ESP-IDF v5.5.4

**推荐方案：Windows 原生 + 乐鑫离线安装器**（而非官方 README 提的 WSL2）

理由：设备四个调试口全部走 USB，Windows 原生可直接识别 COM，WSL2 还要配 `usbipd-win` 做 USB 转发，多一层故障点。Bring-up 阶段每多一层抽象就多一个排查盲区。

1. 下载：乐鑫官方 Windows 工具链安装器（离线版）
   文档入口：https://docs.espressif.com/projects/esp-idf/zh_CN/v5.5.4/esp32p4/get-started/windows-setup.html
2. 安装路径用 **`D:\Espressif`**（纯 ASCII、无空格、非 OneDrive 目录）
3. 安装时勾选 **由安装器托管 Python**（不要让它用系统的 3.13/3.14，见"坑 2"）
4. 安装完成后打开 **ESP-IDF 5.5 PowerShell**，验证：
   ```bash
   idf.py --version          # 期望 ESP-IDF v5.5.4
   echo $IDF_PATH            # 期望 D:\Espressif\frameworks\esp-idf-v5.5.4
   xtensa/riscv 工具链版本
   ```

### P0-2 迁移仓库到纯 ASCII 路径

当前 `E:\workbuddy\学习习惯培育AI\vendor\MetalioClaw4` 含中文，**CMake/Ninja/组件管理器在中文路径下的失败案例极多**，而 Bring-up 阶段最忌讳把"路径编码问题"误判成"硬件问题"。

```bash
# 建议迁移到（仓库代码放这里）
D:\claw4\MetalioClaw4

# 本工作区只保留文档、脚本、测试报告（中文路径只影响文档，不影响构建）
E:\workbuddy\学习习惯培育AI\
```

迁移后立刻校验（这一步也顺便验证了工具链）：

```bash
cd D:\claw4\MetalioClaw4
idf.py build          # 只编译，先不烧
```

**只要 `idf.py build` 能跑通，P0-1 和 P0-2 就算完成，且官方 baseline 成立。**

### P0-3 实机连接与串口识别

设备连电脑并**通电**后，系统会出现**四个**串口，必须按描述符认，不能按 COM 号猜（COM 号每次可能变）：

| 用途 | Windows 设备管理器里的描述符 | Bring-up 用途 |
|:---|:---|:---|
| **P4 主控烧录 + 日志** | `USB JTAG/serial debug unit` | **B001~B013 全靠它**，选这个口 |
| 蓝牙芯片烧录 | `USB Serial`（CH340K） | B012（不急） |
| 4G 模组日志 | `log` | B012 / 4G（非 MVP 准入） |
| 4G AT 指令 | `at` | 同上 |

- 线缆必须是**带数据线的 USB-C**，纯充电线插上没有任何口出现。
- 设备管理器里若 `USB JTAG/serial debug unit` 显示为未知设备/带感叹号，需要装驱动（WinUSB）。日常 `idf.py flash monitor` 走 COM 口即可，**暂不需要 Zadig 或 OpenOCD**。
- 记录实际 COM 号，后面所有命令都要用。

### P0-4 建立"可回滚"基线（最重要的一条）

你手上这个 `Metalio_Claw4_Latest.bin` 是**完整 32MB flash 镜像**（已验证：bootloader magic `0xE9` 位于偏移 `0x2000`，符合从 `0x0` 开始的整体镜像布局）。

**这意味着一条命令就能把设备救回出厂状态**，是所有后续折腾的安全网：

```bash
esptool.py --chip esp32p4 -p COMx -b 921600 write_flash 0x0 Metalio_Claw4_Latest.bin
```

准备动作：
1. 把该 bin 复制到 `D:\claw4\firmware\baseline\` 并**只读备份一份**。
2. 记录其 SHA256。
3. **在动手改任何东西之前，先跑一遍这条恢复命令 + 抓一次启动日志**，确认设备出厂状态完好。这一步的日志就是 B013 的基线日志。

---

## 3. P1 强烈建议

| # | 事项 | 为什么 |
|:--|:---|:---|
| P1-1 | **C5 从机固件**（详见"坑 1"） | 仓库里**没有** C5 固件，README 也没给烧录步骤。Wi-Fi 不工作八成是它 |
| P1-2 | 日志落盘工具 | Bring-up 90% 的结论来自日志。用 `idf.py monitor > log.txt` 或 TeraTerm/Xshell 记录会话 |
| P1-3 | 准备 2.4G + 5G 双频 Wi-Fi SSID/密码 | B009 要测双频；另备一台手机热点用于"路由器重启/AP切换"场景 |
| P1-4 | microSD 卡（FAT32，16–32GB）+ 读卡器 | B008 要测 1KB/1MB/10MB 吞吐与断电恢复 |
| P1-5 | 设备充满电 + 全程接 USB 供电 | B011/B014（1h/8h/24h）期间断电等于白跑 |
| P1-6 | 建立 `AGENTS.md` | Bring-up Prompt 第一节要求开跑前先读它，目前工作区没有 |

### 日志落盘的正确姿势

```bash
# Windows PowerShell， Ctrl+] 退出 monitor
idf.py -p COMx monitor 2>&1 | Tee-Object -FilePath "docs\logs\boot_$(Get-Date -Format yyyyMMdd_HHmm).log"
```

---

## 4. P2 可选加分

| 事项 | 用途 |
|:---|:---|
| USB 电流表 / 万用表 | B011 电源测试，能测真实电流就不用只靠 SOC 推算 |
| GPU 温度/室温记录 | 长稳测试的干扰变量 |
| 备用 Claw4 或第二台同型号 | 区分"个体故障"和"批次/SKU 差异" |
| 独立 5GHz 测试 AP | 排除主路由干扰 |

---

## 5. 目录与路径规范

```
D:\claw4\                          ← 纯 ASCII，代码与工具链（构建相关）
  ├─ MetalioClaw4\                 ← 官方仓库（可构建）
  └─ firmware\baseline\            ← 出厂固件只读备份

D:\Espressif\                      ← ESP-IDF v5.5.4（安装器默认位置附近）
  └─ frameworks\esp-idf-v5.5.4\

E:\workbuddy\学习习惯培育AI\        ← 本工作区：只放文档与脚本
  ├─ docs\                         ← 七份 Bring-up 产出文档
  │   └─ logs\                     ← 抓到的串口日志
  ├─ tools\bringup\                ← 预检脚本、基线采集脚本
  └─ 项目总规划\
```

---

## 6. 三条官方捷径（别自己造轮子）

官方固件已经内置了 Bring-up 需要的大部分观测能力，**先用起来，别急着写测试代码**：

### 捷径 1：System Monitor 每秒打点 → 白送 B002 + B014

README §15.2：板级初始化后，后台任务**每秒**打印 CPU 占用、空闲内存、电池状态。

也就是说 B002（内存体检）和 B014（1h/8h/24h 长稳、每 60 秒采样 free heap / largest block / uptime）**直接抓日志就能出数据**，不需要额外写采样固件。

### 捷径 2：内置工厂测试 App → 白送 B004 + B005 + B012

README §15.3：首页 **Pin Test**（GPIO/外设连通性快检）与 **Test**（工厂入口：自动测试、压力测试、硬件测试）。

先跑一遍官方自检，再决定哪些模块需要自己写用例。这比从零写测试页面快一个数量级。

### 捷径 3：15 个日志标签 → 直接构成 B013 日志基线

README §15.1 给了完整标签表（`METALIO_CLAW_4` / `IOExpander` / `GpsService` / `CameraScreen` / `System Monitor` 等），按标签过滤日志即可建立 `DEVICE_LOG_REFERENCE.md` 的骨架。

---

## 7. 六个已知坑（按踩中概率排序）

### 坑 1：C5 从机固件缺失 —— 最大风险

仓库里**没有任何 C5 固件**（已全量搜索 `*c5*` / `*slave*` / `*hosted*`，零命中），README 只说"Wi-Fi 不扫描就检查 C5 固件有没有烧"。

**推论**：出厂时 C5 应已烧好固件，否则设备联网功能根本不可用。所以——

> **策略：第一轮 Bring-up 必须跑出厂固件（`Latest.bin`），不要一上来就 `idf.py build && flash`。**
> 自建固件只烧 P4 侧，若把 C5 弄坏且没有备份，Wi-Fi 直接变砖，且仓库救不了你。

待办：向厂商/社区确认 C5 从机固件的获取途径与烧录方式（大概率需要单独 clone `espressif/esp-hosted` 的 slave 工程编译）。

### 坑 2：系统 Python 版本过新

本机有 Python **3.13.14** 和 **3.14.3**。ESP-IDF 的组件管理器与部分依赖对新版本 Python 适配滞后，容易出现包安装失败或运行时报错。

**对策**：让 ESP-IDF 安装器托管自己的 Python（v5.5.4 通常带 3.11.x 嵌入版），不要指望系统 Python。若坚持手动安装，单独装 **Python 3.11 或 3.12** 给 IDF 专用。

### 坑 3：分区表偏移是 `0x9000`，不是 `0x8000`

`CONFIG_PARTITION_TABLE_OFFSET=0x9000`。任何手工烧录（尤其 ESPClaw 的 `edge_agent`）若按默认 `0x8000` 烧，分区表错位，设备必挂。所有 esptool 命令都要显式对齐。

### 坑 4：虚拟 U 盘与调试串口互斥

SD 卡 App 里启用"虚拟 U 盘"后，GPIO24/25 切换为 USB MSC，**`USB JTAG/serial debug unit` 口直接消失**，此时无法烧录也无法看日志。

**纪律**：Bring-up 全程**不要进 SD 卡页面启用虚拟 U 盘**；若已在 monitor 中，先 `Ctrl+]` 退出再操作。

### 坑 5：sdkconfig 已存在不一致迹象

实测发现：

```
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHMODE="dio"     ← 与上一行矛盾
```

QIO 被选中但字符串值是 `dio`，说明该文件可能被手工编辑过。**这正是 Bring-up Prompt 第二节要记录 `sdkconfig hash` 的原因**——先记录、别急着改，若后续出现 Flash 相关异常，这里是第一个排查点。

### 坑 6：屏幕驱动有 SKU 差异

默认 `NV3051F`，备选 `FL7707N`（宏 `METALIO_CLAW_4_USE_FL7707N`）。若实测屏幕不亮，先怀疑 SKU 差异，而不是先怀疑接线——这直接对应 Bring-up Prompt 的 `SKU_DEPENDENT` 分级。

另外：`32m_dual.csv` 里**没有 `factory` 分区，只有 `ota_0` / `ota_1`**，这已直接回答了 B003 关于 OpenClaw/ESPClaw 双模式的疑问（确实用 ota_0/ota_1 切换）。

---

## 8. 缺失项汇报（按 Bring-up Prompt 第二十五节要求）

| 前置条件 | 状态 | 待办 |
|:---|:---:|:---|
| 本地仓库路径 | ⚠️ 有但不合规 | 迁至 `D:\claw4\MetalioClaw4` |
| ESP-IDF 环境 | ❌ 缺 | 装 v5.5.4（P0-1） |
| 设备串口 | ❌ 缺 | 接设备、认 `USB JTAG/serial debug unit` 口 |
| USB 连接 | ❌ 缺 | 确认数据线带数据功能 |
| 烧录权限 | ❓ 待验 | 首次 `esptool` 连通即确认 |

**当前判定：不具备开跑 B001~B013 的条件。按 Prompt 要求在此停止，不猜测硬件规格。**

---

## 9. 完成 P0 后的首个动作序列

```bash
# 0) 打开 ESP-IDF 5.5 PowerShell，确认环境
idf.py --version

# 1) 基线快照（自动记录 commit / IDF 版本 / sdkconfig hash / 分区表）
cd E:\workbuddy\学习习惯培育AI
bash tools/bringup/capture_baseline.sh D:/claw4/MetalioClaw4

# 2) 打基线标签（Bring-up Prompt 第二节要求）
cd D:\claw4\MetalioClaw4
git tag bringup-baseline
git tag                       # 确认

# 3) 恢复出厂固件，确认设备完好（先烧 P0-4 那条命令）
esptool.py --chip esp32p4 -p COMx -b 921600 write_flash 0x0 ..\firmware\baseline\Metalio_Claw4_Latest.bin

# 4) 抓首份启动日志（B013 基线日志）
idf.py -p COMx monitor 2>&1 | Tee-Object -FilePath "E:\workbuddy\学习习惯培育AI\docs\logs\boot_factory.txt"

# 5) 环境预检全绿后，才正式开跑 B001
bash tools/bringup/preflight.sh
```

---

## 10. 需要你确认的 3 件事

1. **Claw4 实机是否已到手？** 目前本机未检测到任何串口设备。若未到手，P0-3/P0-4 只能挂起。
2. **C5 从机固件有没有独立获取渠道？** 仓库里没有。若有厂商给的 C5 固件和烧录方式，请提供——这是 Wi-Fi 相关测试的唯一风险点。
3. **是否接受把仓库迁到 `D:\claw4\`？** 若必须在当前中文路径下构建，我需要额外安排路径编码问题的排查预案。
