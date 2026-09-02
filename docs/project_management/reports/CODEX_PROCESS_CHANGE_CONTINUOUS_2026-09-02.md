# Codex 调度变更记录：连续开发与异步修复

- 日期：2026-09-02
- 授权来源：用户明确要求“由 WorkBuddy 持续开发，Codex 后续进行修复”
- 结论：启用一个活动连续工作流；取消普通任务逐项等待 Codex 验收的要求
- 首个工作流：`WB-STREAM-001`
- 分支：`workbuddy/mvp-core-stream`

## 变更内容

1. WorkBuddy 按预授权 checkpoint 连续开发，每个 checkpoint 独立提交和 push。
2. WorkBuddy 推送后无需等待，可继续下一个 `QUEUED` checkpoint。
3. Codex 按远端不可变提交异步复检，并获授权在独立 `codex/` 分支直接修复普通产品缺陷和补测。
4. `main` 继续只接收验收后的提交，WorkBuddy 无权自行合并。
5. Stage 1 未完成时允许纯主机接口、Mock Backend 和自动化测试提前开发，但不允许真机耦合代码或发布声明。

## 未放宽的门禁

- Flash 读取、擦除或刷写；
- partition table、Bootloader、Secure Boot、Flash Encryption、OTA 策略；
- 官方 Metalio 基线修改；
- 儿童真实数据、真实凭据、付费或生产外部服务；
- 摄像头/语音/AI/4G/GPS 等非当前 MVP 核心范围。

## 首批连续 checkpoint

- CP0：收敛 WB-002 的重复投递、challenge、claim 和重启语义。
- CP1：建立纯接口骨架并执行 P4 交叉编译契约检查。
- CP2：建立 FastAPI 内存 Mock Backend 及身份/事件同步自动化测试。

纯 C++ 领域实现暂未进入本工作流：当前主机已确认 P4 交叉编译器可用，但没有本机 C++ 编译器，不能把“只编译未运行”冒充为主机单元测试通过。后续由 Codex 补齐可运行测试环境或在安全的设备/仿真测试方案确定后再调度。
