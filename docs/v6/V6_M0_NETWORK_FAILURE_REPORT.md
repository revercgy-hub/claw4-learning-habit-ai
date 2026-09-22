# M0 网络失败恢复增量

2026-09-22，Codex。用户要求“继续开发1部分”。范围限定网络启动与扫描的同步失败处理，基于 `6286d2f`；状态 CODE/HOST/单编译单元验证通过，待后续候选整机链接及设备复核。

## 修复

- 原候选07仅为隐藏网络处理 esp_wifi_connect 同步错误，可见网络仍 ESP_ERROR_CHECK；esp_wifi_set_config 失败也会 abort。新 StartConnect 对两类网络统一处理配置/启动错误：失败推进已有队列，耗尽后退避，配置失败绝不使用上一份配置调用 connect。
- 队列消费改为循环。可见BSSID较多或连续错误时不会递归积累栈；空队列调用也安全地进入退避。隐藏网络三项上限、可见网络优先、固定BSSID限制保持。
- 全信道扫描启动失败会安排延后扫描。扫描完成事件的 status 失败或事件载荷缺失时，清理旧候选/扫描列表并退避，不把失败当作成功空扫描而触发隐藏网络回退。
- 新增日志只含错误码，不增加SSID/密码输出。NVS、凭据格式、60秒board连接期限和上游运行时所有权不改。

权威变更在 `integration/v6/network/overlay.json`，仍以原始依赖全文件哈希和逐项唯一锚点生成本地补丁。未修改 `E:/v6/s1`，候选07镜像及其组件继续供固定基点复核使用。

## 验证

在 E:/workbuddy/claw4-v6 使用既有 IDF Python：

1. `tools/v6/test_network_station.py --source E:/v6/s1 --cxx E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe`：实际生成的生产方法体 **16场景PASS**。新增空队列、隐藏/可见配置失败、可见连接启动失败、第一组配置失败后第二组成功、失败扫描完成事件、全扫描启动失败/成功、正常扫描完成事件，共9场景。
2. `-m unittest discover -s tools/v6 -p "test_*.py"`：**62 PASS**。
3. 按原 IDF `build/compile_commands.json` 中 wifi_station.cc 的实际参数，将组件源码/include替换为私密临时生成目录，输出对象重定向到 `out/v6-network-failure-compile/wifi_station.obj`；使用同一 riscv32-esp-elf-g++ **编译 exit=0**。实际命令及日志位于该目录 command.txt/compiler.txt。完整 wifi_station.cc（含ESP事件回调）和实际IDF头文件通过编译，不仅是Host shim。

第3项只编译一个生产单元，不生成/链接新固件，不冒称整机BUILD通过。Host shim不模拟异步事件先后、Stop竞争、RF、C5或AP实际行为。尚需下次统一候选验证普通/隐藏网络、错误密码/断网、配网超时及音频回归。

## 交接与候选边界

本次未刷机、未打开串口、未覆盖候选07构建目录。07冻结清单不包含本增量，仍属于实现 f47afda；WorkBuddy不能用07真机结果验收本增量。现有07包保持固定基点，追加代码复核需单独记录本增量提交；下次合并候选后统一完成DEVICE验证，不能把当前源码直接配上07二进制。
