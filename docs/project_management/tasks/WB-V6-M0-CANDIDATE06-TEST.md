# WB-V6-M0-CANDIDATE06-TEST

状态 HOLD（用户要求先完成下一阶段开发再统一复核），禁止领取或执行以下旧刷写步骤。原任务说明保留。旧 candidate05 流 CHANGES_REQUIRED 的待修项由此承接；不并行执行旧包。Codex 负责固件与复核，WorkBuddy 负责本包设备测试。

## 输入与边界

独立干净 worktree，从 `codex/v6-foundation @ ccf7dab5b02259c994bfc37f13940b7950a060e1` 建 `workbuddy/v6-m0-candidate06-test`，另外读取本任务包及最新看板。先读根 AGENTS、`docs/v6/CODEX_V6_CANDIDATE05_REVIEW_AND_06.md`、candidate05 报告 §8/§9、`integration/v6/m0-candidate-06.json`。复核文档与本包位于该实现基点之后的 docs 提交，必须从 Codex 当前分支读取。

仅允许修改本流报告 `docs/project_management/reports/WB-V6-M0-CANDIDATE06-TEST_REPORT.md`、矩阵 `docs/v6/V6_M0_CANDIDATE06_TEST_MATRIX.md`、脱敏 `integration/v6/m0-candidate06-test-*.json`。私密原始日志留自己 worktree 的 out/v6-device-private/candidate06-test，不提交。Board/上游音频/增益/配置/构建树不改。

用户已授权普通刷机及由 WorkBuddy 接续测试；本包将准备步骤限定为 **COM7 上一次 candidate06 app-only 写入 0x200000**，不刷其它镜像，不改分区/C5/eFuse/安全策略，不擦 NVS，不做恢复写回。不得在其他进程占串口时接入；不得重建或修改 E:/v6/s1。固件不匹配或刷写失败立即停止，不试其它地址。Codex 本轮未占用串口或刷机。

解释器 `E:/workbuddy/claw4-v6/toolchains/idf61/python_env/idf6.1_py3.12_env/Scripts/python.exe`；app `E:/v6/s1/build/xiaozhi.bin`；备份 `E:/workbuddy/claw4-v6/out/v6-device-private/pre-v6-full-flash.bin`（32MiB，SHA256 b77343691c97359eaedba4d7d8353ef6df55b10ae2f5f9a13059943c04bb9413）。

## CP0 READY：只核对后写入

1. 运行 56 项 tools/v6 Python unittest；核对 manifest 的 app/ELF/4个非 app 镜像、备份 hash 和目标 P4 rev1.3/32MiB。只读运行 flash_plan 布局检查；该输出含全部镜像，**不是本包的刷写命令**。
2. 仅在以上一致且串口空闲时运行解释器 `-m esptool --chip esp32p4 --port COM7 --baud 460800 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size 32MB --flash-freq 40m 0x200000 E:/v6/s1/build/xiaozhi.bin`。保存实际命令和 hash verified 证据。
3. 捕获 ≤60 秒启动，ELF 前缀匹配、BOOT_READY、Assets applied=1、无 panic/BOOT_BLOCKED 才继续。新日志必须来自 candidate06，不能借用 candidate05 视觉/音频确认。

## CP1 QUEUED：定向音频回归

和用户协调一次刺激批次：10秒安静，20–30cm正常音量连续说4次唤醒词，每次间隔≥3秒；记录实际刺激、命中和 rearm。随后至少两次“录3秒/回放”，用户确认是否听到及音量变化。保留从启动到此时的会话关系；不上传录音。

逐路汇总 INPUT n/rms/peak/clipped/raw_peak/read_failures、tx_overlap_n/peak、tx_frames。必须检查 LOCAL_REFERENCE_PROBE_BEGIN/END drained=1，播放过程中持续有 INPUT，结束后唤醒恢复。无重合样本时参考仍 NOT_VERIFIED；有重合且 ch1 全零记录高优先级疑点，不直接宣称硬件无 AEC。tx_overlap 是软件写调用重合，不是声学时标。

若正常刺激下唤醒0/4、无可闻回放、read_failures、超时 drained=0 或异常复位，保存证据并暂停后续设备队列，不调 PGA/阈值/声道。用户暂不可配合则本项 NOT_VERIFIED，可继续独立的 CP2。

## CP2 QUEUED：稳定性

CP0通过且无 CP1 失败时，执行同候选20轮 repeat_boot USB复位；随后30份60秒空载捕获，不主动制造硬件故障。记录每段 wall-clock、uptime 与实际缺口。断电冷启动、真实 I2C 恢复、网络重连仍未验证。无关联不能称网络稳定通过；隐藏 SSID 缺口本轮未修，不要求反复改网络。

## CP3 QUEUED：工具与报告

用 `audio_evidence.py --manifest <私密会话清单> --output <脱敏汇总>`。清单格式见脚本 docstring，每个 session 首段包含启动 ELF 标识，后段按时间排序，绑定每份文件 SHA256。工具拒绝混候选/重复/重启续段；仍须报告操作者确认的会话连续性，不能只靠 uptime 推断。保留输出的限制说明，旧 hw_matrix 最大值不是总事件数。

每个 checkpoint 独立提交、报告实际SHA/命令/结果/限制；工具需求只记录给 Codex。结束释放 COM7，标 REVIEW_READY，提供分支 HEAD 和报告入口。M0 不总 PASS，SD/Camera/电源键及恢复仍未覆盖。隐藏网络修复另包处理，M1保持门禁。
