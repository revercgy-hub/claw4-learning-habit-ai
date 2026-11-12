# Claw4 当前任务看板

### 2026-11-12 S-30 计时面板与番茄钟复测

新版面板已刷入。真机一分钟倒计时、重复响铃、确认停止和随后重新唤醒均有记录，操作者确认正常。普通对话回环仍存在。见 [V6 S-30 证据](../v6/V6_TASK_BOARD.md#2026-11-12-s-30-计时面板与番茄钟复测)。

### 2026-11-12 S-29 到点重复响铃与确认

S-29 已刷入并完整读回。一次性闹钟真机重复响铃及点击 Stop Alarm 后停止通过，番茄钟也使用相同响铃逻辑。见 [V6 S-29 证据](../v6/V6_TASK_BOARD.md#2026-11-12-s-29-到点重复响铃与确认)。

### 2026-11-12 S-28 计时提示音与语音回采修复

S-28 提示音应用已写入、完整读回并启动。操作者确认语音启动一分钟倒计时，报告到点及稍后的补播均未听见，并复现持续自行接话。实机可听提示和语音回环仍需修正。见 [V6 S-28 证据](../v6/V6_TASK_BOARD.md#2026-11-12-s-28-计时提示音与语音回采修复)。

### 2026-11-08 S-27 闹钟与番茄钟真机预览

应用已写入并完整读回，设备启动与校时已核验。NAS 的模拟设备回执和普通问答已复测。触控、语音计时、到点提示待操作者验证。见 [V6 S-27 记录](../v6/V6_TASK_BOARD.md#2026-11-08-s-27-闹钟与番茄钟真机预览)。

### 2026-11-08 L-40 闹钟与专注倒计时

主机逻辑已实现并通过测试。真机界面、语音设定和提示音待 M1 验收后的阶段放行。见 [V6 L-40](../v6/V6_TASK_BOARD.md#2026-11-08-l-40-闹钟与专注倒计时host_verified)。

### 2026-11-08 L-39 NAS 实时天气与新闻（NAS_SERVICE_VERIFIED）

用户授权增加实时天气和新闻。服务在既有语音会话中处理常见城市的当前、今天及明天天气，并播报中新网当日最新至多三条标题。天气请求仅使用预设城市坐标，新闻使用固定 RSS 请求。缺少支持的城市或数据源失败时有明确提示。集成源码 `fafc8ce9aee3410e02b9f0cd41a00c69e9414547`，本地 V6 Host `223/223`。隔离及正式 NAS 各通过 `6/6` 合成 WebSocket 文本到 Opus 用例，覆盖当前天气、今天、明天、新闻、城市提示、普通算术。正式镜像 `sha256:06d26d826107edc2e4ecd1a0bf07367b354c66d81328e9836438f650748e370c`，容器 `aa6bd98417a4d39449c10af8667fa335c51764f3d062f67b47b25a1b37682a5c`，restart 0，配置 SHA `e56859eb205c6124c66cc30275d643556c9666e66ec2ceaa1ae4f4f55f344911`，OTA HTTP 200 / WS 端点核对通过。正式测试证据 `evidence/live-lookup-20261108-01/active-ws.json` SHA-256 `4c5ec42f96fd83909d285165cce263bd48e2b619505080aa71fd5adbdca49660`。真机 ASR、扬声器和设备轮次仍待验证。M0 `CHANGES_REQUIRED`，AP `SKIPPED_BY_USER / NOT_VERIFIED`，Preflight `0/2–3`，正式 20 轮 `NOT_STARTED`。 [部署与回退](../../deploy/v6/xiaozhi-nas/README.md#live-weather-and-news)。

### 2026-11-08 L-38 NAS synthetic voice-flow evidence

Root's isolated synthetic WebSocket/provider observations and S-26 static source audit are recorded in [L-38 report](../v6/V6_M1_NAS_VOICE_FLOW_REPORT.md). The text-only `listen.detect` path bypasses ASR but reaches same-session Dialogue/Ollama/TTS; TTS FIRST is separate from actual Opus arrival. The first fresh-blue dialogue lacked a system message and returned 200 characters before client timeout; other six initial cases had the expected prompt. This supports a first-turn prompt-readiness hypothesis only. Count segmentation split the first `1`; slow second Edge TTS took 15,785 ms with three failed attempts, while fast second took 2,576 ms without retries. S-25 reclassified both historic segments with zero outer retries, so isolated retries do not explain its 11.989643 s interval. Initial literal-suffix blue exactness was 0/3 for baseline and candidate; directed V2 was 2/3 (fresh and after-theme passed, after-count failed), with violet/explanation/arithmetic controls passing. Safe summary SHA-256 `58a20b705db69b34fb2780e0bad9cb9d25c97c3b752d97e0101f0d0182b04179`. No production changes, device/AEC/latency result, or gate promotion. S-24 DEVICE remains per S-25; M0 `CHANGES_REQUIRED`, AP `SKIPPED_BY_USER / NOT_VERIFIED`, Preflight `0/2–3`, formal 20 `NOT_STARTED`.

### 2026-11-08 S-25 S-24 真机回声诊断

S-24 已 app-only 写入 `ota_0`，完整读回与候选一致。实际分区表、启动选择区及目标模型镜像范围读回一致，写前完整扇区备份已验证。连续采集 371.705 秒，确认 ELF、NAS 端点及 357 个配对音频窗口、351 个 AFE 快照。安静数数首两段 NAS 发送标记相隔 11.990 秒。插话复测精确识别受控短句且 ASR/LLM 哈希匹配，操作者报告答复偏离指令。观察到参考信号可用及一次回复期间 Write 内短缺，物理回采根因未验证。L-37 证据工具定向 7/7、V6 Host 209/209。Preflight `CHANGES_REQUIRED`，仍计 `0/2–3`；正式 20 轮未开始，M0 `CHANGES_REQUIRED`，AP `SKIPPED_BY_USER / NOT_VERIFIED`。见 [S-25 设备报告](../v6/V6_M1_AEC_DIAGNOSTIC_DEVICE_REPORT.md)。下步在服务侧分别复现答偏和分段停顿。

## 2026-11-08 L-36 S-24 AEC diagnostic candidate evidence (DOCS_COMPLETE / BUILD_REVIEW_PASS)

The L-36 docs-only record is complete: [V6 L-36 report](../v6/V6_M1_AEC_DIAGNOSTIC_REPORT.md). Candidate `claw4-learning-v6-m1-aec-diag-s24-20261108-01`, source baseline `5a9dbfcf66ad6d8c505a8d3be8126e8362d1fb03`, manifest SHA-256 `8c082e7199e81561d93d648e17abe6e0f9d76aa0043282bddb3e51b5d7f45fbe`; isolated build completed and R-18 `PASS`. Candidate remains unflashed; device validation `NOT_VERIFIED`; no Flash authorization is inferred. Historical Preflight remains `0/2–3`; formal 20 rounds `NOT_STARTED`; M0 `CHANGES_REQUIRED`; AP recovery `SKIPPED_BY_USER / NOT_VERIFIED`.

### 2026-11-08 L-34 S-17 Voice Preflight 05

Device state is updated by [L-34 report](../v6/V6_M1_VOICE_PREFLIGHT05_REPORT.md): one 427.176 s capture after USB reset (not cold boot), no build/Flash/app readback; target ELF prefix/model/NAS OTA+WS observed. Three exercise scenarios are not credited as full rounds. R2 contains two ASR→LLM forwards matching preceding TTS normalized substrings during/at playback boundary, alongside one reported controlled near-end marker and audible answer. This is a strong recapture/self-reply observation for that interruption exercise; acoustic root cause and general AEC/near-end remain unverified. R1/R3 positive operator observations and their bounds are recorded in L-34. NAS service remains the valid L-33 prewarm deployment; device and service evidence are separate. Preflight `PREFLIGHT_CHANGES_REQUIRED / TTS_RECAPTURE_OBSERVED`, `0/2–3`; formal 20 `NOT_STARTED`; M0 `CHANGES_REQUIRED`; AP `SKIPPED_BY_USER / NOT_VERIFIED`. S-23 source audit leaves root cause unproven; no speculative fix or additional retest requested.

> **SUPERSEDED 历史快照（2026-09-23）**：Candidate19 已构建但未刷机等历史技术事实保留；其中旧的“当前有效/无 WorkBuddy 活动流”调度文字已由 2026-09-24 接管状态覆盖。Candidate17 的设备证据不转移至 Candidate19。

## 2026-09-24 接管及后续开发状态（当前有效）

### 2026-11-08 L-33 Ollama prewarm service evidence

A-07 bounded internal connection-scoped preload candidate `claw4-nas-prewarm-20261106-01`: source `d587f415e29fc3385eebd1858efce8847a5cba60`, S-22 fix `f95ce4679e8842cf21b68353ed81a99e2e8b0dd0`; R-16 `PASS`, targeted Host `9/9`, integrated V6 Host `199/199`. Image built 2026-11-06. Isolated synthetic WS text-to-server-audio `2/2`; first case model absent before connect and at hello (7 ms), observed resident before question (8472 ms from connect); question-to-first-Opus `2173/1513 ms`. Root rollout completed 2026-11-08 Asia/Shanghai: active image `sha256:97c36ec5638a97d47756e906e0c449926ebf4192e701c6bece0ce7912c99d1c1`, container `6f7389a8a62b3215b48404753fc66278712b1e4c7a2f8778109f47f08b38cc20`, restart 0, config `e56859eb205c6124c66cc30275d643556c9666e66ec2ceaa1ae4f4f55f344911` unchanged. Local HTTP 200/expected WS. Active synthetic WS `2/2`, expected answer/TTS stop, 0 HTTP 403/decode errors; question-to-first-Opus `1932/1835 ms`. Compose delta image/build only; old image retained as rollback target, no rollback. First owner guard attempt stopped before mutation due to NAS Python 3.10 lacking `hashlib.file_digest`; the evidence script was corrected to streaming hash before successful retry with same reviewed image/tests. No helper INFO marker was visible; no helper-load-duration or confirmed resident-branch-log claim. This is synthetic service-path evidence, not controlled cold/warm end-to-end A/B, device turn, ASR, or playback validation. L-25 is historical for its prior runtime; L-27 prior device evidence does not transfer. M0 `CHANGES_REQUIRED`; AP `SKIPPED_BY_USER / NOT_VERIFIED`; Preflight `0/2–3`; formal 20 `NOT_STARTED`; M1 not accepted; no M1.5/UI work. Full provenance and artifact SHA-256 values: [L-33 report](../v6/V6_M1_OLLAMA_PREWARM_REPORT.md).
### 2026-11-06 L-32 NAS synthetic stage profile (component evidence only)

Three isolated synthetic trials exercised the actual Ollama provider and EdgeTTS stream/Opus decode wrappers serially with fixed input. LLM first yield was 6798.94/209.69/185.12 ms; full LLM generation 6969.56/380.74/354.53 ms; TTS first MP3 chunk 1266.23/1201.92/1622.91 ms; TTS request 1652.47/2586.35/2157.92 ms; first Opus frame 1856.11/2683.83/2257.40 ms. All returned the expected synthetic relation and decoded 33 frames / 31,680 samples without decode error. The serialized component probe is not a live device turn, cross-service latency, streaming-overlap test, or controlled cold/warm comparison; model was already resident and no unload/keep-alive change was made. A separate synthetic native request after natural model expiry found `resident=false` and completed in 5577 ms wall (load 4962.079 ms, prompt evaluation 425.016 ms, generation 186.803 ms); this demonstrates load dominance for that request only and does not explain the S-17 8.325-sec marker interval or device latency. NAS image/config/container identity and restart count 0 remained unchanged. R-15 independently reviewed integration `8892ae63cbfdf096088f3f7b9b59a304bc71c565` PASS; restored full integration Host result is 190/190, zero failure/error/skip. See [L-32 report](../v6/V6_M1_STAGE_PROFILE_REPORT.md). It does not alter L-27's positive audible response, Preflight `0/2–3`, formal 20-round NOT_STARTED, M0 `CHANGES_REQUIRED`, or AP outage/recovery `SKIPPED_BY_USER / NOT_VERIFIED`. S-22 A-07 implementation is reviewed R-16 `PASS` and Root completed the scoped synthetic service rollout; see L-33 for current NAS service identity and evidence limits. No DEVICE/ASR/M1 acceptance is claimed.

### 2026-11-06 L-27 S-17 Voice Preflight 04 (current device evidence)

The 300-second continuous capture used the existing S-17 app/model after USB reset (not cold boot); no build or Flash. One valid ELF anchor matched `33bcb2d01`; target model, NAS OTA and WS were observed. The operator confirmed an immediate Wake and a correct audible controlled answer; normalized ASR marker matched the controlled question and expected-answer TTS marker was observed. ASR→TTS text-marker interval was 8.325 s; it is not measured speaker playback latency. One ASR event and no additional ASR were observed during this capture, which does not establish AEC PASS. A same-window NAS `LLM_INPUT` later matched the configured end-prompt hash and followed the 120-second silence timeout path; this explains the late speech-state transition as scheduled close/farewell, not a second ASR result. The user also reported a second immediate Wake at the capture boundary, but no second Wake is bound in UART: preserve as `USER_REPORTED_NEXT_WAKE / UART_NOT_VERIFIED_CAPTURE_BOUNDARY`, not a verified Wake or failure. Capture had 286 input windows, RMS max 2615, I2S read failures 0, crash markers 0; heap/PSRAM unknown. Preflight remains conservatively credited `0/2–3` because AEC, near-end and full next-Wake binding remain incomplete; formal 20 rounds unstarted. M0 remains `CHANGES_REQUIRED`, AP outage/recovery `SKIPPED_BY_USER / NOT_VERIFIED`. See [L-27 Preflight 04 report](../v6/V6_M1_VOICE_PREFLIGHT04_REPORT.md). L-25 NAS synthetic service verification remains valid for its prior runtime; current service is L-33, separate from device evidence.

### 2026-11-06 L-25 NAS ASR/prompt repair (historical service snapshot; current service is L-33)

L-25 source `150fe555a83105ad10852865ec363404f24d9a78` follows S-20 `f436a8f` and L-24 `b58e958`; normal push and remote SHA were verified. S-20 focused Host tests passed `10/10`, full V6 Host suite `175/175`. R-13 initially identified one README rollback-guard defect; L-24 fixed it at `f2d758f2eb1885b457d4e1e3d3f3a60de77bf2dc`, and R-13 resumed review returned scoped `PASS` at `ce19ee77f037c2973c5c835779d2926f81e1a34d`. Route repair L-26 first failed, then used Compose `!override`; R-14 returned `PASS` on integrated source `426f187c4ec9c916c09e4edebfbdea919ccce7b8`. Root's guard rejected an ambiguous ordinary `extra_hosts` merge before live config/service mutation; the corrected production and rollback overlays each resolve to one NAS mapping. Root deployed without rebuild or Flash: image `9cd9…`, config `e568…`, container `5af9aa…`, restart count 0, rollback not executed. Exact-byte predeployment JSON manifest SHA256 `0b61eca72f33ab98d482dafc1d1f337c0a5a0bf908baf4e0219e1c9007ce7d16`. Post-deployment identity and OTA checks passed (HTTP 200; expected WS endpoint; source/Ollama/template and model identities matched). Two independent synthetic WebSocket text probes matched expected answers and TTS stop; binary 4,377/4,622 bytes, decoded 31,680/32,640 samples at 16 kHz, decode errors 0, first audio 8,494/2,086 ms, total 10,543/4,190 ms. This is `NAS_SERVICE_VERIFIED` for a synthetic service path only: probes bypass ASR producer, and prompt/cache introspection was from a new Python process, not live server memory. Preflight remains `0/2–3`, formal 20 rounds unstarted; AEC/near-end/next Wake `NOT_VERIFIED`. No device test, Flash, rebuild, M0/M1 acceptance, or AP recovery verification. M0 remains `CHANGES_REQUIRED`; AP outage/recovery remains `SKIPPED_BY_USER / NOT_VERIFIED`. See [L-25 report](../v6/V6_M1_ASR_PROMPT_REPAIR_REPORT.md).

### 2026-10-20 L-23 S-17 Voice Preflight 03 (historical snapshot)

> **SUPERSEDED current status:** L-23 remains historical evidence. Current NAS service evidence is L-25; current device observations are L-27.

L-20 修复后的 `preflight-03` 使用同一 S-17 app/model 与已核验 NAS image/config，没有刷写设备。438.889 秒 UART capture 按 stop file 正常结束、无 serial error，COM7 已释放。用户确认 Wake 并听到包含“1/2”的回答，报告延迟约 5 秒但答案正确性不明；ASR 286,062 ms 与 302,063 ms 经中英数字、全半角归一化后精确匹配受控问题，严格正则筛选未命中属于 parser matching limitation，不是 ASR 错误。两组 TTS text marker hash 相同，marker 只代表解析到的文本分段，不证明音频开始播放时间或回答正确。用户随后确认重复提问并说过其他话，因此额外 ASR 不归因于回声。最后安全时间线状态迁移为 `speaking→listening`，未观察 idle 恢复或 next Wake。NAS safe events 本窗口 HTTP 403 为 0，image/config/restart state 未变。当前状态 `WAKE_OBSERVED / AUDIBLE_RESPONSE_UNCLEAR / PREFLIGHT_CHANGES_REQUIRED`；Preflight `0/2–3`，正式 20 轮未开始；AEC、near-end、next Wake `NOT_VERIFIED`。NAS provider service verification 保留，但不是 device Voice PASS。S-19 Sol 动态 prompt→分段/清洗审计仍在进行，Root flat system/user synthetic probe 不复现实际连接路径，根因未定。M0 仍 `CHANGES_REQUIRED`，AP outage/recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。报告：[S-17 Voice Preflight 03](../v6/V6_M1_VOICE_PREFLIGHT03_REPORT.md)。

### 2026-10-20 L-21 NAS Voice provider repair (historical service verification; device state updated by L-23)

L-20 source `2e9be855f3382db7f8182adfd197e94f88583e95` 已整合至 `531ac23e8ed8608179ad83d42d163c0b8f40efa5`；R-12 独立复核 `PASS` 仅覆盖 CODE/HOST/readiness。worker Host 汇总 `165/0/0/0`，定向测试 `7/7`；Root 实际 NAS Docker build exit `0`。部署 image `sha256:34fb7e5daa3cf24818b459e95b0eb772063788fc52b331841e5780beed48f0cd` 正常运行，restart count 0；仅 image/build 改变，NAS config SHA `3e9a54b782b403932607b1a5c1cc54a3bd4146d48bd2cf176de21639ad1d5438`、provider/model 身份未变。旧 L-17 EdgeTTS fixed image `sha256:acf7314b488ad7e49232797fb42711faad637c00d29142ec782124a92b1d9291` 保留作回退目标，未执行回退。部署后的 service/provider probe first-content `425 ms`、LLM total `459 ms`、预期结果匹配；TTS 音频 `6,480` bytes，SHA-256 `32c81da5dde3e7eff8c1878758d9e30d2f5617f07ab5ebbe524f205ba6ffb6d1`，解码 `1,080 ms`、RMS `3,510`。判定限于 `NAS_VOICE_PROVIDER_SERVICE_VERIFIED / DEVICE_PREFLIGHT_PENDING`，不证明设备播放或完整 Voice 修复。此前 S-17 `preflight-02` 慢且答错保留为历史失败，不因服务探针而消除；修复后尚无新真机测试，Preflight 仍 0/2–3，正式 20 轮未开始。Root 已询问用户是否准备新测试，未收到回复前不记为开始/通过。M0 仍 `CHANGES_REQUIRED`；AP outage/recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。详见 [L-21 NAS Voice provider repair report](../v6/V6_M1_NAS_VOICE_PROVIDER_REPAIR_REPORT.md)。

### 2026-10-20 L-17 NAS TTS repair and L-18 evidence sync (historical TTS/S-17 evidence)

L-17 source `812b12905cda01843f5aeb2d80835abb9bc2b084` 已经 R-11 `PASS` 并部署到 NAS。Root 从原 XiaoZhi v0.9.6 digest-pinned base 构建并运行本地 image `claw4-xiaozhi-v6:tts-731-20261018-01`（image ID `sha256:acf7314b488ad7e49232797fb42711faad637c00d29142ec782124a92b1d9291`），唯一依赖变化为 EdgeTTS 7.2.6→7.3.1。NAS-local config SHA 未变化，service/Compose identity 除 image/build 外保持一致；旧 image 保留但未执行 rollback。部署后 production-provider service probe 生成 11,664-byte 可解码音频（1,944 ms、RMS 4,305、24 kHz mono）。隔离 probe JSON 的音频 SHA 字段长度异常，未作为有效音频 hash；详见 L-18 报告。据此仅记 `NAS_TTS_SERVICE_REPAIR=PASS`，不记 DEVICE playback。S-17 原始 TTS 403 失败窗口保留历史；Root 未重刷 S-17 app/model，`preflight-02` 已结束（600.014 秒，按上限正常停止，无串口错误）。用户报告有响应但慢且答错，设备窗口有 3 次 NAS interrupt、0 个 HTTP 403；文本/状态 marker 不证明播放。设备状态为 `PREFLIGHT_CHANGES_REQUIRED_SLOW_INCORRECT_RESPONSE`，completed rounds `0/2–3`；正式 20 轮未开始，M1 未验收。截至 L-19 证据冻结，旧 Ollama native probe exit 1/result null 且错误详情缺失；先前 production-provider 请求 90 秒总超时 exit 124，仅能说明总请求未完成，不能推断首字延迟或 `think=false` 对照。后续 10/20 新探针为独立证据，另行记录。NAS 同 image 仍运行、restart count 0。M0 仍 `CHANGES_REQUIRED`，AP outage/recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。详情与证据 JSON 实际 SHA-256 见 [L-18 NAS TTS repair report](../v6/V6_M1_NAS_TTS_REPAIR_REPORT.md) 和 [S-17 device report](../v6/V6_M1_WAKE_MODEL_REPAIR_DEVICE_REPORT.md)。

### 2026-10-18 S-17 Wake model repair and first NAS TTS failure window (historical)

S-17 bundle `claw4-learning-v6-m1-model-repair-s17-20261018-01` 复用 S15 app，未重建。R-10 `PASS` 后，用户明确授权精确 model 分区写入及同分区 full-backup 回退；唯一硬件 owner 执行了该范围内的单次模型写入。独立 291,042-byte prefix 与 S17 镜像匹配，完整 978,944-byte model readback、边界、partition table/otadata 和 app 核验均通过。首次写后显式 USB reset 启动加载目标模型并匹配 S15 ELF。395.281 秒同步捕获中，用户确认 Wake 成功并提问，但没有听到答案；观测到 WS 7444、STT/TTS 标记和回到 idle 的状态迁移。NAS 只读诊断观察到 EdgeTTS 7.2.6 的 25 个 HTTP 403 warning，直接 Ollama 合成请求成功不构成端到端 TTS 验证。该时点状态为 `WAKE_OBSERVED / VOICE_BLOCKED_NAS_TTS_403`，Preflight `0/2–3`；首次失败窗口保留为历史。当前 NAS provider/service 与 S-17 设备状态见本节 L-23 report；详见 [S-17 设备报告](../v6/V6_M1_WAKE_MODEL_REPAIR_DEVICE_REPORT.md) 和[审查修复计划及执行回执](../v6/V6_M1_WAKE_MODEL_REPAIR_PLAN.md)。

下面段落保留 S15 设备观察历史；S15 加载了与目标不同的模型，该 Candidate 结果不转移到 S17。

Candidate `claw4-learning-v6-m1-wnctl-s15-20260927-01`（source `7f7357da2e1299b150a17b979fb1a307d027441e`，R-09 构建复核 `PASS`）于 2026-09-27 app-only 写入 `ota_0`，独立 app readback SHA 与候选 app 一致，写入前后 live 分区表和 otadata 哈希不变。9/27 的 `boot-diag-01` 是显式 USB reset 后的 72.125 秒被动诊断；`voice-rearm-02` 捕获 88.328 秒后因 `SerialException` 中断。用户确认后者期间一次 Wake 尝试、没有观察到即时响应，问题未提问；不能精确关联任何语音帧。

2026-10-18 只读 `esptool verify-flash` 在 `0x200000` 核验 3,137,392 bytes，成功匹配 S15 app SHA，没有执行写入。随后的 60.015 秒被动启动捕获（显式 USB reset，不是冷启动）观察到 S15 ELF prefix、Wake enabled/active，未观察到 Wake event、crash 或外部端点；未请求语音刺激。后续只读核对确认 live 模型分区为 978,944 bytes，catalog 仅有 `wn9l_hai1tai4ling2`，而 S15 构建包含 `wn9_nihaoxiaozhi_tts`；两次启动解析均记录加载前者。状态 `MODEL_MISMATCH_CONFIRMED / REPAIR_AUTHORIZATION_PENDING`，S15 设备观察不能代表目标模型表现，且不定位其他 Voice/AEC 根因。模型分区尚未写入；用户授权和独立修复审查前，不进行目标词验收。Preflight `0/2–3`、正式 20 轮未开始，AEC 和 M1 均未验证。细节及私有证据哈希见 [S15 设备报告](../v6/V6_M1_WAKENET_CONTROL_DEVICE_REPORT.md)。S11 历史保留，候选结果不转移。M0 仍 `CHANGES_REQUIRED`，AP recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。

接管基线 `760b2aa66819f8d90186b43af4c02d9b33c8849e` 已保护至 `origin/takeover-v6-m0-c19-baseline`；R-01 在 `a164f11ab5f5843d6ace5c4b990d86a29195a2fe` 独立复审 PASS，Host 96/0。Candidate19 built, not flashed；Candidate20 已 app-only 刷写到 live `ota_0` 且读回 hash 一致。Candidate 构建/设备分区布局仍不匹配，恢复写回未执行。A-02 判定 `M0=CHANGES_REQUIRED`。

用户于 2026-09-24 明确要求跳过 Candidate20 AP outage/recovery 刺激并直接开始下一阶段开发。AP failure/recovery 保持 `SKIPPED_BY_USER / NOT_VERIFIED`，不再派发或重做。M0 仍为 `CHANGES_REQUIRED`；Candidate20 DEVICE 验证仅部分完成。M1-DEV-01 已 `ACCEPTED`（证据工具，不代表语音验收）；XiaoZhi NAS v0.9.6 已部署，7443 HTTP/OTA、7444 WebSocket、SenseVoiceSmall、Ollama `qwen3.5:2b` 与 EdgeTTS 合成连通性通过。M1 真机 Voice 和正式连续 20 轮均为 `NOT_VERIFIED`。

### 2026-09-27 S11 M1 AFE 输入诊断与后续用户辅助观察（最新设备记录）

Candidate `claw4-learning-v6-m1-afe-diag-s11-20260927-01`（source `cc1af698ed0d13066c71ae56d9365acb220683b7`，R-07 `PASS`）对 ESP32-P4 rev1.3 / MAC `80:f1:b2:d2:ed:14` 完成 app-only 写入及独立逐字节读回。显式 USB reset 后的 180.031 秒 UART 捕获解析出 167 个输入窗口、2,685,920 mic samples、I2S read failures 0；WakeNet enabled，检测数 0，crash marker 0。用户是否在该次采集期间讲话尚未确认，因此初始样本仍为被动/含义不确定的 AFE 输入诊断；初次观察到 NAS OTA 7443 一次、WS 7444 零次。

后续连续捕获补充了用户辅助观察：`voice-probe-02`（73.078 秒）记录 AFE detected=1、NAS WS 7444 和 `idle→connecting→listening→speaking` 及后续 speaking/listening 活动；用户先报无反应，之后报告有 Wake 响应，但数学问题无回答，等待超过 10 秒后出现新闻回退。后续 `post-restart-01`（112.860 秒）是另一段独立捕获，同样有 detected=1、WS 7444 及 `idle→connecting→listening→speaking`；重启方法未确认，不能称为 cold boot。两段捕获分别观察到 Wake/WS 活动，但这不证明两次完整响应或语音轮次。`voice-followup-03` 中有一次较晚的 listening→speaking，但用户确认数学问题发生在捕获结束后。`voice-question-04` 与 `rearm-05` 分别覆盖一次与两次用户确认的 Wake 尝试，用户均报告无响应；两个摘要均为 detected=0、无状态迁移/crash。用户报告与日志没有精确同步，故不能把话语、应答或回退绑定到特定日志行/帧，也不能声称完整语音轮次。Realtime Listening 会有意禁用 WakeNet；S12/S13 未证明确定性 lost-rearm 缺陷。S14 opt-in 诊断补丁的 R-08 review 为 `PASS`（Host diagnostics only）；补丁未构建、未设备验证，不构成 S14 Wake PASS。

综合记录不判一般性 WakeNet FAIL/PASS、AEC PASS 或 M1 PASS，也不计完整 Preflight round。Preflight 仍为 `0/2–3`，正式 20 轮未开始；M0 仍 `CHANGES_REQUIRED`，AP outage/recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。S08 用户辅助尝试保持历史，不转移 Candidate 或结果。原始 UART/语音和凭据留在私有目录，报告仅列摘要和哈希。详见 [S11 AFE diagnostic device report](../v6/V6_M1_AFE_DIAGNOSTIC_DEVICE_REPORT.md)。

L-11/L-12 仅完成设备证据文档和看板同步；不构成完整语音轮次、通用 WakeNet 结论或 M1 验收。

### 2026-09-27 S-08 M1 user-assisted Wake preflight attempt（历史状态；现由 S11 更新）

L-08 连续采集 Host 工具已完成（`c4600aa`、`d5326fa`；定向 9/9，V6 Host 138/138）。2026-09-27 首次真实串口冒烟检查因 COM7 未枚举而 `NOT_RUN / COM7_UNAVAILABLE`；之后 S-08 同一 Candidate 的连续捕获与用户辅助尝试见本节报告。L-08 工具和被动输入基线均不能代替语音证据。

S-08 的前一份 [被动设备报告](../v6/V6_M1_WAKE_DIAGNOSTIC_DEVICE_REPORT.md) 仅包含输入基线，不等于语音 Preflight。之后在同一 Candidate 上，采集前只读 `esptool verify-flash` 对 `0x200000` 处 3,136,144 字节核验 reviewed app SHA 成功；设备为 ESP32-P4 rev1.3，MAC `80:f1:b2:d2:ed:14`。一次显式 USB reset 后连续 UART 捕获 201.328 秒、63,958 bytes、1,141 个连续 index chunk；启动 `ota_0`，ELF prefix `fc07bcb12` 观察一次，NAS OTA HTTP 7443 观察一次，未观察外部端点、WS 7444、Wake 或 crash。解析到 188 个 M1 输入窗口、3,023,200 mic samples、RMS max 2507、peak max 21979、I2S read failures 0；heap/PSRAM unavailable。

用户报告第一轮尝试唤醒但设备无反应；原计划继续提问，但仅确认尝试唤醒，问题语句 `NOT_VERIFIED`。第二轮约 20–30cm，仅确认没有灯光/屏幕反应，声音反馈未单独确认。两次话语起始时间未知，不能绑定到具体声学帧；第二次前 30 秒摘要 RMS max 381、peak 4870 仅作上下文，不能证明语音识别。状态 `PREFLIGHT_BLOCKED_WAKE_NOT_OBSERVED / ROOT_CAUSE_UNRESOLVED`，不推断通用 WakeNet FAIL。完整 Wake→Speech→ASR→LLM→TTS→Playback→Next Wake 未完成，Preflight `0/2–3`，正式 20 轮从未开始。M0 仍 `CHANGES_REQUIRED`，AP recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。私有证据哈希、完整边界及报告见 [S-08 Preflight attempt report](../v6/V6_M1_WAKE_PREFLIGHT_ATTEMPT_REPORT.md) 与 [V6 唯一看板](../v6/V6_TASK_BOARD.md)。

> **SUPERSEDED for current M1 device status by 2026-09-27 S-08:** The following S-05 paragraph keeps its endpoint-mismatch and retry findings as history; its stated next step is no longer current.

L-03 根看板同步和 L-04 Preflight/正式 20 轮证据工具已完成。首个 S-05 对 R-02 Candidate 的 app-only 写入和读回通过，启动却实际连接外部 `api.tenclass.net` HTTPS/MQTT，语音刺激前停止、0 轮。S-04-ENDPOINT 随后固定 M1 预飞行 NAS OTA/WS 端点、拒绝外部 MQTT/WS 和自动升级，冻结新 Candidate `claw4-learning-v6-m1-preflight-endpoint-s04-r03-20260925-03`；独立 R-03 复审 `PASS`。S-05-RETRY 对新 Candidate app-only 写入/独立读回通过，设备实际连接 NAS OTA `192.168.3.100:7443` 且未观察到外部连接；WebSocket 懒连接需 Wake 才会建立。用户一次唤醒词报告落在两段日志捕获的时间空档附近，不能绑定刺激，WS 7444 与 Wake 均 `NOT_VERIFIED`，完整 Voice 仍 **0 轮**，设备已断电。下一步只用同一镜像做连续捕获的同步 Wake-only 端点探针，不重新构建或刷机；确认现场 WS 7444 后才可做 2～3 轮 Voice Preflight。正式 20 轮未开始，M0 继续 `CHANGES_REQUIRED`。M1 Voice Core 保持单一 owner，不提前加入 Learning UI 或 M1.5 七命令 Router。详见 [V6_TASK_BOARD](../v6/V6_TASK_BOARD.md)、[S-05 重试报告](../v6/V6_M1_VOICE_PREFLIGHT_RETRY_DEVICE_REPORT.md)和 [NAS 部署报告](../v6/V6_M1_NAS_DEPLOYMENT_REPORT.md)。

> **SUPERSEDED for current M1 device status by 2026-09-27 S-08:** The following 2026-09-26 paragraph preserves the R-03 historical evidence only.

2026-09-26 恢复状态覆盖上段 S-05 预飞行描述：同一 R-03 Candidate 未重建/重刷；连续 326 秒捕获覆盖用户一次唤醒词尝试和回复，串口未见 Wake、WS、外连或崩溃标记。语句声学时间/电平未测，不能据此判一般性 Wake FAIL；WS 7444 仍 `NOT_VERIFIED`，完整 Voice **0 轮**。COM7 已释放，断电确认待回。详见 [连续捕获报告](../v6/V6_M1_VOICE_PREFLIGHT_WS_VERIFY_REPORT.md)。正式 20 轮未开始，M0 仍 `CHANGES_REQUIRED`。

CODE/HOST/BUILD/DEVICE 分开记账；Candidate17/18/19 DEVICE 结果不得转给 Candidate20。Candidate20 现场情况与缺口见 [设备报告](../v6/V6_M0_CANDIDATE20_DEVICE_REPORT.md)及 [A-02 Gate](../v6/V6_M0_CANDIDATE20_GATE.md)。

V6 详细阶段状态见唯一看板：[V6_TASK_BOARD](../v6/V6_TASK_BOARD.md)。

以下未再次标注的旧看板内容均为历史快照（SUPERSEDED），其中遗留的“当前有效”、READY 或派发入口只保留历史证据，不构成现行队列。

## 2026-09-21 V6 覆盖状态（当前有效）

用户要求 Codex 亲自进行重要架构设计开发，WorkBuddy 可承担有界辅助任务并由 Codex 复核。当前活动 `CODEX-V6-FOUNDATION / IN_PROGRESS`；分支 `codex/v6-foundation`，工作区 `E:/workbuddy/claw4-v6`，来源 `5657ebed64ad5962c889fcf90f0979f5891ab862`。

唯一当前队列：[V6_TASK_BOARD](../v6/V6_TASK_BOARD.md)；[架构](../v6/V6_ARCHITECTURE.md)、[基线](../v6/V6_BASELINE.md)、[首批报告](../v6/V6_FOUNDATION_REPORT.md)。V5.3 后续扩展 HOLD；下方旧调度入口 SUPERSEDED，仅留历史，不构成当前 READY 授权。2026-09-22 已发布唯一 WorkBuddy 流 WB-V6-M0-CANDIDATE05-TEST（READY），任务包见 tasks/WB-V6-M0-CANDIDATE05-TEST.md。候选 05 已由 Codex 构建刷入，后续测试交 WorkBuddy；未宣称 M0 总 PASS。

## 2026-09-14 A05方案审查覆盖状态（当前有效）

用户最新要求：WorkBuddy实施和补测；Codex只负责规划、代码/证据审查、疑难定位，不直接代替实施。以下状态覆盖后文首批收口时的历史队列。

| 工作 | 当前状态 | 依据/下一步 |
| --- | --- | --- |
| WB-V53-NEXT-001 | REVIEW_READY，等待验收证据收口 | source `19fd979d4222093ff4ce7464e5b58407586594a2`；WorkBuddy29/29日志，本轮24/29实际运行，5项应用控制阻断；cleanup6 cases通过，非整流ACCEPTED |
| WB-A05-BUILD-001 CP0 | READY（当前唯一可领取checkpoint） | 只做Host证据与C5配置/依赖取证报告 |
| WB-A05-BUILD-001 CP1～CP4 | QUEUED，不得提前开始 | Codex记录前序ACCEPTED并认可CP0清单后放行 |
| A05-DEVICE | HOLD | 唯一候选BUILD审查+用户批次授权 |
| C01/C03及其后 | BACKLOG（本包不执行） | 另行编排；Host任务可不依赖设备连接，设备门禁不变 |

任务入口：[修订版A05任务书](tasks/WB-A05-BUILD-001.md) · [审查依据与实测](reports/CODEX_A05_PLAN_REVIEW_2026-09-14.md)。不能使用E:/c当前H2 sdkconfig生成C5候选；历史C5配置hash及cache来源见报告。未进行本轮IDF构建/Flash。

## 以下为首批收口时的历史快照（调度已由上节覆盖）

更新时间：2026-09-14。维护者：Codex 主 agent。当前唯一可领取工作流：**WB-V53-NEXT-001 / READY**。CODEX-V53-WAVE1（A01/A02/B01，Luna/medium）已完成源码/Host范围验收。

用户最新分工：当前子agent批次已收口；后续由WorkBuddy实施，Codex负责整体架构、代码审查、整合与疑难问题。历史验收不回溯修改。

> **9/14覆盖决定**：仅收口当前三个子agent任务；本批完成后，下一阶段由WorkBuddy实施、Codex审查，不再自动派发下一批子agent。下方角色列是原工作包专业分工，后续统一由WorkBuddy承担实施。

## 1. 基线与入口

- 最新设备开发来源：`origin/workbuddy-app-first-l3-acceptance` @ `7dd6511ab0125962d37f039955298819bdbb77be`（2026-09-13 11:43 +08:00；本轮远端复核一致）。
- 规划分支：`codex/v53-architecture-task-plan`；独立目录 `E:/workbuddy/claw4-v53-control-20260913`。后续agent从主agent给出的已审查SHA建立独立分支，不从旧工作区或main起步。
- 当前整合分支：`codex/v53-foundation-wave1`，从已推送规划提交 `1ae2a0c546615666a57abdf601b343bc88a043c4` 起步；三个实现工作区分别为同级 `claw4-v53-a01`、`claw4-v53-a02`、`claw4-v53-b01`，模型统一 `gpt-5.6-luna / medium`。
- `main` 仍为9/2旧基线，不代表当前设备开发。旧目录HEAD损坏，未修复/覆盖。
- [现状核验](reports/CODEX_V53_FACT_SYNC_2026-09-13.md) · [架构与路线](../ARCHITECTURE_V5_3.md) · [子agent工作包](tasks/CODEX-V53_AGENT_WORK_PACKAGES.md)。
- [旧看板](TASK_BOARD_PRE_V53_20260913.md)保留历史证据，SUPERSEDED仅指调度入口。原V5.3是参考输入，不是自动执行命令。

## 2. 当前事实

| 项目 | 状态 |
| --- | --- |
| L1 PersistFailed | 历史修复+限定设备持久化PASS；不重做排障 |
| L2/L3 | 历史报告6/6链路通过；开发HTTP relay，不等于生产TLS/整机稳定 |
| pause_count | 陈旧构建修正后，9/13报告追加真机验证通过 |
| D2 ResetToSeed | A02源码/Host修复已验收；本批真机验证未执行 |
| D8网络阻塞 | 未修：主循环同步I/O + runtime共享锁跨网络等待，A03优先 |
| 快照覆盖离线终态 | 源码高风险待定向复现，A04 |
| D5 double-free | 最新源码已修，修复候选真机验证未完成，A05 |
| Time/Reminder | B01 TimeAuthority Host已验收；设备校时与Reminder待后续 |
| L4 Voice | 实验功能链通，Voice Gate未过；NAS方案未验证 |
| 本轮Host | 20/20 suites PASS；42/42头、4/4源、1/1契约语法检查PASS；非设备build |

## 3. 调度队列

READY为可下发，QUEUED必须等待列出的依赖与主agent放行；同一共享文件任务串行。首批三个实现包已验收；下一包按WorkBuddy任务书顺序领取。审查包C0不计为产品Gate。

| ID | 任务 | 执行角色 | 状态 | 依赖/门禁 |
| --- | --- | --- | --- | --- |
| C0 | 远端事实、架构、看板、子agent包 | 主agent + 只读审计agents | ACCEPTED（规划范围） | 7dd6511；无硬件声明 |
| A01 | 可复现镜像/补丁/产物证据 | Luna实施+Codex修订 | ACCEPTED（工具/取证） | Base 1ae2a0c；实际镜像只读取证 |
| A02 | 正式任务Reset数据保护 | Luna实施+Codex修订 | ACCEPTED（源码/Host） | Base 1ae2a0c；Host+源码最小修复，无实际擦除 |
| B01 | TimeAuthority Host | Luna实施+Codex修订 | ACCEPTED（Host） | Base 1ae2a0c；独立目录，与A01/A02并行 |
| A03 | 网络I/O与状态所有权 | WorkBuddy + Codex | QUEUED | A02 + 主agent线程ADR审查 |
| A04 | 离线终态与快照合并 | WorkBuddy | QUEUED | A03；共享session/coordinator串行 |
| B02 | InteractionArbiter Host | WorkBuddy | QUEUED | WB CP3；CP2完成且接口冻结 |
| B03 | Reminder Host与恢复 | WorkBuddy | QUEUED | B01/B02接口 + A04合并契约 |
| A05-BUILD | M0候选冻结/已有D5回归 | WorkBuddy + Codex | QUEUED | A01–A04；精确patch与工具链可用 |
| A05-DEVICE | M0同一候选上机 | 主agent协调 | HOLD | A05-BUILD + 候选批次授权 |
| C01 | Backend TodayPlan/时间元数据 | WorkBuddy | QUEUED | A04/B01 schema审查 |
| C02 | PWA计划编辑 | WorkBuddy | QUEUED | C01契约冻结；最终对真实API集成 |
| C03-HOST/BUILD | Device Time/Reminder薄适配 | WorkBuddy | QUEUED | A01–A04/B01–B03/C01；限定文件与vendor边界 |
| C03-DEVICE | 本地提醒/离线/误差验收 | 主agent协调 | HOLD | M0设备Gate + C03构建 + 批次授权 |
| C04 | Focus/DailyReview/习惯闭环 | WorkBuddy | QUEUED | M1/M2；共享文件分提交串行 |
| D01 | 语音服务路线合成验证 | WorkBuddy | QUEUED | B02；先研究/fixture，不自动部署 |
| D02 | Voice/MCP/Level-2联合 | WorkBuddy | QUEUED | 本地提醒Gate + D01；真机部分需候选授权 |
| D03 | 家庭试用安全/全链/长稳 | WorkBuddy | BACKLOG | M1–M4；真实数据/部署另定范围 |
| E01 | ScreenOff/LightSleep/Wake | WorkBuddy | HOLD | M1稳定 + 新板级授权；不阻塞M1–M4 |
| E02 | 个性化/复杂重复规则 | 后续 | BACKLOG | 客观数据可信、核心MVP稳定 |

当前派发入口：[WB-V53-NEXT-001](tasks/WB-V53-NEXT-001.md)。CP0 READY；CP1～CP5 QUEUED，顺序推进，Codex按提交审查；不再派发新子agent。冻结代码SHA `e70c2830d1f7ea43629a61ce01a33f78b6f11128`；完整[首批收口报告](reports/CODEX_V53_WAVE1_CLOSEOUT_2026-09-14.md)。其它队列行仅表示后续路线，不能越过本任务书自动领取。

## 4. 旧任务承接

| 旧项 | 本轮处置 |
| --- | --- |
| WB-LEARNING-V4-HOST / L1c | 已有完成证据保留，旧PersistFailed由修复报告取代 |
| CODEX-APP-FIRST-001/002 | 历史Host/L3证据保留；新调度入口停用，缺口承接A01–A05/C01/C03/D03，不整体补盖ACCEPTED |
| WB-APP-FIRST-L3-ACCEPTANCE | 报告6/6保留；D2/D8及构建缺口承接A01–A05 |
| WB-LEARNING-L4 | 新功能扩张暂停；已有修复源码保留，稳定性与方案承接A05/B02/D01/D02 |
| 原V5.3 | 产品方向采纳；具体顺序/契约由优化架构与工作包覆盖，原文只读参考 |

这是本规划分支的调度约定，不表示已远程停止另一个工具中正在运行的进程。后续开始实现前主agent重新fetch；若源分支推进，先核对新提交与占用路径。

## 5. 硬门禁与验收分层

- 不改partition/bootloader/ota_1/C5/eFuse/SecureBoot/FlashEncryption，不擦NVS，不以清Backend事件表修复D2；srmodels刷写也不是ota_0授权的一部分。
- vendor保持只读；新的修改必须精确任务白名单与适用授权，完整patch登记，禁止推官方origin。
- 每个候选分别记录CODE/ HOST/ BUILD/ DEVICE证据；旧报告PASS不自动适用于新固件。
- 首次实际家庭试用前需D03（身份、TLS、数据保留等）；开发HTTP relay和单家庭stub不能直接发布。
- 子agent只交REVIEW_READY，主agent负责技术审查；用户保有硬件/发布/范围最终决策。
