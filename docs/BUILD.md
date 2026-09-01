# Claw4 官方基线构建说明

## 已验证状态（2026-08-31）

官方 MetalioClaw4 快照已在 Windows 上完成一次 **ESP32-P4 无修改构建**。生成物为 `xiaozhi.bin`，大小 `9,036,192` bytes（约 8.62 MiB）。这证明主机工具链、锁定依赖和当前官方源码可以协同构建；不代表任何实机功能已经验证。

| 项目 | 已验证值 |
| --- | --- |
| 官方源码 Commit | `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950` |
| ESP-IDF | v5.5.4（ESP32-P4 工具链） |
| 构建目标 | `esp32p4` |
| 项目版本 | `2.0.51` |
| 应用二进制 | `E:\\b\\xiaozhi.bin` |
| Bootloader | `E:\\b\\bootloader\\bootloader.bin`（20,416 bytes） |
| 分区表 | `partitions/v1/32m_dual.csv` |

## Windows 路径约束

项目根目录含中文。此官方工程的依赖很多，Windows 会在深层构建时触发非 ASCII 路径或单条命令过长的问题。因此当前机器将**只用于构建的隔离镜像**放在短 ASCII 路径：

| 路径 | 角色 | 是否是日常编辑入口 |
| --- | --- | --- |
| `E:\\c` | 官方源码的隔离构建镜像 | 否 |
| `E:\\i` | ESP-IDF v5.5.4 的隔离镜像 | 否 |
| `E:\\b` | 生成的构建产物 | 否 |
| `E:\\workbuddy\\claw4-idf-tools` | 已安装的 ESP32-P4 工具与 Python 虚拟环境 | 否 |
| `E:\\workbuddy\\学习习惯培育AI\\vendor\\MetalioClaw4` | 已锁定的官方源码证据快照 | 是（只读基线） |

不要在 `E:\\b` 中编辑源码，也不要把生成物直接当作可安全刷入任意设备的固件。若官方源码快照更新，需要先重新同步隔离镜像，再重新跑基线构建。

## 重跑构建

在项目根目录执行：

```powershell
.\tools\bringup\build-official-baseline.ps1
```

脚本只配置环境并构建 `E:\\c` 到 `E:\\b`；不连接串口、不刷机、不修改项目内官方快照。首次变更 SDK/隔离镜像后可加 `-Reconfigure`。

## 已知 OTA 限制

当前官方分区的 `ota_0` 为 9 MiB、`ota_1` 为 4 MiB。构建的应用为约 8.62 MiB：

- `ota_0` 可容纳当前镜像；
- `ota_1` 溢出 `0x49e1a0` bytes。

这是官方构建输出的警告，不是本项目作出的分区修改。真机到货后要先确认实际启动分区、官方 OTA 流程和恢复方式；在此之前禁止调整 partition table，也不要承诺双槽 OTA 可用。

## 真机连接后

在确认未改动官方固件能够启动前，不刷自定义代码。P4 调试端口预计会在 Windows 中显示为 `USB JTAG/serial debug unit`；示例：

```powershell
idf.py -p COMx flash monitor
```

将 `COMx` 替换为实际 P4 端口。蓝牙模块和 4G 的独立串口不可用于 P4 固件烧录。

## 安全边界

- 不运行 `idf.py set-target`，仓库已有正确目标配置。
- 不修改 `sdkconfig` 或 partition CSV。
- 不烧录 `Metalio_Claw4_Latest.bin` 覆盖当前设备，除非先保存原始基线、确认来源与恢复方案。
