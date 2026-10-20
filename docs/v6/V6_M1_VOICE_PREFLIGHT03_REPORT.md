# S-17 Voice Preflight 03 设备证据报告

记录日期：2026-10-20。本文记录 S-17 在 L-20 NAS Voice provider 部署后的 `preflight-03` 同步观察。观察到 Wake 和用户确认的可听响应，但答案正确性未确认，因此不是通过的 Preflight 轮次。状态为 `WAKE_OBSERVED / AUDIBLE_RESPONSE_UNCLEAR / PREFLIGHT_CHANGES_REQUIRED`；completed rounds 仍为 `0/2–3`，正式 20 轮未开始。

## 候选与开始身份

本次继续使用 S-17 app/model，没有刷写设备。`run-preparation.json` 绑定 bundle `claw4-learning-v6-m1-model-repair-s17-20261018-01`、app source `7f7357da2e1299b150a17b979fb1a307d027441e`、app SHA-256 `64deab2b695fd0858586e93b0cc23ef8e566df3a38d461826d77ab22d05c738a`、ELF SHA-256 `33bcb2d01031a24814489aef84eaf155b30a37546f68534583b3aea90f7d5769`，model `wn9_nihaoxiaozhi_tts`、model SHA-256 `7c87dd7adb5a7623907b6354d49bea7f9289371262e6a57f15458fb8908c5814`。运行时 ELF prefix 与此 bundle 对应；启动为显式 USB reset，不是 cold boot。

开始身份核验同时绑定 L-20 NAS image `sha256:34fb7e5daa3cf24818b459e95b0eb772063788fc52b331841e5780beed48f0cd` 与 config SHA-256 `3e9a54b782b403932607b1a5c1cc54a3bd4146d48bd2cf176de21639ad1d5438`；image/container 与运行服务身份匹配，restart count 0。采集窗口 NAS safe events 未观察到 HTTP 403；其 `raw_window_sha256` 字段为 `507980b46a43cdb13cb1744877bdea1a52fa69d6cd167747a7027e31170c6640`，service identity 未变化。

## 捕获与用户观察

`preflight-03` 连续捕获 438.889 秒，由 stop file 正常结束，无 serial error；COM7 已释放。设备摘要为 425 个输入诊断窗口、6,831,680 mic samples、I2S read failures 0、crash markers 0。AFE detected 总数 1；feed chunks 与 fetch OK 均为 13,315，fetch failures 0。显式 reset 不是 cold boot，本次没有刷机。

用户确认 Wake，并报告听到包含“1/2”内容的回答，约 5 秒；答案是否正确未确认。之后用户确认还重复提问并说过其他话，因此后续额外 ASR 不能归因于设备回声或 near-end 失败。原始语句不写入报告。

私有 semantic screen 对 ASR marker 286,062 ms 和 302,063 ms 的受控问题均在混合中英数字、全半角归一化后精确匹配。初次严格正则筛选未命中属于 parser matching limitation，不是 ASR 错误证据。response groups 中这两组各含 4 个 TTS chunks、13 个字符摘要，text SHA-256 均为 `ffa6a6dd7cc47c76ecc79a1681b7e739062835d5e4374b4a1d241be56bbe2d79`。首组 first TTS text marker 为 292,252 ms，即 ASR marker 后 6.190 秒；第二组 first TTS text marker 为 304,034 ms，即其 ASR marker 后 1.971 秒。TTS marker 是解析到的文本分段事件，不代表音频实际开始播放的时刻，也不能证明用户听清或答案正确。后续 343,177 ms 的额外 ASR / response group 不与受控问题绑定；用户确认其间说过其他话。不得据此把额外 ASR 判为回声。

NAS safe events 观察到 0 个 HTTP 403 和 2 个 interrupt 类事件；事件摘要不包含原始日志文本，不能单凭类别归因于设备回声或用户操作。L-20 provider service 的合成验证仍保留 `NAS_VOICE_PROVIDER_SERVICE_VERIFIED`，但与本次设备端可听答复的正确性、AEC 或 near-end 无等价关系。

## 结果与未验证项

本次确认 Wake 并有可听响应，但正确性不明，所以不能计作通过的 Preflight：`PREFLIGHT_CHANGES_REQUIRED`，completed rounds `0/2–3`。安全时间线最后状态迁移为 353,195 ms `speaking→listening`，没有观察到回到 idle 或 next-wake marker；Wake recovery 未验证。正式连续 20 轮未开始，`M1` 未验收。设备端 playback quality / answer correctness、AEC effectiveness、near-end、next Wake / Wake recovery 均 `NOT_VERIFIED`。S-17 `preflight-02` 慢且答错的历史失败仍保留，本次有限观察不消除其未解决缺口。

S-19 Sol 正在审计实际 LLM 动态 prompt 到分段/清洗路径。Root 的 flat system/user 合成算术探针约 877 ms 且结果正确，但并未复现实际服务连接路径，不能据此定位本次慢答或正确性不明的根因。当前不对根因、AEC、near-end 或完整 Voice 流程作结论。M0 仍 `CHANGES_REQUIRED`；AP outage/recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。

## 私有证据索引

允许披露的摘要 JSON 来自 `E:/v6/s17-voice-l20-20261020-01/`。表中记录实际字节数及 SHA-256。`preflight-03.raw`、`preflight-03-index.jsonl` 和 `source-audit-s19.zip` 只计算哈希，未读取内容；不会复制原始 UART、语音、转写、prompt 或凭据。

| 文件 | 字节 | SHA-256 |
| --- | ---: | --- |
| `run-preparation.json` | 2,406 | `b4f126da5c41e63e736595c03726711cd4d0fe8a410ebb0e613328ac030e12a0` |
| `preflight-03-start-identity.json` | 539 | `56c0833a839e48847c79bb44963ad730e9fc3601de7e6404603e051e894dd060` |
| `preflight-03-state.json` | 295 | `d0e397a3d484c7d95c0c0749ac981e0110ab9841bb9f49676b303dc4d20f6bc7` |
| `preflight-03-summary.json` | 1,474 | `3d034cb7a0df7aa8063f0f1b76b87669f5c22313a5ee84ce4acd521f2e3fcc37` |
| `preflight-03-operator-events.jsonl` | 539 | `76285c20cdd8c991aa812b199cd70aec394d33590d1c452ba7a3261ef44ee131` |
| `preflight-03-semantic-screen.json` | 3,769 | `48233d4edd67de5611681c8046839a0ce19e6e758f5f18d934c6d28c83778e24` |
| `preflight-03-response-groups.json` | 1,091 | `56e276728e27e5575710988b2a87a1f276de8024cc7db694e761f79d118c01d1` |
| `preflight-03-nas-safe-events.json` | 7,398 | `8d400d6510e5336ab15691d0962b0aa1dd844d9af112f261233c711662b17bab` |
| `preflight-03-safe-timeline.json` | 7,255 | `135b3cf97ee838fa9e8e4cdef8747fdf3aa8349c65c7be3dcc5e729102a88bd6` |
| `preflight-03.raw`（仅哈希） | 256,931 | `cff2e5a4ba336e905b79a63cea1fc6a8c8dd7af34e3bd59d79c9620aec17c0e1` |
| `preflight-03-index.jsonl`（仅哈希） | 367,839 | `1be93467521ccc9f112b8fe7a15ea3611d11d7924f389050bd7377711331d369` |
| `source-audit-s19.zip`（仅哈希） | 40,234 | `21b78b919e1aff60a4857da3d79af92c63c6a908e0fa5c8425b425dbde937e4c` |

`HARDWARE_VERIFY_REQUIRED`: audible response correctness, actual playback timing, AEC, near-end and next Wake remain unverified. No complete Preflight round is credited.
