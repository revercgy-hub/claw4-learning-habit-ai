# Metalio 集成改动登记表（integration_manifest）

> 政策：`docs/METALIO_ADAPTER_BRIDGE_PLAN_P17.md` §2B（底层少动，上层深做）。
> 任何 **Metalio 官方文件**改动必须在触碰前登记本表并满足：
> - 仅限允许范围（Home App Registry / Learning Screen registration / CMake component·source registration / 必要 include·build glue）；
> - 单条可回滚（最小 diff、可反向/checkout）；
> - 严格禁止：BSP / board driver / MIPI / GT911 / Audio low-level / ESP-Hosted / Power driver / sdkconfig / partition CSV / bootloader / ota_1 / eFuse / Secure Boot / Flash Encryption。

| 序号 | upstream commit | file | lines/functions | reason | rollback | risk | 状态 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| （暂无——尚未触碰任何 Metalio 官方文件） | | | | | | | |

- 正式 pin（复核于 2026-09-03）：`ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950`（vendor/MetalioClaw4 HEAD 与此一致，无上游漂移）。
- 新增 repo 侧（非官方文件，无需登记）：`integration/metalio_claw4/host_glue/*`（P17a，host 可测）、`firmware/tests/unit/metalio/learning_app_glue_tests.cpp`。
