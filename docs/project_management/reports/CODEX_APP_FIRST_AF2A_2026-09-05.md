# CODEX-APP-FIRST-001 AF2a 报告：Virtual Device 离线/重启/重复收敛

- 任务：`CODEX-APP-FIRST-001 / AF2a`
- 分支：`codex/app-first-mvp-loop`
- 实现范围：真实 `LearningApp` + `AppCoordinator` + `FakeDisk`，仅替换网络端口；未连接真机。

## 交付

- `firmware/tests/host/virtual_device_app.cpp`
  - 增加确定性 `RunnerTransport` 和 JSONL 命令 `network offline|accept|duplicate|lost`、`sync`。
  - 网络故障不复制业务状态机，所有状态仍由真实 coordinator/outbox 产生。
- `tools/dev/verify-virtual-device-fault-matrix.ps1`
  - 固定执行断网、请求/响应丢失、重复响应、进程重启和后续完成。

## 验证

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/dev/verify-virtual-device-fault-matrix.ps1 `
  -CompilerPath E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe `
  -OutputDir E:\workbuddy\学习习惯培育AI\out\app-first-af2-fault
```

结果：`AF2` summary `passed=true`；断网后 pending 保持 `2`，重启恢复 active `demo-math-001`，重复 ACK 后最终 pending `0`。AF2a 只覆盖 C++ App/Host 故障子集；认证暂停、存储提交失败已有 coordinator host tests，PWA exactly-once、多轮重复和真实 relay 仍属于 AF2b/AF2c。
