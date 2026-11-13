# V6 唯一阶段看板

## 2026-11-13 S-32 对话打断与继续

主对话页在播报或聆听时显示 `Interrupt`。按下后停止当前回复、清理待播音频并结束当前会话。按 `Continue` 建立语音会话并直接开始聆听。操作者确认真机打断后停住，继续后能正常回答。

S-32 候选 `claw4-learning-v6-timer-preview-s32-20261112-01`，源码 `56c6e1adcdbff83e3858398d1318169f9523ff14`，manifest SHA-256 `bf9be78d186d08a239a05631de0e640ef7aaa7fc0d309fc8b7e4fe516c1612d7`。244 项工具测试和完整构建通过。仅写 `ota_0` 应用，完整读回 SHA-256 `0adfee74dbd2093282177363a1c15a76d72cece666a2e6209c5fc20299a4cdd0`，保护区读回一致。

160.046 秒启动采集核对了候选身份、界面就绪、校时及待机，崩溃标记为 0。采集窗口未记录按键或对话事件，本次交互结果来自操作者确认。安全摘要 SHA-256 `8cd6bbe573baad8a609f34c65c3cd7385d2cd2c63999ce6776068a67d42eff3a`。正式 M1 语音预检及回环验证另行记录。

## 2026-11-12 L-43 实时新闻问法修复

操作者报告无法查询实时新闻。NAS 当时可访问中新网 RSS，已核对有北京时间当天标题。真机识别得到「给我看看今天的新闻」，旧规则未匹配，交给普通问答。修正后该问法及「实时新闻」「请联网查一下实时新闻」等完整短命令路由至固定 RSS。天气与普通问答继续通过既有路径。

源码 `bd9453a8d8e1d3971a42be38c1c2b83aa9820f55`，正式镜像 `sha256:1f997e2003988c0913ddc4b0df4f93b5b0f1be9bb62a12ff766651710cf494f3`。本地 244 项工具测试通过，隔离与正式服务各通过 10 项 WebSocket 回归与 5 项回采接线检查。两种新闻问法都返回当天标题和可解码音频。OTA HTTP 200，发布预期 WebSocket 端点。正式安全证据 `evidence/news-routing-20261112-01/active-ws.json` SHA-256 `0470521b23dcf5eb1012ec87b33c5fde1cf51dc7622d328049a01f05f7794c5b4`。真机复述待核对，M1 验收状态不变。

## 2026-11-12 S-31 主对话界面与播报回环

主界面增加圆角状态栏、表情卡片和对话卡片，以淡紫与薄荷色区分用户和小智。状态、待机时间和对话反馈放大，保留现有中文字形和长回复滚动。预览语音改用轮流说话：播报时停止拾音与唤醒，输出播放队列排空后恢复聆听。当前预览不能在播报中插话。

S-31 候选 `claw4-learning-v6-timer-preview-s31-20261112-01`，manifest SHA-256 `bea61604cd95c2dfa7f8aa6ecb8a1cae6cc203af10a66b7daf3a1cee02742259`，应用 SHA-256 `c47bd3a196eb182dc1cfea331b48d8541dda76f56d85a63d3e062b958519bf3f`。工具测试 244/244，ESP-IDF 构建通过。

应用已写入并完整读回，分区表、启动选择区和唤醒模型前缀读回一致。启动 150.015 秒采集核对了 ELF、界面就绪、校时和待机，崩溃标记为 0，窗口内无唤醒或对话。安全摘要 SHA-256 `af32546b7b55a0e232a93efae743acc480a160b66a7a3b20ddeef588077a9f8e`。等待操作者验证新界面和两次连续提问，语音回环及 M1 预检保持待验证。

## 2026-11-12 S-30 计时面板与番茄钟复测

计时预览面板新增大字号倒计时、进度条和清晰的专注、一次性闹钟分区。专注区有 1、5、25 分钟预设，暂停按钮随状态切换为继续。响铃时面板自动打开，底部显示全宽红色 `Stop Alarm`。

S-30 候选 `claw4-learning-v6-timer-preview-s30-20261112-01`，源码 `b72ef6c3a8b04c4924ae4742341207ecf234e04e`，manifest SHA-256 `4a3f761ae01b9020ba5a3adb64eee97c1650fda0dd721f4e85be811a512ed2bf`。工具测试 244/244，ESP-IDF 构建通过。仅写 `ota_0` 应用，完整读回 SHA-256 `9e8c64b470e8295566d37114785cc06b709519d5904400b21b5a6b160ebeebe9`，保护区读回一致。启动采集核对了候选 ELF、计时界面、校时及待机。

真机用 1 min 预设从开始到到点 60.386 秒，确认前 6 次播放请求，确认后 33.562 秒内为 0，随后成功识别唤醒。操作者确认三步正常，并报告普通语音会话仍出现回环。安全摘要 SHA-256 `6a51426eb9468f897532630dafe5c53cefd5d85fdfe9fc58609f456fffb465a7`。普通对话回环和正式 M1 预检仍待闭合。

## 2026-11-12 S-29 到点重复响铃与确认

操作者补充 S-28 曾听到一声短提示，要求改为重复响铃并按键确认。番茄钟和一次性闹钟到点会打开计时面板，重复播放两声一组的铃声，直到点红色 `Stop Alarm`。如有当前语音会话，响铃会先结束该会话；最后一段本地音频播完后恢复唤醒。一次性闹钟在确认后记录为已处理，保存失败时自动重试。

S-29 候选 `claw4-learning-v6-timer-preview-s29-20261112-02`，源码 `5e0b2437e268d18ddf81c18ca20442b281e3655a`，manifest SHA-256 `db7aae0d631e166044d09dd42e1ef5ea5b8206ac4921776699c102b085415049`。工具测试 244/244，板级 C++17 严格警告测试通过，ESP-IDF 构建通过。仅写 `ota_0` 应用，完整读回 SHA-256 `327b9ff9b5a2cb936c1de2e83527d1dc57dddc7b6f47ee3a82fb356ed056a8b2`，保护区前后读回一致。启动日志核对了 ELF、计时界面和校时，并观察到待机。

真机通过触控 Alarm +1m 复测。操作者确认响铃和按键停止正常。设备记录确认前 9 次播放请求，确认后 29.9 秒内为 0，崩溃标记为 0。安全摘要 SHA-256 `220701d1d363bfc44a8123ce612061c0bba4280be4c087482fec2e44a85bacc8`。番茄钟到点、在语音会话中抢占和确认后的重新唤醒保持待专项真机验证。

## 2026-11-12 S-28 计时提示音与语音回采修复

S-27 真机交互：语音指令已下发，设备因已有计时而拒绝启动；操作者取消旧计时后，触控 5 分钟可以倒数，结束后未听到提示音。同一交互窗口观察到一次 NAS 归一化 ASR/TTS 文本完全匹配，间隔 2.293 秒，结合操作者报告记录为回采观察。旧提示音在语音忙时有超时丢弃路径；当前记录未绑定确切到期时刻，不能认定该路径就是本次无声的原因。

- NAS：先修正已有计时的提示（有有效数据时给出剩余时间，建议手动取消，保留旧计时），再上线本次回采防护。仅在同会话、当前语句关联、确实发送 Opus 包、采集时段与近期发送重叠、文本完全匹配时拦截；其他内容继续处理。集成源码 `f7b96e97aad6a3923b6e4830d74f0d06fc653644`，正式镜像 `sha256:e054664a6c19393aa0a9b4a4e592396ea73a3e7bd992b9db403866ef8ea1a25e`，重启计数 0，配置 SHA 不变，OTA HTTP 200 且公布预期 WebSocket。隔离及正式服务各通过 5/5 模拟 ASR 接线用例和 8/8 合成 WebSocket 用例。安全证据 `evidence/timer-echo-20261108-01/`：`active-echo.json` SHA-256 `49d7df4f53c4a8aa988a1d643a68d5a8d11bb61c7122fce6d3ee7b6752b85758`，`active-ws.json` SHA-256 `b8b10a60cd1114b6b541e5e97796d1075cc99105ad6cd75fca253f7a9a1e7bd0`。发送记录不证明设备扬声器实际播放，服务防护也不等同于物理 AEC 验收。
- 固件：语音或播放未结束时，番茄钟和闹钟提示分别最多延后 180/300 秒；记录等待、发起播放和过期，避免同时发起两种提示。主机 C++17 严格警告测试通过；工具测试 244/244。候选 `claw4-learning-v6-timer-preview-s28-20261108-01` 从相同集成源码构建，manifest SHA-256 `089696c637df2d6311074385eebf81859df6065a787d76bc19f8f7c037928e7c`，应用 SHA-256 `9de54fa7b0810e58b1db31d4999fcc6898dc849de49869c3bd6cbda50ea61548`，ELF SHA-256 `f0e163e4578d771a0b7d1e2aed1e3352c5ef14fd12d63d5c6b5ea1bced43fbb6`。提示音日志标记已在成品中核对。首次构建完成时 COM7 未枚举。设备测试依次覆盖待机到点响铃、语音结束后的延迟提示、安静播报期间无自行接话。

2026-11-12 Claw4 重新连接后，S-28 应用写入 `ota_0`，完整读回 SHA-256 与上面的应用一致。已校验原应用回退备份，实机分区表、启动选择区和唤醒模型前缀的前后读回一致。写入记录 SHA-256 `8bd9680764b7c79c8defd574c2706712b05f9dcd9597c18142b58dd88e31d4d9`。USB 重启后观察到匹配的 ELF 标识、计时界面就绪、校时接受和待机。

操作者用语音启动一分钟番茄钟，确认屏幕倒数，到 `00:00` 未听到提示，并报告设备持续自行接话。真机在语音未结束时记录 `FOCUS_CUE deferred`，约 149 秒后回到待机并记录 `FOCUS_CUE queued`。随后完整音频采样窗口记录了 15,104 个 TX 样本，操作者未听到补播。实机提示音听感 `CHANGES_REQUIRED`。NAS 同窗记录 18 条 ASR，其中 3 条较长识别是近期播报文本的子串，完全匹配防护未触发；子串关联是排查线索。语音回环实机复测 `CHANGES_REQUIRED`。启动至交互连续记录 336.204 秒，安全摘要 SHA-256 `8dcb246bf0bdff8658bab29f112e8cdd789d98dbbd284be0af48c3a708968e6b`。

AEC 保持 `NOT_VERIFIED`。M0 `CHANGES_REQUIRED`，AP `SKIPPED_BY_USER / NOT_VERIFIED`，M1 Preflight 未通过，正式 20 轮 `NOT_STARTED`。

## 2026-11-08 S-27 闹钟与番茄钟真机预览

用户批准在 M1 验收前进行此项预览。S-27 独立应用提供屏幕 Timer / Alarm 入口、5/25 分钟触控预设、语音指定时长、暂停/继续/取消、一次性闹钟及到点提示。计时使用单调时钟，闹钟使用设备接收的 NAS UTC 校时和北京时间，预览最多保存一个待响闹钟。语音确认使用当前设备回执。

设备候选 `claw4-learning-v6-timer-preview-s27-20261108-01`，源码 `9e90e43d6c9bea880cdf0c9a359a8634ece06385`，manifest SHA-256 `c630f07bbc5a1c76da2326a4110ea4b704ce3ee4a5a8fb3a67536269d3bc93f8`。应用仅写 `ota_0`，完整读回 SHA-256 `52c3e678a4025497eadaf1401f3bc0fd297d15afbe625b6b11ba8b1689848448`，回退备份已校验，分区表、启动选择区和唤醒模型前缀保持一致。USB reset 后连续采集 240.077 秒，观察到唯一 ELF 锚点、计时界面初始化、校时接受和设备待机，崩溃标记为 0。启动摘要 SHA-256 `6908cf714ed70f54a492e3549004611b6e3eb57dfe676c8ca5cac7c567176c11`。真机触控、语音计时和到点播报待操作者验证。

NAS 源码 `2ade5879e1c27cf5a971fd79d1b848acc321618d`，正式镜像 `sha256:16f4ac8cd91fb014a89b0e5e4e876849ddd29da1bb80b7bec34907e7337d5314`。隔离及正式服务各通过 6/6 合成 WebSocket 用例，覆盖模拟设备回执、未校时、不支持工具、天气与普通问答。工具回归 235/235，Board C++17 严格警告构建及测试通过。预览证据独立记录，M0 `CHANGES_REQUIRED`，M1 正式 20 轮 `NOT_STARTED`。

### 语音启动复测与 NAS 同音容错

操作者报告语音启动后未开始倒数。本轮实际识别文本为「开始一分钟番茄中」，原规则将它送入普通问答，没有收到设备启动确认。修复 `c9dea009afc0ca680b3a70348823082688d44462` 只在完整计时命令中识别「番茄中」。主机测试 236/236；隔离和正式 NAS 各通过 7/7，包含同一识别文本下发 60 秒计时、收到模拟设备回执、普通问答和天气。正式镜像 `sha256:62e1e8c518fcad680dc6c890351f73aaa0931039739f2f4e4169310f29261ba6`，正式安全证据 SHA-256 `70bd1cff7aca8359d86d712f972cbadd1beafce72aa19b650d3851a48541ec75`。修复后的真机倒计时和到点提示待复测。

## 2026-11-08 L-40 闹钟与专注倒计时（HOST_VERIFIED）

用户要求增加闹钟和番茄钟。主机侧复用 Focus 的单调时钟剩余时间，输出 `MM:SS`，暂停时稳定、到期显示 `00:00`。首版倒计时绑定学习任务。一次性手动闹钟通过既有提醒存储设定和取消，只有时钟可信且保存成功后才返回成功。显式设定的闹钟按设定时间评估，学习提醒继续遵循免打扰策略。断电重启后的主机仿真验证了记录恢复、未校时抑制和重新校时后的触发。真机屏幕、语音命令、RTC、存储适配及提示音待接入；M2/M2.5 阶段门禁待放行。

主机源码 `309c19efb0b9234e8de1243457b1ab9395e49371`，闹钟提交 `3a5be2a`。C++17 `-Wall -Wextra -Werror` 构建通过，V6 CTest 6/6（164 用例），工具测试 223/223。

## 2026-11-08 L-39 NAS 实时天气与新闻（NAS_SERVICE_VERIFIED）

用户授权增加实时天气和新闻。服务在既有语音会话中处理常见城市的当前、今天及明天天气，并播报中新网当日最新至多三条标题。天气请求仅使用预设城市坐标，新闻使用固定 RSS 请求。缺少支持的城市或数据源失败时有明确提示。集成源码 `fafc8ce9aee3410e02b9f0cd41a00c69e9414547`，本地 V6 Host `223/223`。隔离及正式 NAS 各通过 `6/6` 合成 WebSocket 文本到 Opus 用例，覆盖当前天气、今天、明天、新闻、城市提示、普通算术。正式镜像 `sha256:06d26d826107edc2e4ecd1a0bf07367b354c66d81328e9836438f650748e370c`，容器 `aa6bd98417a4d39449c10af8667fa335c51764f3d062f67b47b25a1b37682a5c`，restart 0，配置 SHA `e56859eb205c6124c66cc30275d643556c9666e66ec2ceaa1ae4f4f55f344911`，OTA HTTP 200 / WS 端点核对通过。正式测试证据 `evidence/live-lookup-20261108-01/active-ws.json` SHA-256 `4c5ec42f96fd83909d285165cce263bd48e2b619505080aa71fd5adbdca49660`。真机 ASR、扬声器和设备轮次仍待验证。M0 `CHANGES_REQUIRED`，AP `SKIPPED_BY_USER / NOT_VERIFIED`，Preflight `0/2–3`，正式 20 轮 `NOT_STARTED`。 [部署与回退](../../deploy/v6/xiaozhi-nas/README.md#live-weather-and-news)。

## 2026-11-08 L-38 NAS synthetic voice-flow evidence

Root's isolated synthetic WebSocket/provider observations and S-26 static source audit are documented in [L-38 report](V6_M1_NAS_VOICE_FLOW_REPORT.md). `listen.detect` bypasses ASR but still enters same-session Dialogue/Ollama/TTS; TTS FIRST is a control marker, not actual Opus arrival. The first fresh-blue LLM dialogue lacked a system message and returned 200 characters before client timeout; other six initial cases had the expected prompt. This supports a first-turn prompt-readiness hypothesis only. Count segmentation split the first `1`; slow second Edge TTS took 15,785 ms with three failed attempts, while fast second took 2,576 ms without retries. S-25 exact-text/hash reclassification found zero outer retries on both historical segments, so the isolated retries do not explain its 11.989643 s interval. Initial literal-suffix blue exactness was 0/3 for baseline and candidate; directed V2 was 2/3 (fresh and after-theme passed, after-count failed), with violet/explanation/arithmetic controls passing. Safe summary SHA-256 `58a20b705db69b34fb2780e0bad9cb9d25c97c3b752d97e0101f0d0182b04179`. No live production change, device test, AEC claim, or latency claim. S-24 DEVICE remains as S-25 records; M0 `CHANGES_REQUIRED`, AP `SKIPPED_BY_USER / NOT_VERIFIED`, Preflight `0/2–3`, formal 20 `NOT_STARTED`.

## 2026-11-08 S-25 S-24 真机回声诊断

S-24 已 app-only 写入 `ota_0`，完整读回与候选一致。实际分区表、启动选择区及目标模型镜像范围读回一致，写前完整扇区备份已验证。连续采集 371.705 秒，确认 ELF、NAS 端点及 357 个配对音频窗口、351 个 AFE 快照。安静数数首两段 NAS 发送标记相隔 11.990 秒。插话复测精确识别受控短句且 ASR/LLM 哈希匹配，操作者报告答复偏离指令。观察到参考信号可用及一次回复期间 Write 内短缺，物理回采根因未验证。L-37 证据工具定向 7/7、V6 Host 209/209。Preflight `CHANGES_REQUIRED`，仍计 `0/2–3`；正式 20 轮未开始，M0 `CHANGES_REQUIRED`，AP `SKIPPED_BY_USER / NOT_VERIFIED`。见 [S-25 设备报告](V6_M1_AEC_DIAGNOSTIC_DEVICE_REPORT.md)。下步在服务侧分别复现答偏和分段停顿。

## 2026-11-08 L-36 S-24 AEC diagnostic candidate evidence (DOCS_COMPLETE / BUILD_REVIEW_PASS)

Docs-only evidence record is in [L-36 report](V6_M1_AEC_DIAGNOSTIC_REPORT.md). Candidate `claw4-learning-v6-m1-aec-diag-s24-20261108-01`, source `5a9dbfcf66ad6d8c505a8d3be8126e8362d1fb03`, manifest SHA-256 `8c082e7199e81561d93d648e17abe6e0f9d76aa0043282bddb3e51b5d7f45fbe`; build `BUILT_NOT_FLASHED`, independent R-18 `PASS`. Device validation `NOT_VERIFIED`; no Flash authorization is implied. Bootloader output differs from S15 and is excluded from any permitted write payload. L-34 remains `PREFLIGHT_CHANGES_REQUIRED / TTS_RECAPTURE_OBSERVED`, full Preflight `0/2–3`, formal 20 `NOT_STARTED`; M0 `CHANGES_REQUIRED`; AP `SKIPPED_BY_USER / NOT_VERIFIED`. Diagnostic counters do not establish acoustic delay, AFE feed, AEC effectiveness, or near-end performance.

## 2026-11-08 L-34 S-17 Voice Preflight 05

Single 427.176 s continuous capture with explicit USB reset (not cold boot); no build, Flash, or app readback. ELF prefix `33bcb2d01`, target model and NAS OTA/WS observed. NAS remains L-33 prewarm image/config/container, restart 0, HTTP 403/traceback 0. Three exercise scenarios were observed but none qualifies as a complete round: R1 operator reported a complete audible answer and ~1 s wait, NAS ASR→first-audio marker 1411.991 ms, no extra ASR; exact R1 prompt not established. R2 controlled near-end marker was reported once with no other operator speech; ASR/LLM/audible response confirmed, plus two ASR→LLM forwards matching normalized substrings of preceding TTS during/at the playback boundary. Strong recapture/self-reply observation in this interruption exercise; physical root cause and general AEC/near-end remain `NOT_VERIFIED`. R3 idle→next Wake in the same capture had a normal count and no reported self-response; no extra ASR through capture end. A delayed scheduled silence-end prompt matched its known NAS hash and was natural idle, not ASR. Parser Wake count 0 reflects its literal-only allowlist; private safe timeline recognizes staged Wake string forms, AFE detected total 2; no parser/product change. Crash 0, I2S read failures 0, heap/PSRAM unknown, no measured true speaker onset. Current Preflight `PREFLIGHT_CHANGES_REQUIRED / TTS_RECAPTURE_OBSERVED`, credited full rounds `0/2–3`; formal 20 `NOT_STARTED`; M0 `CHANGES_REQUIRED`; AP `SKIPPED_BY_USER / NOT_VERIFIED`. No speculative fix or new retest requested. S-23 read-only audit: AFE Wake/audio processor/device AEC enabled; speaking keeps Voice processing while listening. TX reference is post-volume after I2S write returns; RX consumes bounded FIFO, underflow may feed zeros, delay 0 uncalibrated. Prior 0/90 ms comparison did not prove benefit. Root cause unproven. Any future diagnostic should first collect aligned aggregate TX acceptance/reference-active/underflow/queue occupancy, mic/reference envelopes and AEC state, then playback-only vs one near-end contrast on the same candidate; no blind gain/delay, server muting/uplink gating, or implied Flash authorization. Evidence and limitations: [L-34 report](V6_M1_VOICE_PREFLIGHT05_REPORT.md); safe summary SHA-256 `923f90097c92398d19059778d29ba7413c78e834f448d0ff46c7a50c6283ae76`.

## 2026-11-08 L-33 Ollama prewarm service evidence

A-07 bounded internal connection-scoped preload candidate `claw4-nas-prewarm-20261106-01`: source `d587f415e29fc3385eebd1858efce8847a5cba60`, S-22 fix `f95ce4679e8842cf21b68353ed81a99e2e8b0dd0`; R-16 `PASS`, targeted Host `9/9`, integrated V6 Host `199/199`. Image built 2026-11-06. Isolated synthetic WS text-to-server-audio `2/2`; first case model absent before connect and at hello (7 ms), observed resident before question (8472 ms from connect); question-to-first-Opus `2173/1513 ms`. Root rollout completed 2026-11-08 Asia/Shanghai: active image `sha256:97c36ec5638a97d47756e906e0c449926ebf4192e701c6bece0ce7912c99d1c1`, container `6f7389a8a62b3215b48404753fc66278712b1e4c7a2f8778109f47f08b38cc20`, restart 0, config `e56859eb205c6124c66cc30275d643556c9666e66ec2ceaa1ae4f4f55f344911` unchanged. Local HTTP 200/expected WS. Active synthetic WS `2/2`, expected answer/TTS stop, 0 HTTP 403/decode errors; question-to-first-Opus `1932/1835 ms`. Compose delta image/build only; old image retained as rollback target, no rollback. First owner guard attempt stopped before mutation due to NAS Python 3.10 lacking `hashlib.file_digest`; the evidence script was corrected to streaming hash before successful retry with the same reviewed image/tests. No helper INFO marker was visible; no helper-load-duration or confirmed resident-branch-log claim. This is synthetic service-path evidence, not controlled cold/warm end-to-end A/B, device turn, ASR, or playback validation. L-25 is historical for its prior runtime; L-27 prior device evidence does not transfer. M0 `CHANGES_REQUIRED`; AP `SKIPPED_BY_USER / NOT_VERIFIED`; Preflight `0/2–3`; formal 20 `NOT_STARTED`; M1 not accepted; no M1.5/UI work. Full provenance and artifact SHA-256 values: [L-33 report](V6_M1_OLLAMA_PREWARM_REPORT.md).


## 2026-11-06 L-32 NAS synthetic stage profile（组件证据，不是设备轮次）

Root 对实际 Ollama provider 与 EdgeTTS stream/Opus decode wrappers 以固定合成输入完成三次隔离串行组件试验：LLM first yield `6798.94/209.69/185.12 ms`，LLM complete `6969.56/380.74/354.53 ms`，TTS first MP3 chunk `1266.23/1201.92/1622.91 ms`，TTS request `1652.47/2586.35/2157.92 ms`，first Opus frame `1856.11/2683.83/2257.40 ms`；三次均匹配预期合成关系，均解码 33 frames/31,680 samples，decode error 0。它串行等待完整 LLM 后再跑 TTS，不是 live device turn、跨服务耗时或 streaming overlap 测量，也不是受控冷/热模型对比；模型试验前已 resident，未强制 unload 或改 keep-alive。另一次模型自然过期后的 native 合成请求显示 `resident=false`，耗时 5577 ms（load 4962.079 ms、prompt evaluation 425.016 ms、generation 186.803 ms）；仅证明该请求由模型加载主导，不解释 S-17 的 8.325 秒 marker interval 或设备时延。在 L-32 service snapshot 中 NAS image/config/container identity 与 restart 0 未变化；这不是当前服务身份。在 L-32 service snapshot 中，R-15 对集成 `8892ae63cbfdf096088f3f7b9b59a304bc71c565` 独立 `PASS`；当时完整 V6 integration Host suite `190/190, 0 fail/error/skip`，NAS identity 当时未变化。后续当前服务见 L-33。详见 [L-32 stage profile report](V6_M1_STAGE_PROFILE_REPORT.md)。这份组件证据不补足真实轮次 ASR/LLM/TTS/device first-audio、heap/PSRAM、AEC/near-end 或绑定 next Wake 指标；L-27 正面可听响应保留，Preflight 仍 `0/2–3`，正式 20 轮未开始，M0 `CHANGES_REQUIRED`，AP recovery `SKIPPED_BY_USER / NOT_VERIFIED`。S-22 的 A-07 实现经 R-16 `PASS`，Root 完成 NAS synthetic service rollout；当前范围与限制见 L-33。

## 2026-11-06 L-27 S-17 Voice Preflight 04（当前设备证据）

300 秒连续采集，配置上限正常结束；显式 USB reset（非 cold boot），无构建或 Flash。单一有效启动 ELF anchor 匹配 `33bcb2d01`，目标模型、NAS OTA/WS 均观察到。用户确认立即 Wake 并听到正确的受控答案；normalized ASR marker 匹配受控问题，expected-answer TTS marker 观察到。ASR→TTS marker 间隔 8.325 秒，不等于扬声器实音延迟。该窗口观察到一次 ASR、无额外 ASR；不构成 AEC PASS。后段 NAS `LLM_INPUT` hash 与配置结束提示精确匹配，符合 120 秒 silence timeout 的 scheduled close/farewell 路径，不是第二个 ASR。用户另报告在采集边界第二次立即 Wake，但 UART 未绑定第二个 Wake：记 `USER_REPORTED_NEXT_WAKE / UART_NOT_VERIFIED_CAPTURE_BOUNDARY`，保留正面用户报告，不判成功绑定或失败。286 个输入窗口、RMS max 2615、I2S errors 0、crash 0；heap/PSRAM unknown。完整 gate 仍保守计 `0/2–3`（AEC、near-end、完整 next-Wake binding 未闭合），正式 20 轮未开始。M0 `CHANGES_REQUIRED`，AP outage/recovery `SKIPPED_BY_USER / NOT_VERIFIED`。详见 [L-27 Preflight 04 报告](V6_M1_VOICE_PREFLIGHT04_REPORT.md)。L-25 NAS synthetic service verification 是其旧 runtime 的有效历史证据；当前 service 状态见 L-33，且仍与设备证据分账。

## 2026-11-06 L-25 NAS ASR/prompt repair（NAS 服务证据；设备状态已由 L-27 更新）

L-25 source `150fe555a83105ad10852865ec363404f24d9a78`（S-20 `f436a8f` + L-24 `b58e958`）普通 push 并核对远端 SHA；S-20 定向 Host `10/10`、完整 V6 Host `175/175`。R-13 对 L-24 README rollback-guard 修复复审 `PASS`（source `ce19ee77f037c2973c5c835779d2926f81e1a34d`）。L-26 路由修复普通 `extra_hosts` 合并首次被 root guard fail-closed 拒绝，未改 live config/service；随后 Compose `!override` retry 经 R-14 `PASS`（集成 source `426f187c4ec9c916c09e4edebfbdea919ccce7b8`）。生产与回退 overlay 均解析为唯一 NAS host 映射。Root sole-owner 部署无重建、无刷机：image `9cd9…`、config `e568…`、container `5af9aa…`、restart 0、未执行 rollback；manifest exact-byte SHA256 `0b61eca72f33ab98d482dafc1d1f337c0a5a0bf908baf4e0219e1c9007ce7d16`。post-deployment source/Ollama/prompt/model identity checks 与 OTA HTTP 200 / 预期 WS endpoint 通过。两个独立 synthetic WebSocket text cases 均匹配预期答案并收到 TTS stop；binary 4,377/4,622 B，解码 31,680/32,640 samples @16 kHz，decode errors 0，first audio 8,494/2,086 ms，总时长 10,543/4,190 ms。仅 `NAS_SERVICE_VERIFIED` synthetic service path：WS text probe bypasses ASR producer；prompt/cache检查在新 Python process，不是 live server memory。**L-25 服务快照时的设备状态**（后续实测见 L-27）：Preflight `0/2–3`，正式 20 轮未开始，AEC/near-end/next Wake `NOT_VERIFIED`；无设备测试、Flash 或 rebuild。M0 `CHANGES_REQUIRED`，AP outage/recovery `SKIPPED_BY_USER / NOT_VERIFIED`。见 [L-25 报告](V6_M1_ASR_PROMPT_REPAIR_REPORT.md)。

## 2026-10-20 L-23 S-17 Preflight 03（历史快照；后续服务与设备状态见 L-25/L-27）

> **SUPERSEDED current status:** L-23 remains historical device evidence. Current NAS service evidence is L-25; current device observations are L-27.

> **SUPERSEDED 历史快照（2026-09-23）**：Candidate19 已构建但未刷机；Candidate17 DEVICE 观察仅属于 Candidate17。Candidate19 没有 DEVICE PASS。旧工具测试统计按发生时间分别为 C19 构建时 68 项、接管基线 83 项（增加 15 项）；均为 Host 工具测试，不是设备验收。AEC NOT PASS；旧 `clipped=0` 计数无效。

## 2026-10-20 L-23 S-17 Preflight 03（历史快照；后续服务与设备状态见 L-25/L-27）

> **SUPERSEDED current status:** L-23 remains historical device evidence. Current NAS service evidence is L-25; current device observations are L-27.

L-20 provider 修复后的 `preflight-03` 使用同一 S-17 app/model 与已核验 L-20 NAS image/config；未刷写设备。438.889 秒 UART capture 按 stop file 正常结束、无 serial error，COM7 已释放。用户确认 Wake 并听到包含“1/2”的回答，主观延迟约 5 秒，但正确性不明；ASR 286,062 ms 与 302,063 ms 在中英数字、全半角归一化后匹配受控问题，严格正则未命中是 parser 盲点，不是 ASR 错误。两组 TTS text marker 摘要 hash 相同；时间线 marker 不等于实际音频开始播放或回答正确。用户之后确认还重复提问并说过其他话，因此额外 ASR 不归因于回声。最终观测状态迁移 `speaking→listening`；没有 idle 恢复或 next Wake 证据。NAS runtime 本窗口 HTTP 403 为 0，image/config 与 restart state 未变。状态为 `WAKE_OBSERVED / AUDIBLE_RESPONSE_UNCLEAR / PREFLIGHT_CHANGES_REQUIRED`，Preflight completed `0/2–3`，正式 20 轮未开始；AEC、near-end、next Wake 仍 `NOT_VERIFIED`。NAS provider service verification 保留，但不等同 device Voice PASS。S-19 Sol 的动态 prompt→分段/清洗审计进行中；Root 的 flat system/user synthetic probe 不代表实际连接路径，当前根因未定。M0 仍 `CHANGES_REQUIRED`，AP outage/recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。证据见 [S-17 Preflight 03 report](V6_M1_VOICE_PREFLIGHT03_REPORT.md)。

## 2026-10-20 L-21 NAS Voice provider repair（历史服务验证；设备状态已由 L-23 更新）

L-20 source `2e9be855f3382db7f8182adfd197e94f88583e95` 已整合至 `531ac23e8ed8608179ad83d42d163c0b8f40efa5`；R-12 独立复核 `PASS`（仅 CODE/HOST/readiness）。worker Host 汇总 `165/0/0/0`、定向 `7/7`；Root NAS Docker build exit `0`。NAS 部署 image `sha256:34fb7e5daa3cf24818b459e95b0eb772063788fc52b331841e5780beed48f0cd` 正在运行，restart count 0；仅 image/build 改变，config/provider/model 保持原身份，旧 L-17 TTS fixed image `sha256:acf7314b488ad7e49232797fb42711faad637c00d29142ec782124a92b1d9291` 保留作回退目标且未执行回退。部署后 production-provider probe first-content/LLM-total `425/459 ms`，预期合成结果匹配；TTS 输出 6,480 bytes，SHA-256 `32c81da5dde3e7eff8c1878758d9e30d2f5617f07ab5ebbe524f205ba6ffb6d1`，解码 1,080 ms、RMS 3,510。此状态仅为 `NAS_VOICE_PROVIDER_SERVICE_VERIFIED / DEVICE_PREFLIGHT_PENDING`，不证明设备播放或修复 S-17 先前慢且答错的表现。该历史失败保留；修复后尚未新测真机，Preflight 0/2–3，正式 20 轮未开始。Root 已询问用户准备新测试，尚无回复，不记录为已开始。M0 仍 `CHANGES_REQUIRED`，AP outage/recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。证据与边界见 [L-21 NAS Voice provider report](V6_M1_NAS_VOICE_PROVIDER_REPAIR_REPORT.md)。

## 2026-10-20 L-17 NAS TTS repair follow-up（历史 service 与 S-17 Preflight 记录；当前状态由 L-21 更新）

L-17 source `812b12905cda01843f5aeb2d80835abb9bc2b084` 已由独立 R-11 复核 `PASS` 并在 NAS 部署。Root 基于同一 digest-pinned XiaoZhi v0.9.6 base 构建本地 EdgeTTS 7.3.1 image；依赖差异仅为 `edge-tts` 从 7.2.6 到 7.3.1。image ID `sha256:acf7314b488ad7e49232797fb42711faad637c00d29142ec782124a92b1d9291` 运行正常；部署后 production-provider service probe 返回 11,664 bytes、可解码 1,944 ms 音频。除 image/build 外，Compose/service config 与 NAS-local provider config 身份保持不变。原失败窗口保留历史；当前只判 `NAS_TTS_SERVICE_REPAIR=PASS`，不推断设备已播放。隔离 probe JSON 的音频 SHA 字段长度异常，未作为有效音频 hash；详见 L-18 报告。Root 未重刷 S-17 app/model，`preflight-02` 已结束：600.014 秒按上限正常停止，用户报告有响应但慢且答错，3 次 NAS interrupt、0 个新 HTTP 403；文本与状态 marker 不证明音频播放。设备状态为 `PREFLIGHT_CHANGES_REQUIRED_SLOW_INCORRECT_RESPONSE`，completed rounds `0/2–3`；正式 20 轮未开始，M1 未验收。截至本次 L-19 证据冻结，旧 Ollama native probe exit 1/result null、错误详情未保留；先前 production-provider 请求 90 秒总超时 exit 124，仅表示请求未完成，不能推断首字延迟。后续 10/20 新探针是独立证据，另行记录。NAS 相同 image 仍运行且 restart count 0。M0 仍 `CHANGES_REQUIRED`，AP outage/recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。详见 [L-18 NAS TTS repair evidence report](V6_M1_NAS_TTS_REPAIR_REPORT.md) 与 [S-17 device report](V6_M1_WAKE_MODEL_REPAIR_DEVICE_REPORT.md)。

## 2026-09-24 接管与用户指令状态（当前有效）

> **SUPERSEDED as the complete current NAS/provider and device status by the 2026-10-20 L-21 report:** The S-17 first-attempt Wake observation and NAS 403 warning below remain historical evidence. L-18 preserves the prior TTS repair and S-17 Preflight 02 result; current provider-service verification and pending device Preflight are recorded above. S15 results do not transfer to S17, and Candidate20 M0/AP recovery state is unchanged.

> 2026-09-27 更新：S-08 同一 Candidate 的用户辅助 Wake 尝试已连续采集并记录于 [Preflight attempt report](V6_M1_WAKE_PREFLIGHT_ATTEMPT_REPORT.md)。结论 `PREFLIGHT_BLOCKED_WAKE_NOT_OBSERVED / ROOT_CAUSE_UNRESOLVED`；不能概括为通用 WakeNet FAIL。Preflight 0/2–3，正式 20 轮未开始。

> 此前被动检查摘要仍由 [S-08 device report](V6_M1_WAKE_DIAGNOSTIC_DEVICE_REPORT.md) 保留；本次用户辅助尝试的状态与计数以 [Preflight attempt report](V6_M1_WAKE_PREFLIGHT_ATTEMPT_REPORT.md) 为准。

> **SUPERSEDED 设备状态：** 2026-09-27 S11 初始被动捕获及后续用户辅助片段仍按 [S11 AFE diagnostic device report](V6_M1_AFE_DIAGNOSTIC_DEVICE_REPORT.md) 保留；其 Candidate 与结果不转移至 S15。当前 S15 证据及 2026-10-18 只读镜像核验见 [S15 WakeNet Control 设备报告](V6_M1_WAKENET_CONTROL_DEVICE_REPORT.md)。

接管基线 `760b2aa66819f8d90186b43af4c02d9b33c8849e` 已保护至 `origin/takeover-v6-m0-c19-baseline`。Candidate20（`claw4-learning-v6-m0.20`）已构建并仅对 live `ota_0` 做 app-only 刷写；读回与 app hash 一致。候选构建布局仍与 live 分区布局不同，未执行恢复写回。A-02 结论为 `M0=CHANGES_REQUIRED`。

2026-09-24 时用户要求跳过 AP outage/recovery 刺激并直接开始下一阶段开发，随后授权在铭凡 N5 x86 NAS 部署小智服务；当时 M1 真机语音轮次尚未开始。该历史指令不改变 M0 结论、不把 AP failure/recovery 标成通过，也不代表 M1 20 轮验收已完成。后续 S11 app-only 写入及其范围见 [S11 设备报告](V6_M1_AFE_DIAGNOSTIC_DEVICE_REPORT.md)。NAS 服务已部署并通过合成连通性验证。

Candidate20 的 CODE/HOST/BUILD/DEVICE 证据分开记账；Candidate17/18/19 DEVICE 结果不得转移；旧 `clipped` 计数不用于验收。

第一实施波 L-01、L-02、S-01、S-02 均已提交并整合至 `7263f67`；L-02 定向复核修复整合至 `a128341`。独立 R-01 在 `a164f11ab5f5843d6ace5c4b990d86a29195a2fe` 复审通过，Host 结果 96/0。

| 队列 | 状态 | 说明 |
| --- | --- | --- |
| L-01 / L-02 / S-01 / S-02 | ACCEPTED | CODE/HOST 经 R-01 通过；DEVICE 证据仍按 Candidate20 单独闭合 |
| R-01 | PASS | 独立复审 source `a164f11ab5f5843d6ace5c4b990d86a29195a2fe`；Host 96/0 |
| S-03 | COMPLETE_WITH_GAPS | app-only readback PASS；设备矩阵部分观察；AP recovery 为 `SKIPPED_BY_USER / NOT_VERIFIED`；详见 [S-03 报告](V6_M0_CANDIDATE20_DEVICE_REPORT.md) |
| A-02 | CHANGES_REQUIRED | Camera 抓帧失败，AEC/唤醒、网络恢复、冷启动、稳定性、故障注入和 recovery evidence 未闭合；布局仍不匹配；见 [Gate 报告](V6_M0_CANDIDATE20_GATE.md) |
| M1-DEV-01 | ACCEPTED | Sol 定向收口；base `db97ad5994f3ee0c5e1fb4deb2fa4fa2ec3ee1ee`；提交 `75d0059a1f0ac6380c1e1e3ee5f41125a44139f2`；定向 13/13、V6 tools 109/109；仅证据工具，不代表 M1 语音验收 |
| M1-NAS-DEPLOY | SERVICE_DEPLOYED | 固定 v0.9.6 镜像、SenseVoiceSmall、本地 Ollama 和 EdgeTTS；OTA/WS/合成 LLM/TTS 连通性通过；[部署报告](V6_M1_NAS_DEPLOYMENT_REPORT.md)；DEVICE 20 轮 NOT_VERIFIED |
| L-03 | COMPLETE | Luna：根看板已同步；集成于 `bde85a5`，不改变架构或历史证据 |
| L-04 | COMPLETE | Luna：M1 Preflight/20 轮模板、日志解析与运行清单已集成至 `ee1a83b`；不改 Voice 架构或既有证据契约 |
| S-04 | COMPLETE | Sol：两处 NVS 初始化已失效关闭；新 Candidate `claw4-learning-v6-m1-preflight-s04-r02-20260925-02` 的 BUILD/Host/身份已冻结至 `f5343c2`；旧 app/ELF `SUPERSEDED` |
| R-02 | PASS | 独立 Sol reviewer：新 stage/镜像/锁哈希匹配，构建来源与集成代码 21/21 Git blob 相等；Host 120/120，设备仍 `NOT_VERIFIED`；仅放行 S-05 的 2～3 轮 Preflight |
| S-05 | BLOCKED_ENDPOINT_MISMATCH | 设备身份/live 布局与 app-only 写入及独立读回通过；启动后实际访问外部 `api.tenclass.net` HTTPS/MQTT，而非 NAS 7443/7444，语音轮次 0；用户已断电、硬件 owner 已释放；见 [设备报告](V6_M1_VOICE_PREFLIGHT_DEVICE_REPORT.md)。修正端点后须新 Candidate/复审/重核身份；AP outage/recovery 不重派 |
| S-04-ENDPOINT | COMPLETE | 原 Sol owner：M1 staged source 固定 NAS OTA/WS 端点，忽略旧 NVS OTA URL、拒绝外部 MQTT/WS 与自动升级；新 Candidate `claw4-learning-v6-m1-preflight-endpoint-s04-r03-20260925-03` 已冻结，旧 Candidate 作废 |
| R-03 | PASS | 独立 Sol reviewer：Host 123/123、22/22 源码映射、13/13 staged 输入、7/7 产物及锁/缓存哈希匹配；仅放行新 Candidate app-only 启动/端点核验，现场只访问 NAS 7443/7444 后才可做 2～3 轮 Voice Preflight |
| S-05-RETRY | BLOCKED_WS_ENDPOINT_NOT_VERIFIED | 新 Candidate app-only/独立读回 PASS，NAS OTA 7443 现场连接；两段 Wake 探针捕获之间有 53 秒空档，用户语句无法绑定，WS 7444 和 Wake 均不得判 PASS/FAIL，完整 Voice 0 轮；用户已断电。见 [重试报告](V6_M1_VOICE_PREFLIGHT_RETRY_DEVICE_REPORT.md) |
| S-05-WS-VERIFY | BLOCKED_WS_ENDPOINT_NOT_VERIFIED | 同一 Candidate 未重建/重刷；连续 326 秒捕获覆盖用户一次唤醒词尝试及回复，但未见 Wake/WS/外连/崩溃标记；不能判一般性 Wake FAIL，WS 7444 仍 NOT_VERIFIED，完整 Voice 0 轮；见 [连续捕获报告](V6_M1_VOICE_PREFLIGHT_WS_VERIFY_REPORT.md)。用户已确认设备断电，COM7 已释放 |
| S-06-WAKE-DIAG | DIAG_COMPLETE / ROOT_CAUSE_UNRESOLVED | 只读检查 324 条参考队列状态均为 idle/零计数，M1 未编译输入 RMS/读取失败诊断；未读出语音内容。不能定位该次未唤醒原因；详见 [诊断报告](V6_M1_WAKE_DIAGNOSTIC_REPORT.md)。设备保持断电，未操作硬件 |
| S-08-WAKE-DIAG-CANDIDATE-EVIDENCE | DEVICE_DIAGNOSTIC_COMPLETE / WAKE_AND_WS_NOT_VERIFIED | R-05 `PASS` 于 manifest revision `36d6443`；merged source `70d463938526ba768f889ebd548f8afd8a03f3af`。同一 S-08 Candidate app-only write/readback PASS，启动于 `ota_0`，观察到 NAS OTA HTTP `192.168.3.100:7443`，未观察外部 Tenclass；WS 7444 与 M1 Wake marker 未观察。48 个输入诊断窗口、771840 mic samples、RMS max 536、peak max 6080、读失败 0、crash markers 0；heap/PSRAM unknown。无 speech，Preflight 0 轮，正式 20 轮未开始。详见[设备报告](V6_M1_WAKE_DIAGNOSTIC_DEVICE_REPORT.md)；不得继承旧 Candidate Voice 结果 |
| S-08-PREFLIGHT-ATTEMPT | PREFLIGHT_BLOCKED_WAKE_NOT_OBSERVED / ROOT_CAUSE_UNRESOLVED | 同一 S-08 Candidate；只读 app 身份核验成功后，进行一次 201.328 秒连续 UART 捕获。188 个输入窗口 / 3,023,200 mic samples，RMS max 2,507、peak max 21,979、I2S read failures 0；观察到 NAS OTA 7443 一次，未观察到 Wake、WS 7444、外部端点或 crash marker。两次用户报告均无法绑定到准确声学帧；第一轮问题语句 `NOT_VERIFIED`。Preflight 0/2–3，正式 20 轮未开始。见 [尝试报告](V6_M1_WAKE_PREFLIGHT_ATTEMPT_REPORT.md)；根因未确定 |
| L-08-M1-CONTINUOUS-CAPTURE | HOST_COMPLETE / DEVICE_CAPTURE_USED | Luna high；`c4600aa` + 定向修复 `d5326fa`。有界连续 UART 采集、时间/字节索引和无载荷状态文件；定向 9/9、V6 Host 138/138。首次 COM7 冒烟检查因串口未枚举而未启动；之后同一工具成功完成 201 秒连续采集并用于 S-08 Preflight 取证。此采集仍不代表 Voice PASS |
| L-09-S08-PREFLIGHT-EVIDENCE | DOCS_COMPLETE | 同一 S-08 Candidate 的用户辅助尝试报告与两级看板更新；捕获/用户报告的证据边界、私有哈希均已记录。仅文档整理，不作根因判定或 WakeNet FAIL 推断 |
| L-11-M1-AFE-DEVICE-EVIDENCE | DOCS_COMPLETE | S11 Candidate 的 app-only/readback、分区不变和初始被动 AFE 输入诊断已记录；用户讲话未确认，不判 WakeNet FAIL/PASS、不计 Preflight。后续捕获由 L-12 同步。见 [设备报告](V6_M1_AFE_DIAGNOSTIC_DEVICE_REPORT.md) |
| L-12-M1-VOICE-OBSERVATION-SYNC | DOCS_COMPLETE | 同步 S11 后续用户辅助捕获：`voice-probe-02` 与 `post-restart-01` 各观察到 Wake/WS 与状态活动，其他捕获有用户报告无响应；不精确绑定话语与日志、不判一般性 WakeNet PASS/FAIL、不计完整 Preflight。S14 opt-in 诊断补丁 R-08 `PASS`（Host diagnostics only），未构建、未设备验证，不构成 S14 Wake PASS；见 [设备报告](V6_M1_AFE_DIAGNOSTIC_DEVICE_REPORT.md) |
| L-14-S15-EVIDENCE-RESUME | DOCS_COMPLETE | 补录 S15 Candidate 历史 app-only 写入/readback 与分区不变摘要、R-09 构建复核状态、9/27 诊断及 10/18 app 身份核验/被动启动摘要；另记录运行加载模型与候选目标模型不一致。`voice-rearm-02` 串口异常中断，用户确认一次 Wake 尝试但未观察到即时响应；捕获不构成完整 Preflight 或 WakeNet 通用 FAIL/PASS。模型修复授权待定。见 [S15 设备报告](V6_M1_WAKENET_CONTROL_DEVICE_REPORT.md) |
| S-15-M1-WAKENET-CONTROL-CANDIDATE | R-09 BUILD_REVIEW_PASS / MODEL_MISMATCH_CONFIRMED | S15 app-only 写入/readback 与分区不变；当前 app 身份核验匹配，但 live model catalog/runtime load 为 `wn9l_hai1tai4ling2`，S15 构建目标为 `wn9_nihaoxiaozhi_tts`。模型修复授权待定；不做目标词验收，不将被动/中断捕获记为 Voice round。Preflight 仍 0/2–3。见 [Candidate 报告](V6_M1_WAKENET_CONTROL_CANDIDATE_REPORT.md) 与 [设备报告](V6_M1_WAKENET_CONTROL_DEVICE_REPORT.md) |
| L-15-SRMODEL-READONLY-VERIFIER | HOST_ACCEPTED | `c4bb339`；只读模型目录、权重哈希与前缀校验；定向 10/10、集成 V6 Host 158/158。真实 live dump 正确返回目标模型缺失；不构成 DEVICE PASS |
| S-17 / R-10 | MODEL_REPAIR_VERIFIED / WAKE_OBSERVED / VOICE_BLOCKED_NAS_TTS_403 | Bundle `claw4-learning-v6-m1-model-repair-s17-20261018-01` 复用 S15 app、不重建；R-10 `PASS`，用户授权的单地址模型写入、prefix/full readback、边界与正确模型启动均核验通过。首个同步尝试用户确认 Wake 并提问但未听到答案；NAS EdgeTTS 403 warning 后续由 L-17 处理。Preflight 已完成 0 轮。见 [修复方案与回执](V6_M1_WAKE_MODEL_REPAIR_PLAN.md) 及 [设备报告](V6_M1_WAKE_MODEL_REPAIR_DEVICE_REPORT.md) |
| L-16-S17-DEVICE-EVIDENCE | DEVICE_EVIDENCE_RECORDED / WAKE_OBSERVED / VOICE_BLOCKED_NAS_TTS_403 | 记录授权写入/读回/边界核验、目标模型启动及 395.281 秒捕获。用户报告 Wake 成功并提问，但无可听答案；NAS 检查见 EdgeTTS 403 warnings，后续修复归 L-17。不是完整 Preflight round；完成轮数仍 0。见 [S-17 设备报告](V6_M1_WAKE_MODEL_REPAIR_DEVICE_REPORT.md) |
| L-17-NAS-EDGE-TTS-FIX / R-11 | NAS_IMAGE_DEPLOYED / REVIEW_PASS | L-17 source `812b12905cda01843f5aeb2d80835abb9bc2b084`；R-11 `PASS`；同一 pinned base，仅 EdgeTTS 7.2.6→7.3.1；NAS image 与 provider probes 详见 [L-18 report](V6_M1_NAS_TTS_REPAIR_REPORT.md)。不判 DEVICE playback |
| L-18-NAS-TTS-EVIDENCE | DOCS_COMPLETE / NAS_TTS_SERVICE_REPAIR_PASS / PREFLIGHT_CHANGES_REQUIRED_SLOW_INCORRECT_RESPONSE (historical device result) | 汇总旧 NAS TTS build/rollout 与 provider probe，并记录 S-17 `preflight-02` 慢且答错的历史观察；L-19 冻结时旧 latency probes 无有效结果，后续 L-20/L-21 另有 provider 修复与探针。本行不覆盖当前 `DEVICE_PREFLIGHT_PENDING`。见 [报告](V6_M1_NAS_TTS_REPAIR_REPORT.md) 与 [S-17 device report](V6_M1_WAKE_MODEL_REPAIR_DEVICE_REPORT.md) |
| L-20 / R-12 | SOURCE_INTEGRATED / REVIEW_PASS | L-20 source `2e9be855f3382db7f8182adfd197e94f88583e95`，集成 base `531ac23e8ed8608179ad83d42d163c0b8f40efa5`；R-12 `PASS` 仅限 CODE/HOST/readiness；Host `165/0/0/0`，定向 `7/7`。实际 NAS build 与 rollout 由 Root 独立执行，不等于设备验证 |
| L-21-NAS-VOICE-PROVIDER-EVIDENCE | DOCS_COMPLETE / NAS_VOICE_PROVIDER_SERVICE_VERIFIED | 文档记录 build、rollout、HTTP/WS/service 与 provider synthetic probes；config/model 身份不变，仅 image/build 改变。其设备 pending 状态已由 L-23 更新。见 [报告](V6_M1_NAS_VOICE_PROVIDER_REPAIR_REPORT.md) |
| L-23-S17-PREFLIGHT03-EVIDENCE | DOCS_COMPLETE / WAKE_OBSERVED / AUDIBLE_RESPONSE_UNCLEAR / PREFLIGHT_CHANGES_REQUIRED (historical device evidence) | 记录 438.889 秒同步 UART、用户确认 Wake 与可听响应但答题正确性未确认；strict regex parser 缺陷与 normalized ASR 匹配已区分。额外 ASR 不归因于 echo；正式 20 轮未开始，AEC/near-end/next Wake NOT_VERIFIED。L-27 是更新的设备观察。见 [报告](V6_M1_VOICE_PREFLIGHT03_REPORT.md) |
| L-25-NAS-ASR-PROMPT-REPAIR | DOCS_COMPLETE / NAS_SERVICE_VERIFIED | S-20 + L-24 integrated source `150fe555a83105ad10852865ec363404f24d9a78`; Host focused `10/10`, V6 full `175/175`; R-13 `PASS` after README fix, L-26 `!override` route correction R-14 `PASS` at `426f187c4ec9c916c09e4edebfbdea919ccce7b8`. Root NAS rollout reused image `9cd9…`, config `e568…`, no build/Flash, restart 0. Manifest SHA `0b61eca72f33ab98d482dafc1d1f337c0a5a0bf908baf4e0219e1c9007ce7d16`; active synthetic WS 2/2 expected-answer/TTS-stop cases, decode errors 0. WS probes bypass ASR producer and do not prove DEVICE. See [report](V6_M1_ASR_PROMPT_REPAIR_REPORT.md) |
| L-27-S17-PREFLIGHT04-EVIDENCE | DEVICE_EVIDENCE_RECORDED / AUDIBLE_CONTROLLED_ANSWER_OBSERVED / FULL_PREFLIGHT_GATE_INCOMPLETE | 300-sec capture; correct audible controlled answer confirmed, expected-answer marker, normalized ASR marker match; ASR→TTS marker 8.325 sec, not speaker playback latency. One ASR / no extra ASR in window (not AEC PASS); same-window 120-sec silence timeout end-prompt explains late transition. User-reported next Wake at capture boundary is not UART-bound. Full-gate credit `0/2–3`, formal 20 unstarted; AEC/near-end/next-Wake binding remain NOT_VERIFIED. See [report](V6_M1_VOICE_PREFLIGHT04_REPORT.md) |
| L-32-NAS-STAGE-PROFILE | COMPONENT_PROBE_RECORDED / HOST_REVIEW_PASS | Three isolated fixed synthetic component trials over actual Ollama/EdgeTTS wrappers; timing table and safe artifact hashes in [report](V6_M1_STAGE_PROFILE_REPORT.md). Not live pipeline/device timing; does not change L-27 Preflight `0/2–3`, formal 20 unstarted or M1 acceptance state |
| S-22-NAS-MODEL-WARMUP | HOST_REVIEW_PASS / NAS_SERVICE_VERIFIED | A-07 candidate reviewed R-16 PASS; targeted 9/9, integrated 199/199; isolated synthetic WS 2/2; Root rollout active synthetic WS 2/2. Full evidence and limits in [L-33 report](V6_M1_OLLAMA_PREWARM_REPORT.md). No DEVICE/ASR/M1 acceptance claim.

Candidate20 已有有限 DEVICE 日志、会话和 readback 事实，但不满足同候选 M0 完整证据；旧全片备份的分区表不同于当前设备，不能作为当前布局的已验证恢复基线。M0 Gate 保持 `CHANGES_REQUIRED`。M1 主机工具与 NAS 部署/合成连通性已完成；M1 验收仍定义为同一候选、同一服务版本下 NAS 连续语音 20 轮和方案规定的故障测试。旧 relay 学习闭环及 Candidate05–08 的 READY 入口均为 `SUPERSEDED`，不构成当前授权。

以下未再次标注的旧看板内容均为历史快照（SUPERSEDED），其中遗留的“当前有效”、READY 或派发入口只保留历史证据，不构成现行队列。

| 顺序 | 工作 | 负责人 | 状态 | 放行与证据 |
| --- | --- | --- | --- | --- |
| 0 | 架构/迁移构建入口/边界检查 | Codex | REVIEW_READY | 代码/文档已交付；78 case PASS，协调器受应用控制阻断，不能标全量 PASS |
| M0-1 | 四方源码冻结 + 环境/恢复清单 | Codex | IN_PROGRESS | SHA/IDF6.1/依赖锁已冻结；32MiB 完整备份和实际布局已核对，恢复写回尚未实测 |
| M0-2 | Claw4 Board Port | Codex | IN_PROGRESS | SD 挂载、metadata-only camera sensor init、触摸与电源键短/长按识别已实机确认；Candidate18 的限时单帧 RAW8 诊断已构建，尚未刷机验证。硬件矩阵解析已覆盖 SD 单次挂载、电源键短/长按和相机单帧/清理结果（接管基线共 83 项 Host 工具测试；C19 构建时为 68 项，之后新增 15 项；不代表新增实机验证）。电源键产品动作未实现。物理 RX ch1 保持静默；诊断版软件参考可进入 AFE ch1，但 AEC 效果未通过 |
| M0-NET | 隐藏网络回退与可复现依赖补丁 | Codex | REVIEW_READY | f47afda；62工具测试、7生产方法Host场景、IDF构建通过；真机未验证 |
| M0-NET-ERR | 网络失败恢复增量 | Codex | REVIEW_READY | 候选10编译链接通过，正常隐藏网络回退及取IP实测；同步调用失败分支未故障注入 |
| M0-3 | Candidate13 回归与音频闭环 | Codex | IN_PROGRESS | Candidate14/15 证实软件参考注入至 AFE ch1；90ms 与 0ms 两组各两次播放结果不一致，且非配对测试，AEC NOT PASS。Candidate17 五分钟静置采样无复位，读取失败/参考丢帧为 0；旧 clipped 字段不具削波证据，Candidate19 的 near_full_scale_n 已构建、未刷机。交互/AEC/播放期 WakeNet 仍待复测。Candidate18 单帧相机取帧等待实机验证；还需设备网络同步失败注入与完整长稳 |
| M1 | NAS 连续语音 20 轮 | Astra/Sol/Luna 按任务拆分 | WAKE_AND_CORRECT_AUDIBLE_RESPONSE_OBSERVED / FULL_PREFLIGHT_GATE_INCOMPLETE; completed=0/2–3 | L-25 NAS service verification is historical for its prior runtime; current synthetic service verification is L-33. L-27 `preflight-04` observed a correct audible controlled answer and matching normalized ASR/TTS markers; 8.325 sec is marker interval, not speaker playback latency. A late scheduled end-prompt transition was classified from the 120-sec silence path. User reported a second immediate Wake at capture boundary, but UART did not bind it. Full-gate credit remains 0/2–3; formal 20 rounds unstarted; AEC and near-end NOT_VERIFIED. M1 not accepted; M0 remains `CHANGES_REQUIRED`, AP recovery `SKIPPED_BY_USER / NOT_VERIFIED`. `preflight-02` slow/incorrect result remains historical. See [L-27 report](V6_M1_VOICE_PREFLIGHT04_REPORT.md), [L-25 NAS report](V6_M1_ASR_PROMPT_REPAIR_REPORT.md), and [S-17 device report](V6_M1_WAKE_MODEL_REPAIR_DEVICE_REPORT.md) |
| M1.5 | 七命令 pre-LLM router | Codex | BACKLOG | M1；认证/幂等/结果确认契约 |
| M2 | 真实学习闭环 | Codex；可拆 WorkBuddy 辅助 | BACKLOG | M1.5；在线/离线/重启/补传 |
| M2.5 | 离线主动提醒 MVP | Codex | BACKLOG | M2；RTC/TimeAuthority、缓存/内置音频恢复 |
| P0 总验收 | V6 原方案 §10 十一步 | Codex | BACKLOG | 20 轮 + 命令 + 断 NAS 提醒 + 完成 + PWA |
| M3 | Camera/WrongBook 业务 | 待分配 | HOLD | P0 总验收通过 |
| M4 | Learning Memory | 待分配 | HOLD | M3；事实与对话记忆独立 |

历史辅助任务建议（SUPERSEDED，不构成当前授权）：冻结 Board 接口后补硬件矩阵记录模板与重跑脚本；冻结 NAS schema 后补合成集成用例。尚未 READY，不能自行修改 Board/Voice 架构或沿 V5.3 继续接线。实际下发时另给允许路径、Base SHA、独立分支、测试、停止条件，Codex 按不可变提交复核。
