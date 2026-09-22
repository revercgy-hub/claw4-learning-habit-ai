# V6 M0 硬件验收矩阵（自动生成）

> 生成时间 2026-09-22T01:46:07+00:00；schema `claw4-v6-m0-hw-matrix/1`。
> 主证据 `boot-m0-04.txt` SHA256 `8aa8ccec897362a5…`。
> 本表只记录日志与用户确认能支撑的事实；缺信号一律 NOT_VERIFIED，不由相邻项推断。

| 项目 | 证据 | 状态 | 说明 |
| --- | --- | --- | --- |
| Flash / PSRAM | `candidate manifest + flash log 'Hash of data verified.'` | **PASS** | verified writes in this capture: 1; long-run stress still not exercised |
| 启动 | `V6M0: BOOT_READY IDF=v6.1;` | **PASS** | assertions=0; observed 29.6s |
| 显示 | `Claw4V6: Native RGB888 display, panel double buffers, full refresh` | **PASS** | display init=yes, user confirmed visible=yes, unsupported-capability errors=2 |
| 触摸 | `Claw4V6: GT911 touch initialized` | **PASS** | tap-driven record cycles=3, last tap count=5 |
| 音频 | `Claw4Audio: I2S slave 16kHz stereo32; mic+reference; bounded IO` | **PASS** | AFE=yes, loopback user-confirmed=yes, WakeNet=AFE_CONFIG: Set WakeNet Model: wn9_nihaoxiaozhi_tts, WAKE_DETECTED events=0 |
| 唤醒词 | `V6M0: WAKE_DETECTED count` | **NOT_VERIFIED** | model is loaded, but a positive detection has never been captured on the wire; wake=1 in HEALTH is IsWakeWordRunning(), not a detection |
| C5 链路 | `transport: Identified slave [esp32c5]` | **PASS** | SDIO card init=yes, coprocessor boot-up=yes |
| 网络 / IP | `no 'Connected to WiFi' line` | **NOT_VERIFIED** | scan cycles=2, 'No AP found' cycles=2, config-mode entries=0, NETWORK_EVENT=0 |
| 资源分区 | `V6M0: Assets applied=1` | **PASS** | assets_apply() returned true |
| SD 卡 / Camera / 电源键 | `no signal in candidate` | **NOT_TESTED** | not integrated in m0.1 |
| 回滚（恢复写回） | `out/v6-device-private/pre-v6-flash.bin` | **NOT_TESTED** | a full 32MiB image was read back and hashed, but it has never been written back; until then rollback is unproven |

## 关键信号（主捕获）

| 信号 | 值 |
| --- | --- |
| `afe_pipeline` | `AFE: AFE Pipeline: [input] -> \|AEC(FD_LOW_COST, NLP_VERY_AGGRESSIVE)\| -> \|VAD(WebRTC)\| -> \|WakeNet(wn9_nihaoxiaozhi_tts,)\| -> [output]` |
| `assertions` | `0` |
| `assets_applied` | `1` |
| `assets_mapped` | `Assets: The assets map size is 576 KB (partition 15360 KB)` |
| `audio_i2s` | `Claw4Audio: I2S slave 16kHz stereo32; mic+reference; bounded IO` |
| `audio_module_probe` | `Claw4V6: Audio module local-mode response bytes=21 (clock test still required)` |
| `boot_ready` | `V6M0: BOOT_READY IDF=v6.1;` |
| `c5_bootup` | `RPC_WRAP: Coprocessor Boot-up` |
| `c5_slave` | `transport: Identified slave [esp32c5]` |
| `display_init` | `Claw4V6: NV3051F display initialized, native RGB888` |
| `display_rgb888` | `Claw4V6: Native RGB888 display, panel double buffers, full refresh` |
| `health_last` | `None` |
| `health_samples` | `0` |
| `idf_version` | `v6.1` |
| `lvgl_first_refresh` | `Claw4V6: LVGL first refresh completed (physical image still requires confirmation)` |
| `mac_not_ready` | `1` |
| `net_event` | `0` |
| `panel_capability_errors` | `2` |
| `playback_queued` | `3` |
| `reset_reason` | `rst:0x17 (CHIP_USB_UART_RESET)` |
| `sdio_card_init` | `H_SDIO_DRV: Card init success, TRANSPORT_RX_ACTIVE` |
| `touch_init` | `Claw4V6: GT911 touch initialized` |
| `touch_last_count` | `5` |
| `touch_record_cycles` | `3` |
| `wake_detected` | `0` |
| `wakenet_model` | `AFE_CONFIG: Set WakeNet Model: wn9_nihaoxiaozhi_tts` |
| `wifi_attempt` | `1` |
| `wifi_config_mode` | `0` |
| `wifi_connected` | `None` |
| `wifi_no_ap` | `2` |
| `wifi_scan_cycles` | `2` |
| `wifi_scan_done` | `2` |

## HEALTH 采样（全部捕获）

| 来源 | 时刻 (ms) | free | psram | WakeWordRunning | taps |
| --- | --- | --- | --- | --- | --- |
| `boot-m0-02.txt` | 18557 | 28027643 | 27878204 | 1 | 0 |
| `boot-m0-02.txt` | 28557 | 28027643 | 27878204 | 1 | 0 |
| `boot-m0-03.txt` | 18565 | 28027003 | 27878604 | 1 | 0 |
| `boot-m0-03.txt` | 28574 | 28027003 | 27878604 | 1 | 0 |
| `boot-m0-03.txt` | 38574 | 28027003 | 27878604 | 1 | 0 |

`WakeWordRunning` 是 `audio.IsWakeWordRunning()` 的运行态，**不是**检测到唤醒；只有 `V6M0: WAKE_DETECTED` 才算正事件。

## 未闭合项

- 唤醒词从未捕获到正事件（WAKE_DETECTED=0）
- 从未取得 IP；C5 射频能力未被独立验证
- 恢复写回未实测，回滚不可信
- SD 卡 / Camera / 电源键未集成
