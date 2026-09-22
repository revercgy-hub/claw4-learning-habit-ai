# Candidate 08 统一构建

2026-09-22。用户要求将网络失败恢复合入新固件统一构建。实现 `500242c7439767e2c45deb82f420f147974121cf`，冻结提交 `1907730926d383072dc267201c6f359c4a001231`，分支 codex/v6-foundation。

**CODE / HOST / BUILD 完成；DEVICE NOT_YET_RUN。** 本轮未打开串口、未刷机。此报告取代网络失败恢复增量“仅单编译单元验证、尚未整机链接”的状态；未替代任何真机验收。

## 合入与验证

- 候选08包含候选06音频归一化/播放同期采集、候选07有界隐藏SSID回退，以及500242c中的扫描/配置/连接同步失败恢复。
- 构建前未发现运行中的Python/IDF/ninja/esptool进程。先按候选07清单核对构建文件和组件全库存，再归档到 `out/v6-candidate07-frozen/`；归档文件逐项SHA验证通过。只在旧组件完全匹配时更新本地生成的 wifi_station.cc 和 include/wifi_station.h，原 managed_components/vendor 未改。
- 使用既有IDF解释器运行 `tools/v6/build_device.py --source E:/v6/s1 --incremental`：完整reconfigure/build/link exit=0；日志 `out/v6-device-build-12.txt`。
- `python -m unittest discover -s tools/v6 -p "test_*.py"`：62 PASS。
- `test_network_station.py --source E:/v6/s1 --cxx E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe`：16生产方法Host场景PASS。
- freeze检查：板级overlay一致、关键配置满足要求、原记录9个上游运行时文件未改、网络本地override全文件一致，CMake确实选择本地组件。候选清单 `integration/v6/m0-candidate-08.json`。
- flash_plan只读核对32MiB完整备份、分区布局和镜像边界通过，结果 `out/v6-candidate08-flash-plan.json`。未执行该计划。

## 固件身份与保存

| 项目 | 结果 |
| --- | --- |
| application | 3,051,504 bytes |
| app SHA256 | 1ef710aec525df3f91142d9ca6188b26e6881e448e72007804515a7f6ae6e7c2 |
| ELF SHA256 | 50fb62c30f8e82797f40ee6e3ab085cebc9281986883db06823d61da761708ca |
| bootloader / partition / models / assets | 四项与候选07清单逐项相同 |
| 当前构建 | E:/v6/s1/build/xiaozhi.bin |
| 冻结归档 | E:/workbuddy/claw4-v6/out/v6-candidate08-frozen/build/xiaozhi.bin |

候选08归档包含清单列出的所有构建文件及网络组件副本，所有构建文件复制后逐项校验SHA。归档/二进制/私密日志不提交Git；Git只保存清单与文档。启动日志可能只显示ELF短前缀，不称完整SHA。版本字符串沿用M0构建版本，必须靠清单/实际hash识别候选。

## 下一步

唯一入口 `docs/project_management/tasks/WB-V6-M0-CANDIDATE08-REVIEW.md`：从冻结提交独立审查后，一次app-only准备写入并统一验证音频、联网失败/隐藏网络、USB复位与观察。06/07包已HOLD/SUPERSEDED。旧07归档保留供复核，不是继续执行旧包的授权。

新固件尚未证明真机音量、唤醒、参考通道、隐藏网络或异步断线行为；M0整体与M1门禁保持。任务包仅在本地仓库发布，没有向外部WorkBuddy自动发送消息。
