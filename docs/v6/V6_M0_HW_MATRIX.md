# V6 M0 硬件验收矩阵（自动生成）

> 生成时间 2026-09-22T03:21:45+00:00；schema `claw4-v6-m0-hw-matrix/1`。
> 冻结候选 `m0-candidate-04.json`，应用 SHA256 `196d8718a211e660…`。
> 证据来源 10 份启动捕获 / 1 份刷写捕获。
> 数值型信号按**跨捕获取最大值**合并，避免后一份没点击的日志抹掉前一份的触摸证据；
> 缺信号一律 NOT_VERIFIED，绝不由相邻项推断。

| 项目 | 证据 | 状态 | 来源 | 说明 |
| --- | --- | --- | --- | --- |
| Flash / PSRAM | `candidate manifest + flash log 'Hash of data verified.'` | **NOT_VERIFIED** | — | verified writes in this capture: 1; long-run stress still not exercised |
| 启动 | `V6M0: BOOT_READY IDF=v6.1;` | **NOT_VERIFIED** | boot-m0-02.txt | assertions=2; longest capture 110.7s |
| 显示 | `Claw4V6: Native RGB888 display, panel double buffers, full refresh` | **PASS** | boot-m0-04.txt | display init=yes, user confirmed visible=yes, unsupported-capability errors=2 |
| 触摸 | `Claw4V6: GT911 touch initialized` | **PASS** | boot-m0-04.txt | tap-driven record cycles=3, last tap count=5 |
| 音频 | `Claw4Audio: I2S slave 16kHz stereo32; mic+reference; bounded IO` | **PASS** | boot-m0-02.txt | AFE=yes, loopback user-confirmed=yes, WakeNet=AFE_CONFIG: Set WakeNet Model: wn9_nihaoxiaozhi_tts, WAKE_DETECTED events=0 |
| 唤醒词 | `V6M0: WAKE_DETECTED count` | **NOT_VERIFIED** | — | model is loaded, but a positive detection has never been captured on the wire; wake=1 in HEALTH is IsWakeWordRunning(), not a detection |
| C5 链路 | `transport: Identified slave [esp32c5]` | **PASS** | boot-m0-02.txt | SDIO card init=yes, coprocessor boot-up=yes |
| 配网模式（热点/配置页） | `WifiConfigurationAp: Access Point started with SSID Xiaozhi-79D9` | **PASS** | netprobe-m0-02.txt | AP SSID=WifiConfigurationAp: Access Point started with SSID Xiaozhi-79D9, DHCP=DHCP server started on interface WIFI_AP_DEF with IP: 192.168.4.1, softap events=2, connect timeout after=1 cycle(s); a started softap proves the radio transmits, it does not prove association |
| 网络 / IP | `Connected to WiFi: realme` | **PASS** | netprobe-m0-06.txt | SSID=realme, IP=10.76.189.105, gw=10.76.189.222, NETWORK_EVENT seen=[0, 1, 2, 3, 4] (0=Scanning 1=Connecting 2=Connected 3=Disconnected 4=ConfigModeEnter), found-AP=1, connecting=1, connected=1; scan cycles=3, 'No AP found' cycles=3, saved-channel scans=1 (saved channel=11), config-mode entries=1; 'No AP found' means no saved SSID matched, not that the scan returned nothing |
| 资源分区 | `V6M0: Assets applied=1` | **PASS** | boot-m0-03.txt | assets_apply() returned true |
| SD 卡 / Camera / 电源键 | `no signal in candidate` | **NOT_TESTED** | — | not integrated in m0.1 |
| 回滚（恢复写回） | `out/v6-device-private/pre-v6-flash.bin` | **NOT_TESTED** | — | a full 32MiB image was read back and hashed, but it has never been written back; until then rollback is unproven |

## 关键信号（跨捕获合并）

| 信号 | 值 | 来源 |
| --- | --- | --- |
| `afe_pipeline` | `AFE: AFE Pipeline: [input] -> \|AEC(FD_LOW_COST, NLP_VERY_AGGRESSIVE)\| -> \|VAD(WebRTC)\| -> \|WakeNet(wn9_nihaoxiaozhi_tts,)\| -> [output]` | boot-m0-02.txt |
| `assertions` | `2` | netprobe-m0-07.txt |
| `assets_applied` | `1` | boot-m0-03.txt |
| `assets_mapped` | `Assets: The assets map size is 576 KB (partition 15360 KB)` | boot-m0-02.txt |
| `audio_i2s` | `Claw4Audio: I2S slave 16kHz stereo32; mic+reference; bounded IO` | boot-m0-02.txt |
| `audio_module_probe` | `Claw4V6: Audio module local-mode response bytes=21 (clock test still required)` | boot-m0-02.txt |
| `boot_ready` | `V6M0: BOOT_READY IDF=v6.1;` | boot-m0-02.txt |
| `c5_bootup` | `RPC_WRAP: Coprocessor Boot-up` | boot-m0-02.txt |
| `c5_slave` | `transport: Identified slave [esp32c5]` | boot-m0-02.txt |
| `config_ap_dhcp` | `DHCP server started on interface WIFI_AP_DEF with IP: 192.168.4.1` | netprobe-m0-02.txt |
| `config_ap_ssid` | `WifiConfigurationAp: Access Point started with SSID Xiaozhi-79D9` | netprobe-m0-02.txt |
| `config_web_server` | `1` | netprobe-m0-02.txt |
| `connect_timeout` | `1` | netprobe-m0-02.txt |
| `display_init` | `Claw4V6: NV3051F display initialized, RGB565 -> RGB888` | boot-m0-02.txt |
| `display_rgb888` | `Claw4V6: Native RGB888 display, panel double buffers, full refresh` | boot-m0-04.txt |
| `found_ap` | `realme` | netprobe-m0-06.txt |
| `found_ap_count` | `1` | netprobe-m0-06.txt |
| `health_last` | `V6M0: HEALTH free=28027643 psram=27878204 wake=1 taps=0` | boot-m0-02.txt |
| `health_samples` | `6` | netprobe-m0-03.txt |
| `idf_version` | `v6.1` | boot-m0-02.txt |
| `lvgl_first_refresh` | `Claw4V6: LVGL first refresh completed (physical image still requires confirmation)` | boot-m0-04.txt |
| `mac_not_ready` | `1` | boot-m0-02.txt |
| `net_events` | `[0, 1, 2, 3, 4]` | netprobe-m0-06.txt |
| `no_ap_on_saved_ch` | `1` | netprobe-m0-03.txt |
| `panel_capability_errors` | `2` | boot-m0-04.txt |
| `playback_queued` | `3` | boot-m0-04.txt |
| `reset_reason` | `rst:0x17 (CHIP_USB_UART_RESET)` | boot-m0-02.txt |
| `saved_channel_scan` | `11` | netprobe-m0-03.txt |
| `sdio_card_init` | `H_SDIO_DRV: Card init success, TRANSPORT_RX_ACTIVE` | boot-m0-02.txt |
| `softap_started` | `2` | netprobe-m0-02.txt |
| `sta_connected_event` | `1` | netprobe-m0-06.txt |
| `sta_gateway` | `10.76.189.222` | netprobe-m0-06.txt |
| `sta_ip` | `10.76.189.105` | netprobe-m0-06.txt |
| `state_machine_reject` | `1` | netprobe-m0-02.txt |
| `touch_init` | `Claw4V6: GT911 touch initialized` | boot-m0-02.txt |
| `touch_last_count` | `5` | boot-m0-04.txt |
| `touch_record_cycles` | `3` | boot-m0-04.txt |
| `wake_detected` | `0` | boot-m0-02.txt |
| `wakenet_model` | `AFE_CONFIG: Set WakeNet Model: wn9_nihaoxiaozhi_tts` | boot-m0-02.txt |
| `wifi_attempt` | `1` | boot-m0-02.txt |
| `wifi_config_mode` | `1` | netprobe-m0-02.txt |
| `wifi_connected` | `Connected to WiFi: realme` | netprobe-m0-06.txt |
| `wifi_connecting` | `1` | netprobe-m0-06.txt |
| `wifi_disconnected` | `1` | netprobe-m0-02.txt |
| `wifi_no_ap` | `3` | netprobe-m0-01.txt |
| `wifi_scan_cycles` | `3` | boot-m0-03.txt |
| `wifi_scan_done` | `4` | netprobe-m0-02.txt |

## HEALTH 采样（全部捕获）

| 来源 | 时刻 (ms) | free | psram | WakeWordRunning | taps |
| --- | --- | --- | --- | --- | --- |
| `boot-m0-02.txt` | 18557 | 28027643 | 27878204 | 1 | 0 |
| `boot-m0-02.txt` | 28557 | 28027643 | 27878204 | 1 | 0 |
| `boot-m0-03.txt` | 18565 | 28027003 | 27878604 | 1 | 0 |
| `boot-m0-03.txt` | 28574 | 28027003 | 27878604 | 1 | 0 |
| `boot-m0-03.txt` | 38574 | 28027003 | 27878604 | 1 | 0 |
| `netprobe-m0-01.txt` | 18579 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-01.txt` | 28584 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-01.txt` | 38593 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-01.txt` | 48603 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-01.txt` | 58613 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-02.txt` | 68622 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-02.txt` | 78628 | 27118527 | 26837296 | 1 | 0 |
| `netprobe-m0-02.txt` | 88637 | 27118047 | 26837296 | 1 | 0 |
| `netprobe-m0-02.txt` | 98643 | 27118047 | 26837296 | 1 | 0 |
| `netprobe-m0-02.txt` | 108653 | 27118047 | 26837296 | 1 | 0 |
| `netprobe-m0-03.txt` | 18569 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-03.txt` | 28579 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-03.txt` | 38584 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-03.txt` | 48593 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-03.txt` | 58603 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-04.txt` | 18580 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-04.txt` | 28585 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-04.txt` | 38594 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-04.txt` | 48604 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-04.txt` | 58614 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-05.txt` | 68623 | 27134019 | 26841788 | 1 | 0 |
| `netprobe-m0-05.txt` | 78629 | 27118527 | 26837296 | 1 | 0 |
| `netprobe-m0-05.txt` | 88638 | 27118527 | 26837296 | 1 | 0 |
| `netprobe-m0-06.txt` | 18581 | 27133675 | 26841568 | 1 | 0 |
| `netprobe-m0-06.txt` | 28586 | 27133675 | 26841568 | 1 | 0 |
| `netprobe-m0-06.txt` | 38595 | 27133675 | 26841568 | 1 | 0 |
| `netprobe-m0-06.txt` | 48605 | 27133675 | 26841568 | 1 | 0 |
| `netprobe-m0-06.txt` | 58615 | 27133675 | 26841568 | 1 | 0 |
| `netprobe-m0-07.txt` | 18569 | 27133675 | 26841568 | 1 | 0 |
| `netprobe-m0-07.txt` | 28579 | 27133675 | 26841568 | 1 | 0 |
| `netprobe-m0-07.txt` | 38585 | 27133675 | 26841568 | 1 | 0 |

`WakeWordRunning` 是 `audio.IsWakeWordRunning()` 的运行态，**不是**检测到唤醒；只有 `V6M0: WAKE_DETECTED` 才算正事件。

## 未闭合项

- 唤醒词从未捕获到正事件（WAKE_DETECTED=0）
- 恢复写回未实测，回滚不可信
- SD 卡 / Camera / 电源键未集成
