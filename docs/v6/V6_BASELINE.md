# V6-M0 源码候选基线

2026-09-21；工作区 `E:/workbuddy/claw4-v6`，分支 `codex/v6-foundation`。来源为远端最新 A05 取证分支，不从旧 main 或 HEAD 损坏目录起步。原工作区未修改。

| 输入 | 版本 / 精确提交 | 证据与限制 |
| --- | --- | --- |
| Learning | `5657ebed64ad5962c889fcf90f0979f5891ab862` | `origin/workbuddy/a05-build-m0`，本轮 fetch/ls-remote；继承实现未整体重新验收 |
| XiaoZhi | v2.5.0 / `ac6deed3d8e75348475364bf40ad953c6cd48054` | 发布 tag；独立 vendor checkout，干净树已核对 |
| ESP-IDF | v6.1 / `fff9895c82d744c7237be8847347bdd1b07c6643` | annotated tag 的 peeled commit；version.cmake=6.1.0；仅源码，submodules/工具链未安装完整 |
| Metalio | `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950` | 原 vendor 干净树；历史板级参考，不声称当前最新 |

权威机器清单：[baseline.lock.json](../../integration/v6/baseline.lock.json)。其空的 component lock、toolchain manifest、partition 字段是未完成门槛，不能作为可重复设备构建已通过。

官方依据：[XiaoZhi v2.5.0](https://github.com/78/xiaozhi-esp32/tree/v2.5.0)、[IDF v6.1](https://github.com/espressif/esp-idf/releases/tag/v6.1)。冻结后使用 SHA；不跟随浮动 HEAD。XiaoZhi manifest 目前固定 ESP-SR=2.4.7，ESP-Hosted ^2.12.11，wifi_remote ^1.6.4；范围约束不是解析后锁文件，首次构建必须保存 dependencies.lock 和摘要。

## 环境与设备事实

- Host 验证：Windows / PowerShell，GNU C++ 16.2.0，CMake 3.30.2；Python 3.12 位于既有 IDF5.5 环境，仅用作 stdlib 工具，绝不冒充 IDF6.1 环境。
- 历史设备：P4 rev1.3，需 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`；C5 SDIO 参数与固件协议兼容须重新核实。
- 9/20 本机 `E:/workbuddy/CODEX-CLAW4/Claw4-物理主机执行记录.md` 报告 32 MiB、factory `0x200000` / `0xE00000`，IDF6.0.2；与旧 A05 ota_0 9 MiB 不同。它是另一批历史记录，尚未本轮重读设备，不据此生成刷写命令。
- 本轮未连接串口、未构建/刷写设备。历史私密 Flash 备份不纳入 Git；后续核对 hash、设备对应关系与可恢复性，不能只凭“存在备份”宣布恢复路径通过。
- 当前锁只固定源码；M0-1 的构建环境/依赖/恢复基线未齐，M0 不通过。

## 重复验证

```powershell
python -m unittest discover -s tools/v6 -v
python tools/v6/baseline.py --xiaozhi vendor/xiaozhi-esp32 --idf vendor/esp-idf --metalio <clean-metalio-checkout>
cmake -S integration/v6/learning_core -B out/v6-core
cmake --build out/v6-core
ctest --test-dir out/v6-core --output-on-failure
```

Windows MinGW 需将其 bin 放进当前进程 PATH，并选择 `-G "MinGW Makefiles"`；否则 GCC 找不到 assembler。这一环境修复不改变系统 PATH。首次失败及修复记录见 [本轮报告](V6_FOUNDATION_REPORT.md)。
