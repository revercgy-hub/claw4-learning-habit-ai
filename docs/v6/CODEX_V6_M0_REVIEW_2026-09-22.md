# Codex V6 M0 交接复检与下一批修正

日期：2026-09-22。复检基点 1b52240，WorkBuddy 分支不可变头 e923a48，实际 17 笔提交、10 个文件。已 fast-forward 整合到 codex/v6-foundation，未改 WorkBuddy 工作区或分支，未推送发布。

## 结论

交接取证接受作为调查输入；原自动验收工具与部分推论 CHANGES_REQUIRED。本轮直接修正并补测，不授予 M0 总验收。当前设备仍运行 m0-candidate-04；本轮源码未刷入。

1. 独立读取 netprobe-m0-07.txt，确认三次 app_main、两次 I2C timeout/abort、一次 BOOT_READY。该日志 SHA 与矩阵一致，ELF 前缀 180ba8dce 与候选 04 一致。启动可靠性 FAIL 成立。总线忙不能单凭日志断言是前一次崩溃导致，故记录为现象。
2. 源码 audio_service.cc:285 起的 AudioTesting 分支直接 ReadAudioData 并取左声道，送 testing 编码队列后 continue，绕过 AFE。可闻回放不证明 AFE 正常、更不证明参考通道正确。
3. afe_audio_engine.cc:438 的 HandleVoiceResult 受 kVoiceProcessingEnabled 控制。wake-only 不产生 VAD 回调；仅接 on_vad_change 不具备原提案设想的观测能力。VAD 变化也不代表 WakeNet 电平、音质、AEC、阈值均正确。
4. 唤醒会同时清 AFE 和 AudioService 启用位，旧 M0 不恢复，这个缺陷成立。wake=0 可能来自主动停用/测试/资源失败等，不能单独推出命中；wake=1 的离散采样也不能证明期间无命中。正事件使用 WAKE_DETECTED。
5. 矩阵混入候选 02/03/04：日志 ELF 分别为 9c60defdc、277bfc614、180ba8dce，不能合并成 04 验收。历史矩阵加醒目标记，未删除原证据。
6. hw_matrix.py 默认把用户确认设为 True，网络只凭 connected 文本判 IP 通过，Flash 写入校验代替 PSRAM 证据，uptime 被称为捕获持续时间；均修正。增加 ANSI 清理、ELF 不一致拒绝、BOOT_BLOCKED 判 FAIL。无 ELF 的中段捕获仍需通过外部采集记录绑定候选，不能独立视为已绑定；旧片段不用于新候选验收。
7. 隐藏 SSID 问题保留独立待修项；本轮不改 managed component。主机学习业务 79 项测试不能等价替代 M1 NAS 原生语音 20 轮验收，也不能据此排除后端全部故障。

## 实现

- TCA9555 probe/read/write：每项最多三次，每次传输超时 100ms，失败间执行 i2c_master_bus_reset 和 100/200ms 退避。最终失败打印 BOOT_BLOCKED 并每 5 秒睡眠报告，不继续未知电源状态下的初始化、不调用重启。仅解决明确的 expander 通路，未声称所有硬件错误均可恢复。需后续故障注入和重复启动验证。
- M0 唤醒计数、命中后 1 秒重新启用；保留上游状态机/AFE 原码。测试状态改为非阻塞 Recording/Playback，HEALTH 使用单调时间，按钮忙期间点击合并/忽略，不排队反复录音。
- M0 音频两路每秒聚合 n/rms/peak/clipped/raw_peak/read_failures，只有数值，不记录原始音频、不上传。保持 >>12 原换算不变；数据用于下一轮确定对齐、削顶、通道与 AEC 问题。
- VAD 回调接入，同时显式 VAD_OBSERVABLE=0，防止把 wake-only 无事件当成输入静音。
- 启动打印完整 CANDIDATE_ELF_SHA256，用于新捕获与固件绑定。

## 验证与阻碍

原工具测试 40/40，本轮加入 5 项判定回归，45/45 PASS。git diff --check 通过。

build-07 三个修改的 C++ 编译单元生成成功，在链接阶段无法启动 collect2.exe。只读核实该文件存在，直接 --version 明确返回“应用程序控制策略已阻止此文件”。build-08 重试仍失败。没有更换路径/改名绕过策略，没有关闭安全机制。

被拦截文件：E:/workbuddy/claw4-v6/toolchains/idf61/tools/riscv32-esp-elf/esp-15.2.0_20251204/riscv32-esp-elf/libexec/gcc/riscv32-esp-elf/15.2.0/collect2.exe。

没有新的完整 ELF/候选冻结/Flash 操作。旧设备候选与完整备份不受影响。构建目录里的旧 bin 不能当作本轮产物。

## 后续顺序

1. 用户/管理员审查 Windows 应用控制记录并允许可信工具链运行后，重建、冻结新候选、核对全部产物变化，仅写必要区域。
2. 独立候选日志：重复启动 20 轮（零 abort、零 BOOT_BLOCKED、每轮 BOOT_READY），另需验证可恢复 I2C 超时和最终失败策略；连续短测不能替代长稳。
3. 用户提供唤醒刺激，先看双通道数据及 WAKE_DETECTED/WAKE_REARM，再决定增益/参考通道/AEC；不先调整阈值。
4. 隐藏 SSID 回退独立补丁和合成测试；再收口 M0 SD/Camera/电源键和恢复验证。M1 仍需 M0 门禁。

## 取证规矩裁定

采纳原计划的观测口、源码核实、窗口覆盖超时、独立复核和绝对路径原则。跨捕获只允许同候选、同事实口径，最大值仅适用于特定计数/峰值，不能用于状态、长期稳定性或成功率；uptime 不等于捕获时长。沙箱/代理错误仅说明那次环境限制，不能概括为所有 curl/socket/ping 无法提供局域网证据。先前用户已授权普通刷机测试，本轮无需重复授权；未执行恢复写回，也未更改永久安全配置。
