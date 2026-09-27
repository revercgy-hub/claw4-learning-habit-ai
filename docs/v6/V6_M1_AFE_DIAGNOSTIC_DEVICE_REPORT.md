# S11 M1 AFE 输入诊断设备报告

日期：2026-09-27
任务：`L-11-M1-AFE-DEVICE-EVIDENCE`（初始采集）；`L-12-M1-VOICE-OBSERVATION-SYNC`（后续观察同步）
分支：`codex/l11-m1-afe-device-evidence`；base：`948ce41e7d1ae6148b6ad8cf472bca96746b72b5`
L-12 文档同步分支：`codex/l12-m1-voice-observation-sync`；base：`d0d8db0613759b356386bf715c006fe3a7d17d77`
Candidate：`claw4-learning-v6-m1-afe-diag-s11-20260927-01`
报告范围：本报告记录 Candidate S11 的 app-only 写入、只读身份/分区核对、最初的被动 AFE 输入诊断，以及之后用户辅助的连续 UART 观察。后续观察记录了部分 Wake/状态活动和数次未观察到响应的尝试，但都不能组成完整语音轮次或 M1 验收。

## Candidate 与设备身份

| 项目 | 结果 |
| --- | --- |
| Reviewed source | `cc1af698ed0d13066c71ae56d9365acb220683b7` |
| R-07 | `PASS` |
| 设备 | COM7，ESP32-P4 rev1.3 |
| MAC | `80:f1:b2:d2:ed:14` |
| app image | 3,137,328 bytes；SHA-256 `5038ed6e5649e8d11794dcdfbffae30d0a2e89cecceb182e00d424924da00a74` |
| ELF SHA-256 | `6a050109fdb821e7464ae9418bb6d137053ddbc550fa9b632cf093e69b57660a` |
| manifest SHA-256 | `4c973902c3b7ba4cd031c1471868c71b6e9bb172fa5e456fef503fa8cf834ba0` |

## 分区、写入与读回

- 写入前 live partition table 为 4 KiB，SHA-256 `8e5a526458f90a2b32a28ca0162611ca680e7ac8ca49f8336999ad85b529521a`。前 3,072 bytes 与构建 partition table 完全相等（构建表 SHA-256 `ef0039b6366c57de098972c0f6e9fd991013b41968da9b866c4704cb68ef7e5f`），末尾为 `FF`。
- 写入前 `ota_0` 9 MiB 备份 SHA-256 `f4876eadf6359a23b3d5bbd5b5ab3b2e1ef63a0210b14ab39e2cef1dfe0794ce`；其 app 前缀与先前 S08 镜像相等。
- 仅向 app offset `0x200000` 写入上述 app image。esptool 报告 hash verified；独立 readback 与 app image 逐字节相等，SHA-256 与候选 app 一致。
- 写入前后 partition table 与 `otadata` 均逐字节相等。写入前 8 KiB `otadata` SHA-256 为 `8ba3b110139f45443d4f268d1a3373ef99a1718b71d51664531b83ee2d4b91a3`。

## 启动与被动捕获

显式 USB reset 后进行连续、有界的 UART 捕获，时长 180.031 秒。设备启动身份观察到 ELF prefix 一次、`ota_0` 一次、NAS OTA 7443 一次；未观察到 WS 7444、外部端点或 crash marker。此次操作不称为 cold boot。

私有证据位于 `E:/v6/s11-device`，本报告不包含原始 UART、语音载荷或凭据。摘要文件 SHA-256：

| 文件 | SHA-256 |
| --- | --- |
| raw UART capture | `75029a13ed907e2ee86fae49d8cce1213a3f582b7b97fccb4f7d6945202dbca1` |
| index | `aef36794e3b6fce396beaf4636cb230793220e13affe50fe5cfa3e4565496ba8` |
| state | `4d5bdf3339041787fed58e1d3a99054ff64bcc4b32003f5d5f4c9d2c72fad5ed` |
| summary | `fdae59cd699ec94f66038c4b602476a1dc4100c67dc4afc080533d4979a04228` |

解析到 167 个输入窗口、2,685,920 个 mic samples；RMS 中位数 30、最大值 622，peak 最大值 6,779，I2S read failures 0。166 个 AFE state/flow 窗口显示 WakeNet enabled；最后观察到 active=1；WakeNet on 总计 1、off 总计 0。AFE feed calls 16,733、feed samples 2,677,280、feed chunks/fetch ok 各 5,228；fetch fail 与 other 均为 0。检测计数为 0，crash marker 为 0。

启动观察：ELF prefix 1 次、`ota_0` 1 次、NAS 7443 1 次、WS 7444 0 次、external endpoint 0 次。

## 后续连续观察

以下捕获是 S11 设备观察的后续记录。除明确标注为 restart 的片段外，不推断重启类型；`post-restart-01` 的重启方法未确认，不能称为 cold boot。用户对交互的描述与 UART 片段分别记录，不把话语、回答或响应绑定到精确声学帧或日志行。所有原始捕获及索引仍保存在私有目录 `E:/v6/s11-device`；此处仅列摘要统计和 SHA-256，不包含原始日志、语音、转写或凭据。

| 捕获 | 时长 / 摘要 | UART 与 AFE 摘要 | 用户辅助观察及边界 | 私有摘要 SHA-256（raw / index / state / summary） |
| --- | --- | --- | --- | --- |
| `voice-probe-02` | 连续 73.078 秒；60 个 mic/AFE 窗口；RMS max 1,946、peak max 23,728 | feed/fetch 1,884/1,884；AFE detected 1；观察到 NAS WS 7444；状态有 `idle → connecting → listening → speaking`，之后还有 speaking/listening 状态 | 用户先称无反应，之后报告有 Wake 响应；数学问题没有得到回答，等待超过 10 秒后出现新闻回退。随后设备被重启。上述用户描述不与特定日志事件或声学帧绑定，也不证明完整语音轮次。 | `0c567b1ff959d86f326ecf151da9d58ddfe8e2d4722ee6ba3bb609c37962794e` / `033cf8115d049b634c6c9000c019c45cb49b38f4871d077880e52471566b5690` / `e9cb1ee139e3528c9372ba4bf5bb3343a7ac632ce7939c8275257fc12297468b` / `1abc996816191de0a9ecfbdd34b6ee6ce47c97211b23aea59b37e7e3db92cf8d` |
| `post-restart-01` | restart 后的 no-reset 连续捕获，112.860 秒；113 个 mic/AFE 窗口；RMS max 3,022、peak max 24,769 | feed/fetch 3,523/3,523；AFE detected 1；观察到 NAS WS 7444；状态 `idle → connecting → listening → speaking` | 用户先称无反应，之后报告设备在听。重启方法未确认；用户报告不与特定日志事件或声学帧绑定。 | `0f29eb1091dcd024889f1f4347e3d9fecd66fafa8b85f2e240f8e488a75a12bc` / `b61821dfbbdcd7c079c7a710cfb507402bcc8489858a4521793badfa7128073c` / `d42f0996c0b3996f8c6f6feb7484685e648d54eaa2f313d4bfd8abde5653f5b3` / `4fd225c5b5b98897b1db43860053c52aa6221eaa1f302459566954eca4acc748` |
| `voice-followup-03` | no-reset 连续捕获 120 秒；119 个 mic/AFE 窗口；RMS max 65、peak max 612 | feed/fetch 3,749/3,749；wake_last=0；未见 STT/TTS marker；有一次较晚的 `listening → speaking`，因此并非无状态变化 | 用户确认数学问题发生在采集开始两分钟以后，即此 120 秒捕获结束之后。本捕获不能用于判定该问题是否失败。 | `d0c5f283d9f372f4b04bb7d375a6ad11e376564a740c6eaaf142adee35df772e` / 未列入本次摘要 / 未列入本次摘要 / `cd5e6008d1defda300807b1581d74d56bbc44da12da01c2cb4af6b92ea57a703` |
| `voice-question-04` | no-reset 连续捕获 120.030 秒；119 个 mic/AFE 窗口；RMS max 1,093、peak max 10,186；43/119 窗口 RMS ≥ 200 | feed/fetch 3,749/3,749；wake_last=1、active=1、detected=0；未见状态迁移或 crash | 一次用户确认的 Wake 尝试没有响应。此捕获未显示 Wake 检测；仅凭窗口幅度和摘要不能判定根因或一般性 WakeNet 结果。 | `b5ac5fce955023ddb5f8641674f9d8a0e1b2d479b6c50b320b34544d2bf84fa1` / `6388568ff26b665a0ba1354f5638e43144b078ffe753d7452c4b63387f4854df` / `d9cbe9f47eeaf340d925fce77c1d00b9463a04c5a946982b7c3c43d61e88d8f2` / `0e47c25a5b70fc4c0f62fad28dce161e6dc49010e1675e1bb236bc29a4502c04` |
| `rearm-05` | no-reset 连续捕获 163.061 秒；162 个 mic/AFE 窗口、2,604,960 mic samples；RMS max 493、peak max 13,235 | feed/fetch 5,103/5,103；wake_last=1、active=1、detected=0；未见状态迁移、crash 或 read failure | 两次用户确认的 Wake 尝试均无响应。摘要显示采集期间仍有 AFE/输入处理活动；不能由 `wake_last` 推断此前是否成功启用，也不能证明确定性的 re-arm 缺陷。 | `d3164d2e1ccd8e47a6891464a1aa5eeab1a064ca65188753b05293d4d17b25d5` / `1918e6f7341eda4f9273216097747fccdf53ea4759a013ee01fffa502bcae016` / `9e72b06e8eb9a1fe206bb871a2d41c675b71be9379d7e23d8957eedf186082ba` / `74017cf053ba4207338a85288819ac5afc3a6ea27abc5e930f0725f1d6de8832` |

这些片段显示两段独立捕获（`voice-probe-02`、`post-restart-01`）各自观察到 Wake/WS 与状态活动，也有用户报告无响应且捕获未见 Wake 检测的片段。它们没有覆盖完整 Wake→Speech→ASR→LLM→TTS→Playback→Next Wake 流程；两次捕获中的 Wake/WS 观察不能扩展为稳定通过。Realtime Listening 会有意关闭 WakeNet；现有 `wn_on` 间隔为零不能说明先前 enable 的返回结果。S12/S13 审计未证明确定性的 lost-rearm 缺陷。S14 opt-in 诊断补丁的 R-08 review 为 `CHANGES_REQUIRED`，定向修复进行中；补丁待复审、未构建、未设备验证。

## 结论与边界

最初的 S11 样本仍只分类为**被动/含义不确定的 AFE 输入诊断**；后续片段增加了用户辅助观察，但日志事件与用户描述没有精确同步，且结果不一致。捕获窗口、RMS/peak、WakeNet 状态或某个片段中的检测数均不足以判定一般性 WakeNet 失败或通过，也没有形成可验收的完整 Preflight round。后续捕获不覆盖 S08 的 Candidate 或历史用户尝试结果。

- M1 Preflight：`0/2–3`；正式 20 轮：未开始。用户辅助捕获期间曾观察到 NAS WS 7444，但端到端语音链仍为 `NOT_VERIFIED`，稳定性也未验证。
- WakeNet 通用结论：`NOT_VERIFIED`；Realtime Listening 期间 WakeNet 有意关闭。无确定的 lost-rearm 缺陷结论。
- M0：`CHANGES_REQUIRED`；AP outage/recovery：`SKIPPED_BY_USER / NOT_VERIFIED`。
- `HARDWARE_VERIFY_REQUIRED`：仍需完成并可核验 2–3 轮 Wake/Voice Preflight；现有用户辅助尝试没有证明完整轮次。S14 诊断补丁待复审、未构建、未设备验证，且仅用于补充 WakeNet attempt/return 观察，不替代 Preflight。不得把语句或响应与捕获精确关联。本报告不包含原始语音、转写或凭据。

## 证据自检

- Candidate、review、app/ELF/manifest 哈希和设备身份均按本任务给定证据记录。
- 只记录 app-only 写入；partition table、`otadata` 和限制范围未改变。
- 采集数据只以摘要和哈希呈现，不包含原始语音或凭据。
- 当前任务仅更新本报告及两份任务看板；未改源码、架构、runbook 或私有证据文件。

## 任务交付自检

- 修改文件：本报告、`V6_TASK_BOARD.md`、`docs/project_management/TASK_BOARD.md`。
- 状态同步：S11 初始被动样本与后续用户辅助片段均保留；S08 保留为历史，Preflight 保持 0/2–3，正式 20 轮未开始，WakeNet 与端到端 Voice 不作通用 PASS/FAIL，M0 与 AP recovery 状态不变。
- 验证：`git diff --check` 通过；未运行产品测试（仅文档变更）。
- 范围偏差：无。L-12 仅同步摘要与已提供哈希；未操作设备、读取原始 UART 或修改私有证据。
- 复核重点：核对用户报告与日志的边界、各捕获统计/哈希、no-reset/restart 描述、WakeNet/AEC/M1 状态以及 S08 不转移和看板交叉链接。
