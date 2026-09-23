# V6 M0 候选10构建与真机验证

日期：2026-09-23。候选10由复检修复提交 `26f60ec` 构建。候选09过渡镜像已标记作废：网络 overlay 已编译，但诊断字段修正是在构建后才同步，故不作为统一候选或验收依据。

## 变更

- Wi-Fi 断线重连检查 `esp_wifi_connect()` 同步返回值；调用失败时停止将其当作异步重连，转入有界候选队列或退避扫描。
- HEALTH 字段由 `vad_observable` 改为 `local_playback_active`，值仍表示本地 Playback 状态。

## 构建与软件验证

- IDF 6.1，目标 ESP32-P4，完整重配置、编译、链接通过。
- `python -m unittest discover -s tools/v6 -p "test_*.py"`：62 项通过。
- `test_network_station.py --source E:/v6/s1 --cxx E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe`：19 个生产方法 Host 场景通过，包括直接执行生产 `WifiEventHandler` 的同步重连成功、同步失败进入退避、同步失败后推进有限候选队列三种场景。该故障仍未在真机上注入。
- 冻结清单：[`integration/v6/m0-candidate-10.json`](../../integration/v6/m0-candidate-10.json)。

## 真机结果

- 设备身份复核为 ESP32-P4 revision v1.3、32 MiB；完整备份 SHA256 与既有恢复清单一致。
- 只写 application 分区 `0x200000`，未写 bootloader、分区表或 NVS。esptool 写入校验通过。
- 读取 application 分区 3,051,568 字节后，SHA256 与构建镜像相同：`2fbed0cbe87b5f4f7ffbbc54345e5d74d0c1d99be63155794790b92082deb153`。
- 60 秒启动捕获确认候选 ELF 短哈希 `511026ed4`、`Assets applied=1`、`BOOT_READY`，未发现 panic/abort/BOOT_BLOCKED。
- 网络在保存信道无匹配后执行全信道扫描及有界隐藏网络回退，最终取得 IP；全过程 `NETWORK_EVENT` 从启动推进到已连接。该会话再次确认候选08已覆盖的正常隐藏网络路径。
- HEALTH 日志出现 `local_playback_active=0`，字段名修正已进入实际运行镜像。
- 两次无语音的按钮周期均有 `TOUCH`、3 秒录音、Playback queued、`LOCAL_REFERENCE_PROBE_END drained=1`；播放期间 `tx_frames` 增至 94,080 / 141,120，RX 仍采集。因录音里没有说话，这两次播放未听到声音符合输入内容，不能据此判定扬声器故障。
- 随后用户按提示在录音期间说短句并确认听到回放：**主观可闻 PASS**。该次听感确认没有与串口捕获同时记录，故不附加为同步的电平/削顶证据。
- 两次 playback drain 后，捕获末段 HEALTH 保持 `wake=1`，说明录放后唤醒仍处于 armed 状态。输入参考 ch1 仍为零，不据此宣称参考/AEC 通过。

## 未完成验证

- 候选10的播放可闻性已由用户确认；但这次确认未与串口捕获同步，受控正常音量削顶余量及清晰度仍为 `NOT_VERIFIED`。候选08以来的归一化静态算术核对不能替代这些指标。
- 未对异步断线事件中的 `esp_wifi_connect()` 同步失败做硬件故障注入；当前普通隐藏网络重连实测通过，修复代码已编译并链接。
- 不宣称 M0 总通过，不重复候选05已经完成的复位长稳基线，也不进入 NAS/M1。

原始串口捕获、回读二进制、完整备份和刷写计划均留在被 `.gitignore` 覆盖的 `out/v6-device-private/`。本报告不包含原始网络名称或凭据。
