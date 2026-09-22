# M0 网络恢复与统一候选阶段

2026-09-22，Codex。用户要求继续完成一阶段开发再安排 WorkBuddy 复核。本阶段选择 M0 中已确认的隐藏 SSID 重连缺口及其可复现构建，不越过 M0 开始 M1。

实现提交 `f47afda7a7fc2f2175e25a10f40fd65e9268d08b`，分支 `codex/v6-foundation`。状态 **REVIEW_READY（源码、Host、BUILD）**；设备验证未运行。candidate06 测试包已 HOLD，由 candidate07 统一承接音频与网络复核，不再单独刷 candidate06。

## 结果与边界

上游连接入口依赖扫描列表中出现已保存 SSID。隐藏网络可能不出现在该列表，导致设备虽然保存了凭据却没有发起定向连接。本阶段增加全信道扫描无匹配后的有界凭据回退；可见 AP 路径优先，保留现有 board 60 秒连接期限、扫描退避及配网页。

- 按保存顺序选择最多3个不同有效 SSID，跳过空/超长/含 NUL 的凭据，不截断后尝试连接。
- 定向连接使用未知信道0、全信道查找，不使用已过期的保存信道。固定 BSSID 偏好开启时不走回退，因为保存结构没有可用的权威 BSSID，不能凭空填0或取消限制。
- 每个回退项一次驱动连接调用，不额外连续重试同一项5次；失败推进下一项，耗尽后按原扫描退避。获得 IP 后使用原正常重连逻辑。三项是每轮上限，不宣称60秒内必能试完，也不宣称无限期离线恢复已经实现。
- 扫描列表改为自动生命周期容器，空列表不再对空 malloc 指针做排序；读取扫描结果报错时延迟重试，不依据缺失结果构造硬件结论。
- 新诊断只输出候选数/错误码，不输出凭据。原组件私密日志仍可能含网络名称，原始日志不能提交。

## 构建隔离

白名单：项目拥有的 `integration/v6/network/`、对应 tools/v6 构建/冻结/验证工具；生成目录仅为 `E:/v6/s1/components/78__esp-wifi-connect`。原始 `managed_components/78__esp-wifi-connect` 与 vendor 源码不修改。保留 MIT LICENSE。

`overlay.json` 固定组件3.3.1（源码 cc103899…）全文件 SHA 和替换锚点。未知版本/已有本地编辑/多重或缺失锚点均拒绝；生成本地 override 后检查完整清单。冻结另外核对 CMake 实际选中的组件路径，防止“补丁存在但没有被编入”。

本次依赖解析没有版本升级。锁文件变化是 WiFi 组件来源改为 local，以及 cjson registry URL 末尾斜杠规范化；cjson 版本及内容 hash 未变。原始 dependency lock 保留，候选记录本次构建锁 hash。

## 验证证据

解释器：`E:/workbuddy/claw4-v6/toolchains/idf61/python_env/idf6.1_py3.12_env/Scripts/python.exe`，工作区 `E:/workbuddy/claw4-v6`。

1. `-m unittest discover -s tools/v6 -p "test_*.py"`：**62 PASS**（含6项覆盖源漂移、额外文件、手改保护、幂等、锚点和策略漂移的 overlay 测试）。
2. `g++ -std=c++17 -Wall -Wextra -Werror tools/v6/test_saved_network_policy.cc -o out/v6-network-policy-test.exe` 并运行：PASS。使用 w64devkit-2.9.1 同目录 PATH；覆盖候选顺序、上限、去重、长度/NUL边界、空配置和固定BSSID限制。
3. `tools/v6/test_network_station.py --source E:/v6/s1 --cxx E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe`：**7个实际生产方法 Host 场景 PASS**。从生成代码提取 HandleScanResult/StartConnect 方法体，在假驱动上执行，非另一份模拟算法；覆盖全扫描直连、局部扫描先升级全扫描、pin限制、3次立即失败后退避、读取扫描列表失败、可见AP正常路径、空凭据。初次fixture缺少SsidItem别名/bzero导致编译失败，补齐Host shim后通过。
4. `tools/v6/build_device.py --source E:/v6/s1 --incremental`：IDF构建 exit=0，输出 `out/v6-device-build-11.txt`。构建记录确认选择 local override。
5. `freeze_candidate.py --source E:/v6/s1 --output integration/v6/m0-candidate-07.json`：PASS。原板级overlay一致，关键配置一致，原记录的9个上游运行时文件保持不变。
6. `flash_plan.py --backup out/v6-device-private/pre-v6-full-flash.bin --build E:/v6/s1/build --output out/v6-candidate07-flash-plan.json`：PASS，只核对备份与分区范围，不写设备。

Host shim不模拟完整事件调度、射频、C5、断线与Stop的竞争。没有证明隐藏网络关联成功、密码错误时实际超时、掉线后重连或配网切换正确。这些是 WorkBuddy 复核的必要范围，不能用上述 PASS 代替。

## 候选07

- app：3,051,184字节，SHA256 `6d1e169a09778a76012c2b945a05e7d4fabe1985baa468eaf6e8b2b1c05960d1`。
- ELF：`4b4ed0db0ac70b1c224ba8e3f8e69a2269a1aeea0a64d97afbf3d3f00cfce0c1`。
- bootloader/分区/models/assets 四项与候选06逐字节哈希一致；只允许后续 app-only 准备写入。
- 包含候选06的音频归一化和播放同时采集修订；音量/唤醒回归不可跳过。
- **本阶段未打开串口、未刷机、未运行真机测试。** 设备仍不能写成候选07已部署。M0整体、M1门禁及SD/Camera/电源键等缺口均保持。

统一复核入口：`docs/project_management/tasks/WB-V6-M0-CANDIDATE07-REVIEW.md`。本地任务包已发布，没有通过外部工具发送给WorkBuddy。
