# V6 M0 硬件验收矩阵（自动生成）

> Codex 2026-09-22复核：原始证据保留；参考通道“实际播放期间无效”、网络“确定环境原因”及间隙总量的旧结论 SUPERSEDED。以 `docs/v6/CODEX_V6_CANDIDATE05_REVIEW_AND_06.md` 为当前解释，参考链路 HARDWARE_VERIFY_REQUIRED，网络 NOT_VERIFIED，提供的 wall-clock 间隙合计35秒。

> 生成时间 2026-09-22T11:01:20+00:00；schema `claw4-v6-m0-hw-matrix/1`。
> 冻结候选 `m0-candidate-05.json`，应用 SHA256 `f109b9282fcec671…`。
> 证据来源 55 份启动捕获 / 1 份刷写捕获。
> 数值型信号按**跨捕获取最大值**合并，避免后一份没点击的日志抹掉前一份的触摸证据；
> 缺信号一律 NOT_VERIFIED，绝不由相邻项推断。

| 项目 | 证据 | 状态 | 来源 | 说明 |
| --- | --- | --- | --- | --- |
| Flash / PSRAM | `candidate manifest + flash log 'Hash of data verified.'` | **PASS** | — | verified writes in this capture: 1; long-run stress still not exercised |
| 启动 | `V6M0: BOOT_READY IDF=v6.1;` | **PASS** | cp0-boot-01.txt | boot attempts=1, BOOT_READY=1, aborts=0, panic resets=0; latest device timestamp 2172.2s (not capture duration) |
| 显示 | `Claw4V6: Native RGB888 display, panel double buffers, full refresh` | **NOT_VERIFIED** | cp0-boot-01.txt | display init=yes, user confirmed visible=no, unsupported-capability errors=2 |
| 触摸 | `Claw4V6: GT911 touch initialized` | **PASS** | cp2-loopback-01.txt | tap-driven record cycles=1, last tap count=2 |
| 音频 | `Claw4Audio: I2S slave 16kHz stereo32; mic+reference; bounded IO` | **PASS** | cp0-boot-01.txt | AFE=yes, loopback user-confirmed=yes, WakeNet=AFE_CONFIG: Set WakeNet Model: wn9_nihaoxiaozhi_tts, WAKE_DETECTED events=5 |
| 唤醒词 | `V6M0: WAKE_DETECTED count` | **PASS** | — | positive wake callbacks=5; armed state is not detection evidence |
| C5 链路 | `transport: Identified slave [esp32c5]` | **PASS** | cp0-boot-01.txt | SDIO card init=yes, coprocessor boot-up=yes |
| 配网模式（热点/配置页） | `WifiConfigurationAp: Access Point started with SSID Xiaozhi-79D9` | **PASS** | cp2-wake-02.txt | AP SSID=WifiConfigurationAp: Access Point started with SSID Xiaozhi-79D9, DHCP=DHCP server started on interface WIFI_AP_DEF with IP: 192.168.4.1, softap events=2, connect timeout after=1 cycle(s); a started softap proves the radio transmits, it does not prove association |
| 网络 / IP | `no 'Connected to WiFi' line` | **NOT_VERIFIED** | — | SSID=None, IP=None, gw=None, NETWORK_EVENT seen=[0, 3, 4] (0=Scanning 1=Connecting 2=Connected 3=Disconnected 4=ConfigModeEnter), found-AP=0, connecting=0, connected=0; scan cycles=3, 'No AP found' cycles=3, saved-channel scans=1 (saved channel=11), config-mode entries=1; 'No AP found' means no saved SSID matched, not that the scan returned nothing |
| 资源分区 | `V6M0: Assets applied=1` | **PASS** | cp0-boot-01.txt | assets_apply() returned true |
| SD 卡 / Camera / 电源键 | `no signal in candidate` | **NOT_TESTED** | — | not integrated in m0.1 |
| 回滚（恢复写回） | `out/v6-device-private/pre-v6-flash.bin` | **NOT_TESTED** | — | a full 32MiB image was read back and hashed, but it has never been written back; until then rollback is unproven |

## 关键信号（跨捕获合并）

| 信号 | 值 | 来源 |
| --- | --- | --- |
| `afe_pipeline` | `AFE: AFE Pipeline: [input] -> \|AEC(FD_LOW_COST, NLP_VERY_AGGRESSIVE)\| -> \|VAD(WebRTC)\| -> \|WakeNet(wn9_nihaoxiaozhi_tts,)\| -> [output]` | cp0-boot-01.txt |
| `assertions` | `0` | cp0-boot-01.txt |
| `assets_applied` | `1` | cp0-boot-01.txt |
| `assets_mapped` | `Assets: The assets map size is 576 KB (partition 15360 KB)` | cp0-boot-01.txt |
| `audio_i2s` | `Claw4Audio: I2S slave 16kHz stereo32; mic+reference; bounded IO` | cp0-boot-01.txt |
| `audio_module_probe` | `Claw4V6: Audio module local-mode response bytes=21 (clock test still required)` | cp0-boot-01.txt |
| `boot_attempts` | `1` | cp0-boot-01.txt |
| `boot_blocked` | `0` | cp0-boot-01.txt |
| `boot_ready` | `V6M0: BOOT_READY IDF=v6.1;` | cp0-boot-01.txt |
| `boot_ready_count` | `1` | cp0-boot-01.txt |
| `c5_bootup` | `RPC_WRAP: Coprocessor Boot-up` | cp0-boot-01.txt |
| `c5_slave` | `transport: Identified slave [esp32c5]` | cp0-boot-01.txt |
| `config_ap_dhcp` | `DHCP server started on interface WIFI_AP_DEF with IP: 192.168.4.1` | cp2-wake-02.txt |
| `config_ap_ssid` | `WifiConfigurationAp: Access Point started with SSID Xiaozhi-79D9` | cp2-wake-02.txt |
| `config_web_server` | `1` | cp2-wake-02.txt |
| `connect_timeout` | `1` | cp2-wake-02.txt |
| `display_init` | `Claw4V6: NV3051F display initialized, native RGB888` | cp0-boot-01.txt |
| `display_rgb888` | `Claw4V6: Native RGB888 display, panel double buffers, full refresh` | cp0-boot-01.txt |
| `found_ap` | `None` | — |
| `found_ap_count` | `0` | cp0-boot-01.txt |
| `health_last` | `V6M0: HEALTH free=27130899 psram=26838768 wake=1 taps=0 wakes=0 vads=0 vad_observable=0` | cp0-boot-01.txt |
| `health_samples` | `6` | cp2-loopback-01.txt |
| `idf_version` | `v6.1` | cp0-boot-01.txt |
| `lvgl_first_refresh` | `Claw4V6: LVGL first refresh completed (physical image still requires confirmation)` | cp0-boot-01.txt |
| `mac_not_ready` | `1` | cp0-boot-01.txt |
| `net_events` | `[0, 3, 4]` | cp2-wake-02.txt |
| `no_ap_on_saved_ch` | `1` | cp0-boot-01.txt |
| `panel_capability_errors` | `2` | cp0-boot-01.txt |
| `panic_resets` | `0` | cp0-boot-01.txt |
| `playback_queued` | `1` | cp2-loopback-01.txt |
| `psram_found` | `Found 32MB PSRAM device` | cp0-boot-01.txt |
| `reset_reason` | `rst:0x17 (CHIP_USB_UART_RESET)` | cp0-boot-01.txt |
| `saved_channel_scan` | `11` | cp0-boot-01.txt |
| `sdio_card_init` | `H_SDIO_DRV: Card init success, TRANSPORT_RX_ACTIVE` | cp0-boot-01.txt |
| `softap_started` | `2` | cp2-wake-02.txt |
| `sta_connected_event` | `0` | cp0-boot-01.txt |
| `sta_gateway` | `None` | — |
| `sta_ip` | `None` | — |
| `state_machine_reject` | `1` | cp2-wake-02.txt |
| `touch_init` | `Claw4V6: GT911 touch initialized` | cp0-boot-01.txt |
| `touch_last_count` | `2` | cp2-loopback-01.txt |
| `touch_record_cycles` | `1` | cp2-loopback-01.txt |
| `wake_detected` | `5` | cp2-wake-01.txt |
| `wakenet_model` | `AFE_CONFIG: Set WakeNet Model: wn9_nihaoxiaozhi_tts` | cp0-boot-01.txt |
| `wifi_attempt` | `1` | cp0-boot-01.txt |
| `wifi_config_mode` | `1` | cp2-wake-02.txt |
| `wifi_connected` | `None` | — |
| `wifi_connecting` | `0` | cp0-boot-01.txt |
| `wifi_disconnected` | `1` | cp2-wake-02.txt |
| `wifi_no_ap` | `3` | cp0-boot-01.txt |
| `wifi_scan_cycles` | `3` | cp0-boot-01.txt |
| `wifi_scan_done` | `5` | cp2-loopback-01.txt |

## HEALTH 采样（全部捕获）

| 来源 | 时刻 (ms) | free | psram | WakeWordRunning | taps |
| --- | --- | --- | --- | --- | --- |
| `cp0-boot-01.txt` | 19603 | 27130899 | 26838768 | 1 | 0 |
| `cp0-boot-01.txt` | 29619 | 27130899 | 26838768 | 1 | 0 |
| `cp0-boot-01.txt` | 39634 | 27130899 | 26838768 | 1 | 0 |
| `cp0-boot-01.txt` | 49675 | 27130899 | 26838768 | 1 | 0 |
| `cp0-boot-01.txt` | 59693 | 27130899 | 26838768 | 1 | 0 |
| `boot-01.txt` | 19609 | 27130859 | 26838768 | 1 | 0 |
| `boot-02.txt` | 19606 | 27130899 | 26838768 | 1 | 0 |
| `boot-03.txt` | 19609 | 27130879 | 26838768 | 1 | 0 |
| `boot-04.txt` | 19609 | 27130879 | 26838768 | 1 | 0 |
| `boot-05.txt` | 19609 | 27130899 | 26838768 | 1 | 0 |
| `boot-06.txt` | 19606 | 27130879 | 26838768 | 1 | 0 |
| `boot-07.txt` | 19555 | 27130879 | 26838768 | 1 | 0 |
| `boot-08.txt` | 19609 | 27130899 | 26838768 | 1 | 0 |
| `boot-09.txt` | 19606 | 27130879 | 26838768 | 1 | 0 |
| `boot-10.txt` | 19609 | 27130879 | 26838768 | 1 | 0 |
| `boot-11.txt` | 19555 | 27130887 | 26838768 | 1 | 0 |
| `boot-12.txt` | 19609 | 27130879 | 26838768 | 1 | 0 |
| `boot-13.txt` | 19609 | 27130899 | 26838768 | 1 | 0 |
| `boot-14.txt` | 19609 | 27130879 | 26838768 | 1 | 0 |
| `boot-15.txt` | 19609 | 27130879 | 26838768 | 1 | 0 |
| `boot-16.txt` | 19609 | 27130899 | 26838768 | 1 | 0 |
| `boot-17.txt` | 19609 | 27130879 | 26838768 | 1 | 0 |
| `boot-18.txt` | 19609 | 27130879 | 26838768 | 1 | 0 |
| `boot-19.txt` | 19609 | 27130879 | 26838768 | 1 | 0 |
| `boot-20.txt` | 19609 | 27130899 | 26838768 | 1 | 0 |
| `cp2-wake-01.txt` | 19609 | 27130879 | 26838768 | 1 | 0 |
| `cp2-wake-01.txt` | 29643 | 27130879 | 26838768 | 1 | 0 |
| `cp2-wake-01.txt` | 39708 | 27130879 | 26838768 | 1 | 0 |
| `cp2-wake-01.txt` | 49729 | 27130879 | 26838768 | 1 | 0 |
| `cp2-wake-01.txt` | 59738 | 27130879 | 26838768 | 1 | 0 |
| `cp2-wake-02.txt` | 69812 | 27115891 | 26834276 | 1 | 0 |
| `cp2-wake-02.txt` | 79912 | 27115503 | 26834276 | 1 | 0 |
| `cp2-wake-02.txt` | 89929 | 27115503 | 26834276 | 1 | 0 |
| `cp2-wake-02.txt` | 100014 | 27116147 | 26834276 | 0 | 0 |
| `cp2-loopback-01.txt` | 150132 | 27114843 | 26834276 | 1 | 1 |
| `cp2-loopback-01.txt` | 160238 | 27114843 | 26834276 | 1 | 2 |
| `cp2-loopback-01.txt` | 170243 | 27114843 | 26834276 | 1 | 2 |
| `cp2-loopback-01.txt` | 180264 | 27114843 | 26834276 | 1 | 2 |
| `cp2-loopback-01.txt` | 190303 | 27114843 | 26834276 | 1 | 2 |
| `cp2-loopback-01.txt` | 200303 | 27114843 | 26834276 | 1 | 2 |
| `cp2-loopback-02.txt` | 210363 | 27114843 | 26834276 | 1 | 2 |
| `cp2-loopback-02.txt` | 220363 | 27114843 | 26834276 | 1 | 2 |
| `cp2-loopback-02.txt` | 230392 | 27114843 | 26834276 | 1 | 2 |
| `soak-01.txt` | 300571 | 27114843 | 26834276 | 1 | 2 |
| `soak-01.txt` | 310603 | 27114843 | 26834276 | 1 | 2 |
| `soak-01.txt` | 320702 | 27114843 | 26834276 | 1 | 2 |
| `soak-01.txt` | 330711 | 27114843 | 26834276 | 1 | 2 |
| `soak-01.txt` | 340743 | 27114843 | 26834276 | 1 | 2 |
| `soak-01.txt` | 350743 | 27114843 | 26834276 | 1 | 2 |
| `soak-02.txt` | 360771 | 27114843 | 26834276 | 1 | 2 |
| `soak-02.txt` | 370802 | 27114843 | 26834276 | 1 | 2 |
| `soak-02.txt` | 380825 | 27114843 | 26834276 | 1 | 2 |
| `soak-02.txt` | 390825 | 27114843 | 26834276 | 1 | 2 |
| `soak-02.txt` | 400861 | 27114843 | 26834276 | 1 | 2 |
| `soak-02.txt` | 410902 | 27114843 | 26834276 | 1 | 2 |
| `soak-03.txt` | 430925 | 27114843 | 26834276 | 1 | 2 |
| `soak-03.txt` | 440962 | 27114843 | 26834276 | 1 | 2 |
| `soak-03.txt` | 450962 | 27114843 | 26834276 | 1 | 2 |
| `soak-03.txt` | 460962 | 27114843 | 26834276 | 1 | 2 |
| `soak-03.txt` | 471001 | 27114843 | 26834276 | 1 | 2 |
| `soak-03.txt` | 481001 | 27114843 | 26834276 | 1 | 2 |
| `soak-04.txt` | 491067 | 27114843 | 26834276 | 1 | 2 |
| `soak-04.txt` | 501081 | 27114843 | 26834276 | 1 | 2 |
| `soak-04.txt` | 511116 | 27114843 | 26834276 | 1 | 2 |
| `soak-04.txt` | 521141 | 27114843 | 26834276 | 1 | 2 |
| `soak-04.txt` | 531170 | 27114843 | 26834276 | 1 | 2 |
| `soak-04.txt` | 541266 | 27114843 | 26834276 | 1 | 2 |
| `soak-05.txt` | 551281 | 27114843 | 26834276 | 1 | 2 |
| `soak-05.txt` | 561281 | 27114843 | 26834276 | 1 | 2 |
| `soak-05.txt` | 571321 | 27114843 | 26834276 | 1 | 2 |
| `soak-05.txt` | 581321 | 27114843 | 26834276 | 1 | 2 |
| `soak-05.txt` | 591321 | 27114843 | 26834276 | 1 | 2 |
| `soak-05.txt` | 601355 | 27114843 | 26834276 | 1 | 2 |
| `soak-06.txt` | 611361 | 27114843 | 26834276 | 1 | 2 |
| `soak-06.txt` | 621460 | 27114843 | 26834276 | 1 | 2 |
| `soak-06.txt` | 631489 | 27114843 | 26834276 | 1 | 2 |
| `soak-06.txt` | 641500 | 27114843 | 26834276 | 1 | 2 |
| `soak-06.txt` | 651500 | 27114843 | 26834276 | 1 | 2 |
| `soak-06.txt` | 661529 | 27114843 | 26834276 | 1 | 2 |
| `soak-07.txt` | 681640 | 27114843 | 26834276 | 1 | 2 |
| `soak-07.txt` | 691739 | 27114843 | 26834276 | 1 | 2 |
| `soak-07.txt` | 701748 | 27114843 | 26834276 | 1 | 2 |
| `soak-07.txt` | 711780 | 27114843 | 26834276 | 1 | 2 |
| `soak-07.txt` | 721880 | 27114843 | 26834276 | 1 | 2 |
| `soak-07.txt` | 731915 | 27114843 | 26834276 | 1 | 2 |
| `soak-08.txt` | 741920 | 27114843 | 26834276 | 1 | 2 |
| `soak-08.txt` | 752019 | 27114843 | 26834276 | 1 | 2 |
| `soak-08.txt` | 762048 | 27114843 | 26834276 | 1 | 2 |
| `soak-08.txt` | 772060 | 27114843 | 26834276 | 1 | 2 |
| `soak-08.txt` | 782160 | 27114843 | 26834276 | 1 | 2 |
| `soak-08.txt` | 792188 | 27114843 | 26834276 | 1 | 2 |
| `soak-09.txt` | 802239 | 27114843 | 26834276 | 1 | 2 |
| `soak-09.txt` | 812253 | 27114843 | 26834276 | 1 | 2 |
| `soak-09.txt` | 822279 | 27114843 | 26834276 | 1 | 2 |
| `soak-09.txt` | 832308 | 27114843 | 26834276 | 1 | 2 |
| `soak-09.txt` | 842402 | 27114843 | 26834276 | 1 | 2 |
| `soak-09.txt` | 852419 | 27114843 | 26834276 | 1 | 2 |
| `soak-10.txt` | 862474 | 27114843 | 26834276 | 1 | 2 |
| `soak-10.txt` | 872479 | 27114843 | 26834276 | 1 | 2 |
| `soak-10.txt` | 882579 | 27114843 | 26834276 | 1 | 2 |
| `soak-10.txt` | 892607 | 27114843 | 26834276 | 1 | 2 |
| `soak-10.txt` | 902618 | 27114843 | 26834276 | 1 | 2 |
| `soak-10.txt` | 912619 | 27114843 | 26834276 | 1 | 2 |
| `soak-11.txt` | 932698 | 27114843 | 26834276 | 1 | 2 |
| `soak-11.txt` | 942738 | 27114843 | 26834276 | 1 | 2 |
| `soak-11.txt` | 952838 | 27114843 | 26834276 | 1 | 2 |
| `soak-11.txt` | 962867 | 27114843 | 26834276 | 1 | 2 |
| `soak-11.txt` | 972898 | 27114843 | 26834276 | 1 | 2 |
| `soak-11.txt` | 982921 | 27114843 | 26834276 | 1 | 2 |
| `soak-12.txt` | 992958 | 27114843 | 26834276 | 1 | 2 |
| `soak-12.txt` | 1002958 | 27114843 | 26834276 | 1 | 2 |
| `soak-12.txt` | 1012986 | 27114843 | 26834276 | 1 | 2 |
| `soak-12.txt` | 1023003 | 27114843 | 26834276 | 1 | 2 |
| `soak-12.txt` | 1033018 | 27114843 | 26834276 | 1 | 2 |
| `soak-12.txt` | 1043058 | 27114843 | 26834276 | 1 | 2 |
| `soak-13.txt` | 1053077 | 27114843 | 26834276 | 1 | 2 |
| `soak-13.txt` | 1063081 | 27114843 | 26834276 | 1 | 2 |
| `soak-13.txt` | 1073117 | 27114843 | 26834276 | 1 | 2 |
| `soak-13.txt` | 1083137 | 27114843 | 26834276 | 1 | 2 |
| `soak-13.txt` | 1093147 | 27114843 | 26834276 | 1 | 2 |
| `soak-13.txt` | 1103157 | 27114843 | 26834276 | 1 | 2 |
| `soak-14.txt` | 1113237 | 27114843 | 26834276 | 1 | 2 |
| `soak-14.txt` | 1123257 | 27114843 | 26834276 | 1 | 2 |
| `soak-14.txt` | 1133280 | 27114843 | 26834276 | 1 | 2 |
| `soak-14.txt` | 1143317 | 27114843 | 26834276 | 1 | 2 |
| `soak-14.txt` | 1153320 | 27114843 | 26834276 | 1 | 2 |
| `soak-14.txt` | 1163325 | 27114843 | 26834276 | 1 | 2 |
| `soak-15.txt` | 1183337 | 27114843 | 26834276 | 1 | 2 |
| `soak-15.txt` | 1193366 | 27114843 | 26834276 | 1 | 2 |
| `soak-15.txt` | 1203366 | 27114843 | 26834276 | 1 | 2 |
| `soak-15.txt` | 1213377 | 27114843 | 26834276 | 1 | 2 |
| `soak-15.txt` | 1223386 | 27114843 | 26834276 | 1 | 2 |
| `soak-15.txt` | 1233406 | 27114843 | 26834276 | 1 | 2 |
| `soak-16.txt` | 1243411 | 27114843 | 26834276 | 1 | 2 |
| `soak-16.txt` | 1253416 | 27114843 | 26834276 | 1 | 2 |
| `soak-16.txt` | 1263445 | 27114843 | 26834276 | 1 | 2 |
| `soak-16.txt` | 1273545 | 27114843 | 26834276 | 1 | 2 |
| `soak-16.txt` | 1283556 | 27114843 | 26834276 | 1 | 2 |
| `soak-16.txt` | 1293585 | 27114843 | 26834276 | 1 | 2 |
| `soak-17.txt` | 1303585 | 27114843 | 26834276 | 1 | 2 |
| `soak-17.txt` | 1313596 | 27114843 | 26834276 | 1 | 2 |
| `soak-17.txt` | 1323625 | 27114843 | 26834276 | 1 | 2 |
| `soak-17.txt` | 1333725 | 27114843 | 26834276 | 1 | 2 |
| `soak-17.txt` | 1343731 | 27114843 | 26834276 | 1 | 2 |
| `soak-17.txt` | 1353765 | 27114843 | 26834276 | 1 | 2 |
| `soak-18.txt` | 1363765 | 27114843 | 26834276 | 1 | 2 |
| `soak-18.txt` | 1373765 | 27114843 | 26834276 | 1 | 2 |
| `soak-18.txt` | 1383775 | 27114843 | 26834276 | 1 | 2 |
| `soak-18.txt` | 1393805 | 27114843 | 26834276 | 1 | 2 |
| `soak-18.txt` | 1403904 | 27114843 | 26834276 | 1 | 2 |
| `soak-18.txt` | 1413915 | 27114843 | 26834276 | 1 | 2 |
| `soak-19.txt` | 1434044 | 27114843 | 26834276 | 1 | 2 |
| `soak-19.txt` | 1444050 | 27114843 | 26834276 | 1 | 2 |
| `soak-19.txt` | 1454084 | 27114843 | 26834276 | 1 | 2 |
| `soak-19.txt` | 1464084 | 27114843 | 26834276 | 1 | 2 |
| `soak-19.txt` | 1474084 | 27114843 | 26834276 | 1 | 2 |
| `soak-19.txt` | 1484124 | 27114843 | 26834276 | 1 | 2 |
| `soak-20.txt` | 1494174 | 27114843 | 26834276 | 1 | 2 |
| `soak-20.txt` | 1504184 | 27114843 | 26834276 | 1 | 2 |
| `soak-20.txt` | 1514204 | 27114843 | 26834276 | 1 | 2 |
| `soak-20.txt` | 1524238 | 27114843 | 26834276 | 1 | 2 |
| `soak-20.txt` | 1534264 | 27114843 | 26834276 | 1 | 2 |
| `soak-20.txt` | 1544369 | 27114843 | 26834276 | 1 | 2 |
| `soak-21.txt` | 1554423 | 27114843 | 26834276 | 1 | 2 |
| `soak-21.txt` | 1564454 | 27114843 | 26834276 | 1 | 2 |
| `soak-21.txt` | 1574483 | 27114843 | 26834276 | 1 | 2 |
| `soak-21.txt` | 1584494 | 27114843 | 26834276 | 1 | 2 |
| `soak-21.txt` | 1594523 | 27114843 | 26834276 | 1 | 2 |
| `soak-21.txt` | 1604543 | 27114843 | 26834276 | 1 | 2 |
| `soak-22.txt` | 1614623 | 27114843 | 26834276 | 1 | 2 |
| `soak-22.txt` | 1624663 | 27114843 | 26834276 | 1 | 2 |
| `soak-22.txt` | 1634683 | 27114843 | 26834276 | 1 | 2 |
| `soak-22.txt` | 1644718 | 27114843 | 26834276 | 1 | 2 |
| `soak-22.txt` | 1654723 | 27114843 | 26834276 | 1 | 2 |
| `soak-22.txt` | 1664723 | 27114843 | 26834276 | 1 | 2 |
| `soak-23.txt` | 1684833 | 27114843 | 26834276 | 1 | 2 |
| `soak-23.txt` | 1694863 | 27114843 | 26834276 | 1 | 2 |
| `soak-23.txt` | 1704962 | 27114843 | 26834276 | 1 | 2 |
| `soak-23.txt` | 1715002 | 27114843 | 26834276 | 1 | 2 |
| `soak-23.txt` | 1725102 | 27114843 | 26834276 | 1 | 2 |
| `soak-23.txt` | 1735202 | 27114843 | 26834276 | 1 | 2 |
| `soak-24.txt` | 1745242 | 27114843 | 26834276 | 1 | 2 |
| `soak-24.txt` | 1755342 | 27114843 | 26834276 | 1 | 2 |
| `soak-24.txt` | 1765442 | 27114843 | 26834276 | 1 | 2 |
| `soak-24.txt` | 1775482 | 27114843 | 26834276 | 1 | 2 |
| `soak-24.txt` | 1785482 | 27114843 | 26834276 | 1 | 2 |
| `soak-24.txt` | 1795482 | 27114843 | 26834276 | 1 | 2 |
| `soak-25.txt` | 1805517 | 27114843 | 26834276 | 1 | 2 |
| `soak-25.txt` | 1815522 | 27114843 | 26834276 | 1 | 2 |
| `soak-25.txt` | 1825522 | 27114843 | 26834276 | 1 | 2 |
| `soak-25.txt` | 1835552 | 27114843 | 26834276 | 1 | 2 |
| `soak-25.txt` | 1845561 | 27114843 | 26834276 | 1 | 2 |
| `soak-25.txt` | 1855561 | 27114843 | 26834276 | 1 | 2 |
| `soak-26.txt` | 1865561 | 27114843 | 26834276 | 1 | 2 |
| `soak-26.txt` | 1875596 | 27114843 | 26834276 | 1 | 2 |
| `soak-26.txt` | 1885601 | 27114843 | 26834276 | 1 | 2 |
| `soak-26.txt` | 1895701 | 27114843 | 26834276 | 1 | 2 |
| `soak-26.txt` | 1905732 | 27114843 | 26834276 | 1 | 2 |
| `soak-26.txt` | 1915741 | 27114843 | 26834276 | 1 | 2 |
| `soak-27.txt` | 1925741 | 27114843 | 26834276 | 1 | 2 |
| `soak-27.txt` | 1935772 | 27114843 | 26834276 | 1 | 2 |
| `soak-27.txt` | 1945781 | 27114843 | 26834276 | 1 | 2 |
| `soak-27.txt` | 1955881 | 27114843 | 26834276 | 1 | 2 |
| `soak-27.txt` | 1965911 | 27114843 | 26834276 | 1 | 2 |
| `soak-27.txt` | 1975916 | 27114843 | 26834276 | 1 | 2 |
| `soak-28.txt` | 1995971 | 27114843 | 26834276 | 1 | 2 |
| `soak-28.txt` | 2005990 | 27114843 | 26834276 | 1 | 2 |
| `soak-28.txt` | 2016000 | 27114843 | 26834276 | 1 | 2 |
| `soak-28.txt` | 2026040 | 27114843 | 26834276 | 1 | 2 |
| `soak-28.txt` | 2036054 | 27114843 | 26834276 | 1 | 2 |
| `soak-28.txt` | 2046071 | 27114843 | 26834276 | 1 | 2 |
| `soak-29.txt` | 2056075 | 27114843 | 26834276 | 1 | 2 |
| `soak-29.txt` | 2066081 | 27114843 | 26834276 | 1 | 2 |
| `soak-29.txt` | 2076100 | 27114843 | 26834276 | 1 | 2 |
| `soak-29.txt` | 2086140 | 27114843 | 26834276 | 1 | 2 |
| `soak-29.txt` | 2096160 | 27114843 | 26834276 | 1 | 2 |
| `soak-29.txt` | 2106170 | 27114843 | 26834276 | 1 | 2 |
| `soak-30.txt` | 2116225 | 27114843 | 26834276 | 1 | 2 |
| `soak-30.txt` | 2126260 | 27114843 | 26834276 | 1 | 2 |
| `soak-30.txt` | 2136280 | 27114843 | 26834276 | 1 | 2 |
| `soak-30.txt` | 2146314 | 27114843 | 26834276 | 1 | 2 |
| `soak-30.txt` | 2156334 | 27114843 | 26834276 | 1 | 2 |
| `soak-30.txt` | 2166339 | 27114843 | 26834276 | 1 | 2 |

`WakeWordRunning` 是 `audio.IsWakeWordRunning()`。**注意方向**：`EnableWakeWordDetection(true)` 置位、**检测到唤醒时清位**，该位只表示检测是否启用；0 或 1 都不能单独证明命中，采样也可能漏掉中间变化。真正的正事件只有 `V6M0: WAKE_DETECTED`。

## 未闭合项

- 恢复写回未实测，回滚不可信
- SD 卡 / Camera / 电源键未集成

---

## 补充：WorkBuddy 手工结论（本表由工具生成，此节为工具未覆盖的项）

本节内容不在 `hw_matrix.py` 的输出口径内，写在这里避免被误读为工具判定。原始证据见
`integration/v6/m0-candidate05-test-*.json` 与私密目录内的日志。

### 1. 工具的两处口径需注意（不是缺陷，是本轮输入形态导致的）

| 现象 | 说明 |
| --- | --- |
| `wake_detected` 显示 **5**，实际 **6** | 数值信号按"跨捕获取最大值"合并。本轮 CP2 是**同一个未复位会话**的分段捕获：`cp2-wake-01.txt` 有 5 次、`cp2-wake-02.txt` 有 1 次，取最大值得到 5，**少算 1 次**。累积计数器在连续会话里不能用 max 合并。 |
| `health_samples` 显示 6 | 同上，取的是单份捕获的最大值（6）。本轮全部 CP3 捕获合计 **180** 个 HEALTH 采样。 |

### 2. 工具没有行、但本轮有明确结论的项

| 项目 | 状态 | 证据 |
| --- | --- | --- |
| 音频输入电平（麦克风余量） | **FAIL** | ch0 在说话时 `peak=32768`（int16 满量程）、`clipped` 最高 **1098/秒**、`raw_peak=2147418112`（距 2^31 仅 0.003%）。`2^31>>12` 是 int16 上限的 16 倍 → `>>12` 必然削顶，`>>16` 正好满量程。空闲时 `rms` 266–316 且不削顶。 |
| 音频参考通道（AEC 参考链路） | **FAIL** | `INPUT ch=1` 在回环 87 个窗口 + 唤醒 96 个窗口里一律 `rms=0 peak=0 raw_peak=1`，**播放期间 0 次变化**。而 `claw4_audio.cc:14-15` 声明 `input_reference_=true; input_channels_=2`，同构建 AFE 流水线的 `\|AEC(...)\|` 级开着。 |
| 回环可闻性（主观） | **PASS** | 用户确认两次点击录音按钮、听到自己的回放，清楚、音量正常。**不构成 AEC 正常的证据**（回环走 testing 队列，绕过 AFE）。 |
| 重复唤醒 | **PASS** | 6 次命中、6 次 `WAKE_REARM armed=1`；窗口内 HEALTH 曾出现 `wake=0`（命中 99578 ms / 重臂 100734 ms / 采样 100014 ms），锁存位方向实测确认。 |
| 长稳 31.3 分钟（不复位） | **PASS** | 30/30 段；`abort/panic=0`、`BOOT_BLOCKED=0`、`task_wdt=0`、`read_failures=0`；`free` 与 `psram` **各有唯一取值**（零漂移）跨 180 个采样；HEALTH 每 10 秒一次、30 段全覆盖；静默期 `wakes` 恒为 6（无虚假唤醒）。 |
| 配网回退路径 | **PASS** | 60 秒连接超时后按设计进入配网：`NETWORK_EVENT=3` → 软 AP `Xiaozhi-79D9` → DHCP `192.168.4.1` → Web server → `NETWORK_EVENT=4`。 |
| 网络 / IP | **NOT_VERIFIED（环境，非候选回归）** | 本会话 55 份捕获**没有任何** `Connected to WiFi` / `sta ip:`。每次启动都是 `Scanning saved channel 11` → `No AP on saved channels` → 全信道扫描 → `No AP found`（10/20/40 s 递增）→ 68.68 s 超时进配网。NVS 里的凭据指向信道 11 的 AP，该 AP 当前不在空中。**不是固件缺陷**。 |
| 显示 | **NOT_VERIFIED（间接证据：可用）** | 按任务包要求未传 `--assume-display-visible`。但用户在屏上**成功点击了两次** `Record 3s / Playback` 按钮（`TOUCH count=2` 且按钮回调是 `LV_EVENT_CLICKED`），这要求按钮已渲染、可命中。记为间接证据，不升级为本行 PASS。 |
| 配网期间的周期性扫描 | **OPEN（非失败）** | 31.3 分钟里 138 次 `Scan start Req` / `StaScanDone`，周期精确 **12,879 ms**。已定位为 `WifiConfigurationAp` 的扫描定时器：`wifi_configuration_ap.cc:861` `esp_timer_start_once(scan_timer_, 10*1000000)`，扫描耗时约 2.879 s → 10 s + 2.879 s = 12.879 s。**配网页的预期行为**。 |

### 3. 本表未涵盖

- 历史候选 02 / 03 / 04 的捕获**未纳入**本表，仅含候选 05。
- 网络重连未验证：**没有施加断线刺激**，不构成重连已通过的结论。

