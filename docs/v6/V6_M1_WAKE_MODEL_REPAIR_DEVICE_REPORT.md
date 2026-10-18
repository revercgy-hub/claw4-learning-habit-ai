# S-17 Wake model repair 设备证据报告

更新时间：2026-10-18。记录已授权 S-17 模型分区写入、读回、边界核验及首次正确模型启动观察。当前仅确认模型修复和 runtime 模型身份；Voice Preflight 仍在进行，已完成轮数为 0。

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

写后以显式 USB reset 启动（不是冷启动），新建 `preflight-01` 捕获，最长 600 秒。被动启动摘要确认 S15 ELF prefix，runtime `Model 0` 与 `AFE_CONFIG` 均为 `wn9_nihaoxiaozhi_tts`，证明正确模型已被固件加载。Wake control 摘要为 `on_total=1/on_rc=1/off_total=0/off_rc=0`；因 `off_total=0`，`off_rc=0` 是初始值，不表示执行过 OFF 调用。此次被动启动不算 Wake/Voice Preflight round。

主 agent 已提示用户进行首次正确模型下的 Wake/数学提问观察；截至本报告提交时，尚未收到用户测试结果。`PREFLIGHT_IN_PROGRESS / completed rounds=0`；不宣称 Wake 成功、语音问答成功、AEC 有效或 M1 PASS。待后续捕获和用户观察整理后再补充证据。正式连续 20 轮尚未开始。M0 仍为 `CHANGES_REQUIRED`，AP outage/recovery 仍 `SKIPPED_BY_USER / NOT_VERIFIED`。S08/S11/S15 等旧 Candidate 设备结果不继承到 S17。

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

`HARDWARE_VERIFY_REQUIRED`: await the synchronized user Wake/math observation and final capture summary before counting any Preflight round. No additional Flash operation is recorded; any recovery would remain limited to the already authorized conditional same-partition full-backup rollback.
