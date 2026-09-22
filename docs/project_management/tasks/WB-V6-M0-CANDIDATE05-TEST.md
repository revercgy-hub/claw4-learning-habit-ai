# WB-V6-M0-CANDIDATE05-TEST

状态 CHANGES_REQUIRED，已由 WB-V6-M0-CANDIDATE06-TEST 承接，旧队列不可再领取。以下原任务说明保留。

原状态 READY。2026-09-22 用户明确要求后续测试交给 WorkBuddy，Codex 负责重要开发与复核。本文件是唯一活动 WorkBuddy 测试流，旧 WB-V6-M0-AUDIT-001 已交接，不另开并行流。

## 输入与边界

先读 E:/workbuddy/claw4-v6/AGENTS.md、本任务包、docs/v6/CODEX_V6_M0_REVIEW_2026-09-22.md、docs/v6/V6_M0_CANDIDATE_05_REPORT.md、integration/v6/m0-candidate-05.json，以及上轮 WorkBuddy handoff。原唤醒提案和历史矩阵仅作已被修正的参考，不直接照抄判据。

不可变实现/工具基点 e7db074a8e468b9729bcb1057ce1853cd754cd68。在干净独立 worktree 建 workbuddy/v6-m0-candidate05-test；从该基点开始，另外读取 Codex 当前任务包/看板。不要切换或修改 Codex 的工作区，不改原 WorkBuddy 旧分支的历史。

当前设备 COM7 / ESP32-P4 rev1.3，已经刷候选 05；app SHA256 f109b9282fcec6711a351e2fa62523a51ecce0fb2447fd7803b1a682883cc0d6，3,045,984 字节；完整 ELF SHA 见候选，运行日志前缀 533d4a15d。源代码和四项关键文件哈希以 manifest 为准。设备仍是本地 M0，无云端协议/OTA。

解释器绝对路径 E:/workbuddy/claw4-v6/toolchains/idf61/python_env/idf6.1_py3.12_env/Scripts/python.exe。构建树 E:/v6/s1 只读。原始数据 E:/workbuddy/claw4-v6/out/v6-device-private/ 只读参考；自己的新捕获写到自己 worktree 的 out/v6-device-private/candidate05-test/，保留原文件，不覆盖、不要提交原始日志或 NVS/口令。

允许修改自己的 docs/project_management/reports/WB-V6-M0-CANDIDATE05-TEST_REPORT.md、docs/v6/V6_M0_CANDIDATE05_TEST_MATRIX.md、integration/v6/m0-candidate05-test-*.json。允许只读查看源码、运行现有工具测试、操作 COM7 复位/monitor、配合用户屏侧/语音测试。需要补工具时先记录需求，Codex 决定实现。

禁止改 Board/audio/main/managed_components/vendor/sdkconfig/分区/构建树；禁止刷写、擦除、恢复写回、C5/eFuse/安全策略变更；禁止自行改增益/阈值；禁止上传音频/凭据。不在其他程序占 COM7 时并发接入。Codex 已释放串口；领取前仍核实。已观察的 10 轮见 m0-candidate-05-boot-series.json，不得将未计账尾部日志补写成 Codex 已验证结果。

## CP0 READY：候选核对与工具回归

核对所读代码基点与冻结候选，运行 python -m unittest discover -s tools/v6 -p "test_*.py"，期望 49 项。所有命令用实际解释器绝对路径和 worktree 绝对路径。不要打印私密 NVS 或口令。

采一份不超过 60 秒的日志，需 CANDIDATE_ELF_SHA256 与 manifest 匹配；如果没有启动标识，可做一次 --reset 采集。BOOT_READY、Assets applied=1、没有 abort/BOOT_BLOCKED 是继续条件。发现固件不一致则停止联系 Codex，不能自行重刷。

## CP1 QUEUED：20 轮 USB 复位

运行 tools/v6/repeat_boot.py --port COM7 --candidate <manifest绝对路径> --output-dir <全新私密目录> --rounds 20。工具约 7 分钟，每轮 20 秒，失败自动停测。结果 JSON 脱敏复制到允许路径，原始日志不提交。

成功判据：20 轮都 passed=true，一轮一次 app_main/BOOT_READY、无 panic/BOOT_BLOCKED、候选匹配。I2C_RETRY/RECOVERED 如出现须报告实际次数。0 次重试不能证明恢复分支已测试；USB 复位不等于断电冷启动，不统计成 20 次冷开机。任何失败保留日志，停止设备后续队列并报告 Codex。

## CP2 QUEUED：唤醒与回环同步刺激

和用户协调开始时间，先静音背景约 10 秒，再正对设备 20–30cm、正常音量说 4 次“你好小智”，两次至少间隔 3 秒，保持同一串口捕获窗口；可分段 60+45 秒连续取证。明确记录用户确认的次数/时段，不把没有刺激确认的窗口当成失败率。

逐路汇总 INPUT 的 n/rms/peak/clipped/raw_peak/read_failures；channel 0/1 的值分开，不混成单一 RMS。计数 WAKE_DETECTED 与 WAKE_REARM。四次有确认的说话无正事件，记录可复现“该刺激条件下 0/4”，交 Codex 分析，不调增益。不从 VAD 缺失判定电平低：此固件明确 VAD_OBSERVABLE=0，wake-only 不调用 HandleVoiceResult。

若有命中，检查后续 WAKE_REARM armed=1，并至少再次真实命中，验证重复唤醒。单次正事件仅证明能触发，不等价可靠唤醒率。用户不方便配合则 CP2 记 NOT_VERIFIED，不阻塞独立 CP3；无需让用户反复同意整个队列。

最后一次用户点击录音按钮，说一句话，确认听到自己回放；同时看 TOUCH、record/playback 日志和两路电平，特别关注播放时参考通道是否有变化。可闻回放绕过 AFE，不推出 AEC/唤醒通过。报告失真/音量/参考链路为独立项。

## CP3 QUEUED：30 分钟连续观察

不再复位。连续 30 份各 60 秒捕获（实际起止 wall-clock 记录），片段之间的采集间隙也记录；不能把设备 uptime 称为观测时长。每分钟 HEALTH 与 INPUT 统计摘要，关注 abort/BOOT_BLOCKED、串口丢失、读取失败、内存趋势、NETWORK_EVENT。无需用户持续在场；不要另建自动任务。

判据：没有 crash/block，HEALTH 连续且采集缺口有解释；发现网络断连或明显持续内存下降，保留时刻及前后数据，记异常/未闭合，不用“允许一次断连”把它直接判 PASS。没有断线刺激不能标重连已验证。本候选隐藏 SSID 缺口未修，不要求用户切回隐藏网络反复验证。

## CP4 QUEUED：报告与交接

每个 checkpoint 独立提交，报告写实际 SHA、命令、日志文件名+SHA、刺激条件、结果与限制。私密日志留本机供 Codex 独立复核。使用更新后的 hw_matrix.py；显示/回环默认不假定确认，只有本候选确有用户确认才传对应参数。只合并同一候选；没有 ELF 标识的中段捕获须用连续采集记录绑定前序，不能随便并入。历史候选 02/03/04 不放入此矩阵。

完成后标 REVIEW_READY，提供最终分支 SHA 和报告入口；Codex 决定 ACCEPTED/CHANGES_REQUIRED。不得自行宣称 M0 总 PASS：SD/Camera/电源键、实际故障注入、恢复写回仍有未完成项。M1 仍是 NAS 原生语音 20 轮，不是学习业务后端 E2E。
