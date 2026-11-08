# L-34 S-17 Voice Preflight 05 证据报告

- 日期：2026-11-08（Asia/Shanghai）
- 基线：`4b0aeeef18ee22d58ef4fd30816c7770cedc2d4f`
- 分支：`codex/l34-preflight05`
- 结论：`PREFLIGHT_CHANGES_REQUIRED / TTS_RECAPTURE_OBSERVED`
- 证据目录：`E:/v6/s17-prewarm-preflight05-20261108-01`
- 安全摘要 SHA-256：`923f90097c92398d19059778d29ba7413c78e834f448d0ff46c7a50c6283ae76`

## 范围与身份

本次是一次 427.176 秒连续捕获，显式 USB reset 启动，非 cold boot。未构建、未 Flash、无 app readback；启动 ELF 前缀为 `33bcb2d01`，目标模型运行标记、NAS OTA 7443 与 WS 7444 均观察到。捕获无 crash，I2S read failure 为 0；heap/PSRAM 仍未知。NAS prewarm L-33 image/config/container 与本次安全摘要一致，restart count 0、HTTP 403 和 traceback 均为 0。NAS 服务证据与设备证据分账。

## 观察

本次有三个练习情境，但均不计完整 Preflight round（当前 0/2–3）：

1. R1：操作者唤醒设备，听到完整播报，报告等待约 1 秒；NAS ASR→first-audio marker 为 1411.991 ms，回答期间无额外 ASR。严格的 R1 prompt 文本未建立，marker 延迟不等于扬声器实音 onset。
2. R2：count-to-20 情境中，操作者报告只说过一次受控 near-end marker，无其他人声；ASR、LLM 与可听回答均有记录。另有两次 ASR 被转发至 LLM，其规范化子串匹配先前 TTS，发生在播放期间/边界。这是该中断情境下强烈的 TTS recapture/self-reply 观察；不证明物理声学根因，也不构成通用 AEC 或 near-end PASS。
3. R3：同一捕获中从 idle 再次 Wake，操作者报告回答正常且无自响应；随后到捕获结束没有额外 ASR。没有测得真实扬声器发声起点。

另有一次延迟的 scheduled silence-end prompt，NAS marker 与已知 hash 精确匹配；属于自然 idle 路径，不是 ASR。安全 parser 的 Wake 计数为 0，但私有 safe timeline 有与本次 Wake 关联的文本形态；AFE detected 总计 2。该差异来自仓库 parser allowlist 只接纳无后缀字面值，不能据此改产品代码或宣称 parser/设备 Wake 失败。原始 UART、音频和文本不进入报告。

## 只读源审计与判断

S-23 对冻结 staged 源与配置的只读审计发现：AFE Wake、audio processor 与 device AEC 在配置中启用；speaking 期间实时 listening 仍保留 Voice processing。Claw4Audio 在 I2S write 返回后提供 post-volume TX reference samples，RX 独立从有界 FIFO 消费；underflow 可能送零，delay 0 明确未校准。先前 0ms/90ms 比较未证明收益（[M0 AEC reference experiment](V6_M0_AEC_REFERENCE_EXPERIMENT_REPORT.md)）。因此根因仍 `NOT_VERIFIED`；不做盲目 gain/delay 调整，不做服务端静音或 uplink gating，也不请求新的设备复测或 Flash 授权。

如后续另行授权诊断，范围应先限定为同步聚合观测：TX accepted/reference active/underflow/queue occupancy、mic/reference envelope 与 AEC state；再在同一候选上对比 playback-only 与一次 near-end。此建议不构成实施或设备操作授权。

## 状态与验收

- Preflight：`PREFLIGHT_CHANGES_REQUIRED / TTS_RECAPTURE_OBSERVED`，完整轮次 `0/2–3`。
- Formal 20：`NOT_STARTED`；M0：`CHANGES_REQUIRED`。
- AP outage/recovery：`SKIPPED_BY_USER / NOT_VERIFIED`。
- 未解决：声学根因、AEC effectiveness、heap/PSRAM、严格 prompt 匹配、真实 speaker onset 与完整轮次门槛。
- L-34 仅更新报告和看板。现场采集及 NAS 核对由主线程完成。

## 复现与完整性

已核对 safe summary 的 SHA-256，且只读取指定安全 JSON（safe summary、timeline、echo match、NAS final、parser summary、capture state），未读取 raw capture。证据摘要列出的七个文件均通过 Get-FileHash 核验，原始 UART 文件仅核对哈希；链接目标存在；状态与两份看板同步；`git diff --check` 通过。详见对应安全摘要及 [L-33 NAS prewarm report](V6_M1_OLLAMA_PREWARM_REPORT.md)。
