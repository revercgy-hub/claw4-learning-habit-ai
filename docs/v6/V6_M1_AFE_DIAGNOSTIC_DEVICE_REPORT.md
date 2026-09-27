# S11 M1 AFE 输入诊断设备报告

日期：2026-09-27
任务：`L-11-M1-AFE-DEVICE-EVIDENCE`
分支：`codex/l11-m1-afe-device-evidence`；base：`948ce41e7d1ae6148b6ad8cf472bca96746b72b5`
Candidate：`claw4-learning-v6-m1-afe-diag-s11-20260927-01`
报告范围：本报告仅记录 Candidate S11 的 app-only 写入、只读身份/分区核对及一次被动、含义不确定的 AFE 输入诊断采集。它不构成唤醒、语音轮次或 M1 验收。

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

## 结论与边界

捕获期间是否存在用户讲话尚未确认。因此该样本只分类为**被动/含义不确定的 AFE 输入诊断**。输入窗口、RMS/peak、WakeNet 状态和零检测数不证明用户曾说话，也不能据此判定 WakeNet 失败或通过；本次不计 Preflight round，不覆盖 S08 的任何历史用户尝试或结果。

- M1 Preflight：`0/2–3`；正式 20 轮：未开始。
- WS 7444：`NOT_VERIFIED`；端到端语音链：`NOT_VERIFIED`。
- M0：`CHANGES_REQUIRED`；AP outage/recovery：`SKIPPED_BY_USER / NOT_VERIFIED`。
- `HARDWARE_VERIFY_REQUIRED`：仍需一项可与用户确认的 Wake/Voice Preflight 尝试，才能计入 2–3 轮 Preflight；本次讲话与否未确认的被动捕获不满足该项。后续执行范围由主控指示。本报告不假设用户在本次捕获中讲话。

## 证据自检

- Candidate、review、app/ELF/manifest 哈希和设备身份均按本任务给定证据记录。
- 只记录 app-only 写入；partition table、`otadata` 和限制范围未改变。
- 采集数据只以摘要和哈希呈现，不包含原始语音或凭据。
- 当前任务仅更新本报告及两份任务看板；未改源码、架构、runbook 或私有证据文件。

## 任务交付自检

- 修改文件：本报告、`V6_TASK_BOARD.md`、`docs/project_management/TASK_BOARD.md`。
- 状态同步：S11 是最新 M1 设备记录；S08 保留为历史，Preflight 保持 0/2–3，正式 20 轮未开始，M0 与 AP recovery 状态不变。
- 验证：`git diff --check` 通过；未运行产品测试（仅文档变更）。
- 范围偏差：无。硬件操作和原始证据采集均由上游设备任务完成，本任务只整理已提供的证据。
- 复核重点：校对 Candidate 身份/哈希、app-only 和分区边界、被动/含义不确定分类、历史 S08 不转移及看板交叉链接。
