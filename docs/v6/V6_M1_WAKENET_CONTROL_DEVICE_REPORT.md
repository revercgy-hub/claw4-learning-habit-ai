# S15 WakeNet Control Candidate 设备证据报告

更新时间：2026-10-18。此报告记录 S15 Candidate 的设备写入、只读镜像身份核验和三段设备捕获（两段始于 2026-09-27，一段始于 2026-10-18）；它们不是完整 Voice Preflight。

## Candidate 与构建复核

- Candidate：`claw4-learning-v6-m1-wnctl-s15-20260927-01`。
- 冻结源码：`7f7357da2e1299b150a17b979fb1a307d027441e`，分支 `codex/v6-m1-wakenet-control-candidate-s15`；候选清单记录源码树干净。
- R-09：此前 Codex 只读复核结论 `PASS`，且为构建复核；没有单独的 R-09 review 文件。本报告不扩展该复核范围。
- 构建产物：app `3,137,392` bytes，SHA-256 `64deab2b695fd0858586e93b0cc23ef8e566df3a38d461826d77ab22d05c738a`；ELF SHA-256 `33bcb2d01031a24814489aef84eaf155b30a37546f68534583b3aea90f7d5769`。候选证据清单 `m1-wakenet-ctl-s15-candidate.json` SHA-256 `0aa6738af1f1adf477b95cb65e92efd49c2d63dedf7547893f9ced022e175a6b`；独立 stage manifest `v6-stage-manifest.json` SHA-256 `d95ab8496f5db5004d77c24aee0f40cc06ef9afc9b7167d9d8c4b949789dc717`。
- 构建清单记录 Host 测试 148 passed / 0 failed，app 构建成功；候选清单记录未从早期 Candidate 继承设备结果。

## 2026-09-27 写入及分区安全证据

设备为 ESP32-P4 rev1.3，MAC `80:f1:b2:d2:ed:14`。写入前读取的 live 分区表为 4096 bytes，SHA-256 `8e5a526458f90a2b32a28ca0162611ca680e7ac8ca49f8336999ad85b529521a`；其前 3072 bytes 与候选构建分区表一致（SHA-256 `ef0039b6366c57de098972c0f6e9fd991013b41968da9b866c4704cb68ef7e5f`）。live otadata 为 8192 bytes，SHA-256 `8ba3b110139f45443d4f268d1a3373ef99a1718b71d51664531b83ee2d4b91a3`。写入前 9 MiB `ota_0` 备份 SHA-256 为 `79322d42ec103602f531d2cac19cd39c4fc1f0f121707dd820e4667a2ac620ad`。

仅向 `0x200000` 写入 app 镜像（3,137,392 bytes）；esptool 写入校验成功。独立 app readback（SHA-256 `64deab2b695fd0858586e93b0cc23ef8e566df3a38d461826d77ab22d05c738a`）与候选 app SHA 一致；写入前后 live 分区表和 otadata SHA 均一致。该时点尚未核对模型分区内容。没有写入 bootloader、分区表、ota_1、模型分区或安全熔丝，也没有执行恢复写回。

## 2026-09-27 诊断观察

`boot-diag-01` 在一次显式 USB reset 后采集 72.125 秒；这是 USB reset 启动，不是冷启动。索引解析摘要：59 个输入窗口、949,120 mic samples、RMS max 836、peak max 6,869、read failures 0；59 个 AFE 状态窗口，Wake enabled/active 最后均为 1，Wake detection 0，feed/fetch 1,870，fetch failures 0，crash markers 0。没有观察到 Wake marker。该段没有对应的用户语音刺激确认，不能据此判定 WakeNet 失败或通过。

随后 `voice-rearm-02` 未请求复位，计划采集 240 秒，但在 88.328 秒遇到 `SerialException` 中断。摘要为 88 个输入窗口、1,415,040 samples、RMS max 2,895、peak max 24,700、read failures 0；87 个控制窗口，最后 `on_total=1/on_rc=1/off_total=0/off_rc=0`，Wake enabled/active 为 1，feed/fetch 2,760，detected/crash 均 0。`off_total=0` 表示没有记录到 disable 调用，因此 `off_rc=0` 是初始值，不代表一次 OFF 调用返回成功。用户确认采集期间做过一次 Wake 尝试，报告未观察到即时设备响应；问题未提问（`NOT_RUN`），没有保留或推断具体语句/帧。由于采集提前中断，不能声称完成 240 秒、Preflight round、稳定性测试或任何 ASR/LLM/TTS/播放链路。

候选绑定的 `espressif__esp-sr/include/esp32p4/esp_afe_sr_iface.h` 将 WakeNet 控制返回值定义为 `-1` 失败、`0` 禁用、`1` 启用。`*_rc` 只有在对应累计调用数大于 0 时，才表示实际调用的最新返回码；因此各摘要里的 `off_total=0/off_rc=0` 是初始值，不代表执行过 disable 调用，也不能据此宣称 OFF 成功。该接口值不按 `ESP_OK` 语义替换。

## 2026-10-18 身份恢复与被动启动捕获

COM7 当前镜像身份在任何新写入前以只读 `esptool verify-flash` 对 `0x200000`、3,137,392 bytes 核验，返回成功且摘要匹配上述 S15 app SHA。验证操作触发显式 USB reset；没有写 Flash。紧接着的被动启动捕获再次触发一次显式 USB reset（不是冷启动），持续 60.015 秒：解析 48 个输入窗口、772,320 mic samples、RMS max 692、peak max 6,462、read failures 0；47 个 AFE 状态/控制窗口，最后 Wake enabled/active 为 1，`on_total=1/on_rc=1/off_total=0/off_rc=0`，feed/fetch 1,489、fetch failures 0、Wake detected 0、crash markers 0。这里 `off_rc=0` 是因为没有记录到 OFF 调用，不能作为 OFF 成功证据。观察到精确 S15 ELF prefix 1 次、NAS OTA 7443 marker 1 次、WS 7444 marker 0 次、外部端点 marker 0 次。

后续只读模型分区核对确认，live model 分区标签为 `0x111000`、分区大小 `0xEF000`（978,944 bytes），完整读回 SHA-256 `69949f497970885c03782948bafec0ab8bad1e9d53f37e20ff1ee6f8df715565`。分区 catalog 只有一个模型 `wn9l_hai1tai4ling2`，metadata 为 `wakenet9l_tts3h12_Hai1Tai4Ling2_3_0.625_0.629`；live `wn9_data` 为 291,201 bytes，SHA-256 为 `bf7e1fd2321cd73382f6e1b947f5a4cc727d493b5c2e300ae193347037414c2e`。S15 构建则仅包含模型 `wn9_nihaoxiaozhi_tts`，其完整 291,042-byte `srmodels.bin` SHA-256 为 `7c87dd7adb5a7623907b6354d49bea7f9289371262e6a57f15458fb8908c5814`。两者不匹配。历史 `boot-diag-01` 与今日 `boot-current` 的只读启动解析都记录实际加载 `wn9l_hai1tai4ling2`，而不是 S15 目标模型。此项确认 `MODEL_MISMATCH_CONFIRMED`；它限制了 S15 设备观察对目标 WakeNet 模型的代表性，不定位其他 Voice/AEC 问题的根因。

此次为无语音请求的被动采集。只读 app 镜像身份与 ELF prefix 支持“app 镜像与 S15 固件一致”的有限结论；模型内容及运行加载模型与 S15 build 不一致。无 Wake marker 既不构成用户刺激结果，也不构成 WakeNet FAIL。捕获时长、输入统计和服务端点 marker 不证明 AFE 信号质量、AEC 效果、WebSocket 语音链路或稳定性。

当前恢复证据文件保存在私有目录 `E:/v6/s15-resume-20261018-01/`。报告只保留允许披露的摘要和 SHA-256，不包含原始 UART、语音、转写、端点凭据或原始日志内容：

| 证据 | 字节 | SHA-256 |
| --- | ---: | --- |
| Live model partition readback（2026-10-18） | 978,944 | `69949f497970885c03782948bafec0ab8bad1e9d53f37e20ff1ee6f8df715565` |
| Live `wn9_data` | 291,201 | `bf7e1fd2321cd73382f6e1b947f5a4cc727d493b5c2e300ae193347037414c2e` |
| S15 `srmodels.bin` | 291,042 | `7c87dd7adb5a7623907b6354d49bea7f9289371262e6a57f15458fb8908c5814` |
| `app-readback.bin`（2026-09-27） | 3,137,392 | `64deab2b695fd0858586e93b0cc23ef8e566df3a38d461826d77ab22d05c738a` |
| `boot-diag-01-summary.json` | 1,462 | `37d857fec197bdb21cf42611ca72980b242aa2d735d6f59709ba57a9838f6cf6` |
| `boot-diag-01-state.json` | 291 | `db0e1d66cfe82b4e78ed1eb7795e399471ce6f223b5d1945c1232ad64e42e79e` |
| `boot-diag-01-index.jsonl` | 81,248 | `70591b8cff5cfd3e519d5e46b6ae27c85e6fa3cdff0dbe60250c7cde0af0aed7` |
| `voice-rearm-02-state.json` | 267 | `25dbb9fa1b28c8aa68a585d0eb07bcf23a236e8e86b941c7d09d157d8f465e63` |
| `voice-rearm-02-index.jsonl` | 65,782 | `dd6a3a996f1b177090b24fd1a5481b1194727fba2f0ce9eb5563dd0e3419e393` |
| `voice-rearm-02-operator-events.jsonl` | 249 | `4eb7a533fa235ebf359c452d2e31743e2e4d5e6c5662b821551c65f004e1cc37` |
| `identity-verify.log` | 620 | `996a90fe8e3af5591a10f6d25f5cf35ee54b815c6a1ac287838a8071564e4693` |
| `boot-current-state.json` | 292 | `0823a9cba57f3329b059268f575f069949eb720dfc01b5608d3417b1f44cae2c` |
| `boot-current-summary.json` | 1,462 | `d7652a89a51d9b88143a9ccab41f10c46ebc4478a6cd89f94568499bd9b1c899` |
| `boot-current-index.jsonl` | 75,481 | `9c6e9e1307cef13413793690b82b8707e6643b7e9044b44747f0767e54bb8a4f` |
| `boot-current.raw` | 39,835 | `4717bbed463e804d29e3743b32f468019b6451b9828cd6a73e138d1dc13c34e9` |

## 结论与门禁

- S15 app 的 app-only 写入/readback、分区与 otadata 不变性有摘要和哈希证据；2026-10-18 只读身份核验再次确认该 app 位于设备上。模型目录及实际加载模型与候选构建模型不一致，状态 `MODEL_MISMATCH_CONFIRMED`。
- `boot-diag-01`、`voice-rearm-02` 和今日被动启动捕获均不构成完整 Wake→Speech→ASR→LLM→TTS→Playback→Next Wake 轮次。Preflight 仍为 `0/2–3`，正式连续 20 轮未开始。
- WakeNet 目标词验收、AEC 效果、Voice 服务链路、稳定性和 M1 PASS 均为 `NOT_VERIFIED`。模型修复授权待定；在授权和独立审查前不得写模型分区或做目标词验收。M0 仍 `CHANGES_REQUIRED`；AP outage/recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。
- `HARDWARE_VERIFY_REQUIRED`：后续模型分区写入需要用户明确授权其范围并先完成独立修复审查；本报告不构成额外写入或故障注入授权。
