# S-17 Wake model repair 设备证据报告

更新时间：2026-10-20。记录已授权 S-17 模型分区写入、读回、边界核验及后续同步 Wake/语音观察。模型修复和目标模型加载已核验；首次 Preflight 01 未听到答案，后续 Preflight 02 用户报告有响应但较慢且不正确。当前状态 `PREFLIGHT_CHANGES_REQUIRED_SLOW_INCORRECT_RESPONSE`，完成轮数仍为 0。

## Candidate 与授权范围

- Bundle：`claw4-learning-v6-m1-model-repair-s17-20261018-01`。
- 复用已冻结 S15 app，未重建：source `7f7357da2e1299b150a17b979fb1a307d027441e`；app 3,137,392 bytes，SHA-256 `64deab2b695fd0858586e93b0cc23ef8e566df3a38d461826d77ab22d05c738a`；ELF SHA-256 `33bcb2d01031a24814489aef84eaf155b30a37546f68534583b3aea90f7d5769`。
- S-17 模型镜像：291,042 bytes，SHA-256 `7c87dd7adb5a7623907b6354d49bea7f9289371262e6a57f15458fb8908c5814`；唯一模型为 `wn9_nihaoxiaozhi_tts`。
- R-10 独立审查 `PASS`，只覆盖所审查的单地址写入及同分区回退准备。用户明确确认该精确模型写入和有界回退后，唯一设备 owner root 执行。
- 写入前重新核验设备 MAC `80:f1:b2:d2:ed:14`、live 分区表、live app 和完整旧 model 分区备份。旧 978,944-byte model 分区备份 SHA-256 `69949f497970885c03782948bafec0ab8bad1e9d53f37e20ff1ee6f8df715565`，与 live 读回匹配并保留。

## 实际写入与独立读回

仅在 `0x111000` 写入 S-17 `srmodels.bin`。镜像为 291,042 bytes；esptool 实际写入 291,044 bytes，末尾含 2 bytes 对齐填充。4 KiB 擦除包络 `[0x111000, 0x159000)` 共 294,912 bytes，完全位于 live `model` 分区 `[0x111000, 0x200000)` 内，符合 R-10 审查的边界。没有写 application、分区表、bootloader、ota_1、otadata 或其他分区；没有重建固件，也没有执行回退。

独立读取 `0x111000` 起始的 291,042-byte 前缀与 S-17 模型镜像字节相同，SHA-256 `7c87dd7adb5a7623907b6354d49bea7f9289371262e6a57f15458fb8908c5814`。只读 catalog/payload 校验返回 `MATCH`：唯一模型 `wn9_nihaoxiaozhi_tts`，其 `wn9_data`、`wn9_index` 和 `_MODEL_INFO_` 的长度/哈希均与计划镜像一致。完整 978,944-byte model 分区读回 SHA-256 `09f698b9acb864b7670198baa3ffddc29d96d491002527ed25e2c56827a54f7a`。完整分区中擦除包络之外的字节与旧备份相同；包络内除目标模型镜像外的填充为 `0xFF`。

写后分区表与写前逐字节一致（SHA-256 `8e5a526458f90a2b32a28ca0162611ca680e7ac8ca49f8336999ad85b529521a`），otadata 也逐字节一致（SHA-256 `8ba3b110139f45443d4f268d1a3373ef99a1718b71d51664531b83ee2d4b91a3`）。app 写后只读 verify 成功，仍与上述 S15 app SHA 匹配。

## 首次启动观察与当前状态

写后以显式 USB reset 启动（不是冷启动），新建最长 600 秒的 `preflight-01` 捕获。初始被动启动摘要确认 S15 ELF prefix，runtime `Model 0` 与 `AFE_CONFIG` 均为 `wn9_nihaoxiaozhi_tts`，证明正确模型已被固件加载。初始 Wake control 摘要为 `on_total=1/on_rc=1/off_total=0/off_rc=0`；因 `off_total=0`，`off_rc=0` 是初始值，不表示执行过 OFF 调用。初始被动启动本身不算 Wake/Voice Preflight round；捕获后续覆盖同步用户刺激，结果见下文。

`preflight-01` 在写后显式 USB reset 后连续采集 395.281 秒，以 stop file 正常结束，无 SerialException。用户确认 Wake 成功且问了问题，但没有听到答案。摘要记录 AFE `detected_total=1`，parser 的 Wake log marker 为 0；这两个观测单独保留，不推断缺陷。采集见 WS 7444、3 个 STT marker（其中 1 个由私有解析器匹配到预期受控问题，不披露问题内容）和 3 个 TTS sentence marker。状态从 `idle→connecting→listening→speaking`，后续有多次 speaking/listening 活动并回到 idle。用户未听到可辨认答案，因此该尝试不是完整可验收 Voice Preflight round，也不计入 2–3 轮。

AFE 摘要为 381 个输入窗口、6,126,720 samples、RMS max 1,183、peak max 13,592、I2S read failures 0、crash markers 0；feed chunks 11,982、fetch OK 11,981、fetch failures 0。不要从不同汇总计数的差值推断丢帧或根因。最后控制计数 `on_total=2/on_rc=1/off_total=8/off_rc=0`；启用/禁用均有对应调用计数，返回码可按 pinned interface 解读。该状态活动不是 Wake rearm 缺陷的证明。

同期只读 NAS 诊断记录 service image `sha256:d8ab77bb1f2b212ee3f1d45fb9d9f9faf953be46c13c70ff6537044dd0b1e27e`、base image `sha256:9cf52d6b79c9d157356aba2c8bf1db645c2d2db4f14720c28128920020addccc`、config SHA-256 `3e9a54b782b403932607b1a5c1cc54a3bd4146d48bd2cf176de21639ad1d5438`，Compose SHA-256 `8e420b89d0381608b5833078168c53c3c41b741fc9f0e971f2630c6d3d488f91`。EdgeTTS 版本 7.2.6；在检查的 30 分钟 Docker 日志窗口观察到 25 条 HTTP 403 warning。直接合成 Ollama 请求返回预期数值 `2`，但不是完整端到端 TTS 证明。结合无可听答案，当前归类为 `WAKE_OBSERVED / VOICE_BLOCKED_NAS_TTS_403`。Root 将通过独立 L-17 处理 NAS 依赖；在其另行验证前不得记为已修复。此现场结果不判定其他 Voice/AEC 根因。

### Preflight 02：NAS 修复后的同步用户观察

`preflight-02` 在未重刷 S-17 app/model 的情况下，经显式 USB reset（不是冷启动）连续捕获 600.014 秒，按 `max_seconds` 正常结束且无串口错误；COM7 随后已释放。运行时仍为 S-17 app/model。NAS 绑定已更新的 EdgeTTS 7.3.1 image；本窗口观察到 0 个 HTTP 403，但有 3 次 NAS interrupt。独立 NAS provider probe 通过不等同于设备端播放验证。

用户报告本轮有响应，但较慢且回答不正确；尚无进一步的用户描述可精确说明实际听到的内容或主观延迟，故不推测。摘要记录 585 个输入窗口、9,404,640 mic samples、RMS max 1,377、peak max 14,038、I2S read failures 0、crash markers 0；AFE detected 总数 1，feed chunks/fetch OK 均 18,361，fetch failures 0。控制摘要为 `on_total=1/on_rc=1/off_total=6/off_rc=0`；各返回码只在对应调用计数大于 0 时有调用含义。状态时间线在 501,267 ms 记录回到 listening，556,615 ms 再记录 listening→speaking；不能据此称最终回到 idle 或 Wake recovery/next Wake 已验证，且没有 next-wake marker。

安全时间线标记：受控问题 ASR marker 在 439,032 ms；首个后续 TTS marker 在 445,914 ms；另有 3 个 ASR marker 在 447,779、450,985、452,490 ms；匹配预期数学答案文本的 TTS marker 在 494,463 ms。状态时间线在 501,267 ms 记录 speaking→listening，556,615 ms 再记录 listening→speaking；没有 next-wake marker，不能称最终状态为 idle 或 Wake recovery 已验证。用户反馈为有响应但慢且答错。Marker 只表示解析到的文本/状态事件，不证明对应音频实际播放或用户听到；echo-screen 的相似度也不是内容或播放确认。额外 ASR 的来源未明，不能据此判断 AEC、near-end 或中断根因。NAS provider latency 旧探针证据见 [L-18 NAS TTS repair report](V6_M1_NAS_TTS_REPAIR_REPORT.md)：截至本次 L-19 证据冻结，记录的 native Ollama 探针以 exit 1 结束且结果为 null，错误详情未保留；先前 production Ollama provider 请求在 90 秒总超时后以 exit 124 结束，表示请求未完成，不足以测定首字延迟。该旧探针不能确认 `think=false` 对照或定位延迟根因；后续 10/20 新探针是独立证据，另行记录。

本次结果不是通过的 Preflight round：`PREFLIGHT_CHANGES_REQUIRED_SLOW_INCORRECT_RESPONSE`，completed rounds `0/2–3`。正式连续 20 轮未开始；AEC、playback、near-end、next Wake 均 `NOT_VERIFIED`，M1 未验收。M0 仍 `CHANGES_REQUIRED`，AP outage/recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。S08/S11/S15 等旧 Candidate 设备结果不继承到 S17。

本次安全摘要及哈希（私有路径 `E:/v6/s17-device-20261018-01/`；raw/index 仅计算哈希，未读取内容）：

| 证据 | 字节 | SHA-256 |
| --- | ---: | --- |
| `preflight-02-state.json` | 295 | `ee3128e0cd1123eac0a6bffc1f663cb8836e52a880d2ac6fe3e8baf55e7ba0b7` |
| `preflight-02-summary.json` | 1,474 | `6a18357db3b4328a39f869a12981f84af798964ae55849d58518099f3837d421` |
| `preflight-02-safe-timeline.json` | 3,815 | `38fd15c6b7b9a356a8ee719485fc9ea1bb3e4a9fe1807e877b88a247ddd1c352` |
| `preflight-02-echo-screen.json` | 1,406 | `fc388e7cf2e8305317ca69fd4be3fdd352e69d1a917b4f4360ae00e236575633` |
| `preflight-02-nas-safe-events.json` | 3,849 | `12453d776983cd8383c23ba99a7dbd98f2547485a417c07d0db55ce28aae9502` |
| `preflight-02-environment.json` | 2,245 | `2a1731e2577ae3e37200dea05ecaf57d43df4d8869a57238e483745fef55903c` |
| `preflight-02-operator-events.jsonl` | 342 | `42bb95ae483e255a56ce2533d1e329e628dd1efcd5180ffb713cdee650ddc2e7` |
| `preflight-02-index.jsonl` | 492,472 | `ea9b3aa743224b0a64a6bfe71ae949d3f9846b40e61fe024e706c77c5668c4f7` |
| `preflight-02.raw` | 347,829 | `9b56c2353f7d6a74d499813d34834bacd2fafe34f0382adac872dbeecb7f1a93` |
| `ollama-latency-native-probe.json` | 233 | `f8789d183a6982b6dd37fd32e34e81947738c7298defd73d624d0751b74e2f04` |

S17 model/app 身份延续写入后已核验身份；本次环境 JSON SHA-256 与 NAS-safe-events JSON SHA-256 分别为上表所列。原始 UART、语音、转写、凭据和日志文本未写入报告。

## 证据索引

允许披露的证据仅来自私有目录 `E:/v6/s17-device-20261018-01/` 中的审计 JSON 和二进制摘要；不包含原始 UART 内容、语音、转写、凭据或原始日志文本。

| 证据 | 字节 | SHA-256 |
| --- | ---: | --- |
| `execution-precheck.json` | 1,752 | `f42434c51fac6067a93a8d1c67bd47b7ca395b1f0ffd25dba5e0b4c44829ccc3` |
| `device-prewrite-verified.json` | 498 | `1527b3ce897428c72f768d281899e99505b45d3207d096d9ab4276c96f884a1e` |
| `model-verification.json` | 1,697 | `f3ac93aaab36835d380222dfa0d62d7e08a71963e0a685fb90cda6b580cdc49a` |
| `postwrite-boundary-verification.json` | 290 | `eb83bd3a1679852cec69bbdcae300e946fa2f1fa7cd4196fd7f8e160ac5d0d25` |
| `runtime-model-verification.json` | 365 | `ca7531f3d6a49930fc98657f2fc2bd0a5e0e092cd0931d4619c19646fd2a6cdf` |
| `model-readback-prefix.bin` | 291,042 | `7c87dd7adb5a7623907b6354d49bea7f9289371262e6a57f15458fb8908c5814` |
| `model-readback-full.bin` | 978,944 | `09f698b9acb864b7670198baa3ffddc29d96d491002527ed25e2c56827a54f7a` |
| `preflight-01-state.json` | 295 | `c11050e0c853f4eda5084cbcce65173bb087d631110f3667910964782aaf8816` |
| `preflight-01-summary.json` | 1,473 | `382894569fad6d9d10d12d2f8a4172c2f161f8aaa54296b7e4acef6ba7c8843d` |
| `preflight-01-operator-events.jsonl` | 411 | `b54fa62d9eee1b622c3b02f6d6ea22a31e669dc62b1cc6faf83acb3766547dd0` |
| `preflight-01-safe-timeline.json` | 2,095 | `221da5c8e1fa79b42680fe4ff9d7116fb5850de8266683b42d14ae0c0a782eba` |
| `preflight-01-index.jsonl` | 337,390 | `e0afc16bb9b00311259908d38b600cc33edd9fc397c65f9cde3a0f95d7d69c1f` |
| `preflight-01.raw` | 231,729 | `8e23fb82f300cdc6f8577a40c77a89e4cc987e986133addb74867013f0972d6f` |
| `nas-diagnostic-summary.json` | 698 | `ebd7346496d37613922395d0f89a078f87e7af2feddcbe17f2320dbdf41cbcc1` |

`HARDWARE_VERIFY_REQUIRED`: Preflight 02 response quality and actual playback remain unresolved; user feedback says the response was slow and incorrect, and parsed text markers are not playback proof. Await the user's description and ongoing S18/NAS latency diagnostics before assigning a root cause. Any expansion of the already-approved model write or same-partition rollback scope requires separate authorization.
