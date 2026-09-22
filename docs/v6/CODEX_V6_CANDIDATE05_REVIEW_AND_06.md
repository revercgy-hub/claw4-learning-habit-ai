# Candidate 05 复核与 Candidate 06

2026-09-22，Codex。复核对象 `workbuddy-v6-m0-candidate05-test @ bf34b4e`，已通过 merge `18abe25` 保留全部原始提交。开发基点 `ccf7dab5b02259c994bfc37f13940b7950a060e1`（固件 `c720c29`）。

## 复核结论

**CHANGES_REQUIRED（结论表述及音频缺陷）；测试证据保留，M0 未总通过。** 后续由 candidate 06 流承接，不要求重写历史证据。

- 独立读取 CP1 的 20 份原始日志，20/20 符合一次匹配候选启动、BOOT_READY、无崩溃的工具判据。只覆盖 USB 复位，未覆盖断电冷启动或实际 I2C 故障恢复。
- CP2 两份唤醒日志分别 5、1 次，合计 6 次唤醒与 6 次 rearm；不外推唤醒率。麦克风窗口为 52、44、57、30；转换 clipped 合计分别 0、864、2656、0。转换放大后溢出成立。
- **参考通道故障结论 SUPERSEDED**：旧 harness 在 Playback 阶段关闭所有输入消费者。loopback-01 的 playback queued 为 153678ms，下次 INPUT 为 157178ms，恰好间隔 3500ms；“入队之后”不等于“实际播放期间”。旧日志说明已采集 ch1 窗口为零，不能证明同步播放参考必定无效，更不能证明硬件 AEC 不可用。当前状态 HARDWARE_VERIFY_REQUIRED。
- CP3 原始 30 文件与报告 SHA 全匹配，180 HEALTH 样本，free=27114843、psram=26834276 恒定。按提供的 wall_start/end 逐项复算，29 个间隙合计 **35 秒**；原文其它间隙总量表述 SUPERSEDED。时间元数据仅秒精度；串口覆盖不等于完整 wall-clock 跨度，也不证明间隙中没有异常。复算结果见 `integration/v6/m0-candidate05-codex-recount.json`。
- 网络关联/IP 保持 NOT_VERIFIED。扫描未发现已保存 AP 不能独立证明“AP 不在空中”，也不足以排除隐藏 SSID、信道或客户端逻辑。原报告“这是环境”属于未证实归因，SUPERSEDED。配网扫描周期与现有定时器实现一致，但未测功耗。
- 保留原报告 §8 的限制；SD/Camera/电源键、恢复写回、真实故障注入及 M1 NAS 20 轮都未完成。

## 修订实现

1. 32 位输入槽提取高 16 位，去掉原 >>12 隐式 16 倍增益。转换不会越过 int16 范围；`clipped=0` 因此只说明没有转换饱和，不证明 ADC/模拟源没有削顶。raw_peak 继续保留。有效电平降低约 24dB，唤醒与可闻回放必须回归；不采纳无校准依据的 RMS 1000–3000 门槛，不自动改 PGA。
2. 录音开始前启用 voice processing（该调用会清 decoder，不能移到录音完成后），使回放时 RX 有消费者。录音队列排空结束，10 秒有界超时，随后恢复 wake。AFE 编码包在本地丢弃，未启动网络协议/上传音频。
3. 两路 INPUT 新增 tx_overlap_n/peak、tx_frames；仅表示采样统计处理与非零 I2S 写调用的重合，不是物理 DAC 时间测量。需要有声刺激、实际回放确认与足够重合窗口；参考非零也不代表 AEC 质量合格。
4. I2C 三次重试策略抽出为生产共用函数，注入式 Host 测试覆盖首次成功、第二/第三次成功、三次失败及 100/200ms 等待。没有把 Host 注入写成真机总线恢复通过。
5. 新 `tools/v6/audio_evidence.py` 接受 SHA256/会话/路径清单；首段必须有匹配 ELF 标识，后段不得重启或时间倒退、重复/重叠文件被拒绝。按实际捕获的 wake 事件计数（session+counter），HEALTH 按 session+timestamp 计数，不使用 max 或 last-first。明确绑定仍需要操作者确认同次启动，单调时间不证明物理连续。旧 hw_matrix 的 max 汇总不作为全会话事件总数。

## 验证与冻结

- 56 项 Python unittest PASS；C++17 `-Wall -Wextra -Werror` Host 编译运行 PASS，覆盖全部 65536 个 int16 左对齐槽往返及边界、重试注入。初次 Host 编译缺 assembler PATH，加入同一工具链 bin 后正常运行，未修改安全策略。
- IDF 增量构建 PASS，私密构建输出 `out/v6-device-build-10.txt`。freeze 检查 overlay 全文件一致、关键配置应用、9 个上游运行时源文件无内容改动。
- Candidate 06 app 3,048,496 字节，SHA256 `3b2d7dd0b910c2aa4262bad82cda6439e2e2d9096eebbd0f639f0b5ea6519d7d`。
- 完整 ELF SHA256 `0dce199900357d4daf76002c239ddf5d3766b31afe47814f88c89bc6cb90d5f2`；启动标识可能只输出 9 字符前缀，不能冒称完整 hash。
- manifest `integration/v6/m0-candidate-06.json`。四个非 app 镜像与 candidate 05 完全一致；32MiB 备份与当前分区布局预检 PASS。
- **尚未刷 candidate 06、未做新设备测试。** 当前设备证据仍属于 candidate 05。后续测试由 WorkBuddy 独占 COM7，按新任务包实施。
