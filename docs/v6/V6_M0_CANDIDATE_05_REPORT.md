# M0 候选 05：恢复构建与启动/音频诊断实测

2026-09-22。源码 2831d0f（含 WorkBuddy e923a48 复检修正），当前候选 m0-candidate-05.json。

用户处理 Windows 智能应用控制后，collect2 已能启动；单独运行提示找不到 ld 是缺少构建环境路径，正式 build_device.py 导出的工具链环境完成 build-09，exit=0。冻结与刷写计划检查均通过。除 factory application 外四个产物哈希与候选 04 相同，因此仅刷 0x200000 应用；flash-m0-05.txt exit=0，写入校验通过。

boot-m0-05.txt 30 秒：BOOT_READY、NETWORK_EVENT=2，音频统计开始产出，无读取失败/削顶。此轮仅是初始启动观察，不作为可靠性总验收。

首次唤醒诊断窗口 wake-instr-05-01.txt 持续 60 秒：左通道 59 个统计窗口、最大 RMS=410、峰值=1904、削顶=0、WAKE_DETECTED=0。未获得同步用户刺激确认，故不能据此计算命中率或判定唤醒失败。参考通道空闲近零，不等价于播放时参考链路正确。

运行时 CANDIDATE_ELF_SHA256 使用 IDF API，其实际配置输出 9 位 ELF 前缀 533d4a15d；与冻结完整 ELF SHA 的前缀核对，不能称为输出了完整哈希。原复检报告中“完整”描述以此更正。

repeat_boot.py 使用每轮 20 秒、20 轮复位，按同一候选 ELF 前缀、一轮一次 app_main/BOOT_READY、无 abort/BOOT_BLOCKED 判定，遇失败停测；每轮保留原始日志哈希和实际耗时。此测试验证 USB 复位序列，不等价于断电冷启动、总线故障注入或长稳 soak。

工具测试 49/49 PASS（包含 4 个重复启动判定回归）。原始日志与完整备份均留在忽略的私密目录，不提交。

重复启动由用户要求移交 WorkBuddy 而停止。结果文件完整记账 10/20 轮，10 轮均一次 BOOT_READY、无 panic/BOOT_BLOCKED、无 I2C 重试；不能宣称 20 轮通过，也未实际触发恢复分支。停止时可能存在尚未计账的尾部捕获，保留但不纳入此统计。脱敏结果：integration/v6/m0-candidate-05-boot-series.json。
