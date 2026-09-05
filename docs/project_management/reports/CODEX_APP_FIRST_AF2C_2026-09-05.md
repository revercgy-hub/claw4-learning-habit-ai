# CODEX-APP-FIRST-001 AF2c 报告：认证/存储/ACK 故障门禁

- 任务：`CODEX-APP-FIRST-001 / AF2c`
- 分支：`codex/app-first-mvp-loop`
- 范围：纯 C++ host coordinator/outbox；没有真机、真实 NVS 或 Flash 操作。

## 覆盖项

现有 `coordinator_tests` 与新增 `backend_sync_transport_tests` 联合覆盖：

- Auth 401/403：单次 re-auth；失败后 `PausedAuth` 短路，pending 不变；显式 reset 后恢复发送。
- 网络/5xx：确定性退避增长并封顶，pending 不删除。
- 业务拒绝/dead-letter：原事件保留；dead-letter 持久化失败时 ACK cleanup 整批中止，恢复后可重试。
- Outbox commit failure：UI/domain 不发布未提交状态；retry 复用相同 event_id。
- duplicate/lost response：重启后原 pending 使用相同事件重发并收敛。
- ACK/Conflict/Gap：只清理 consecutive prefix，非连续或冲突事件保持可重放。

## 验证命令与结果

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/dev/verify-host-cpp-tests.ps1 `
  -CompilerPath E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe `
  -CrossCompilerPath E:\workbuddy\claw4-idf-tools\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-g++.exe `
  -OutputDir E:\workbuddy\学习习惯培育AI\out\app-first-af2-seam2
```

结果：native unit `14/14 PASS`，其中 `backend_sync_transport_tests: all PASS`；interface cross-check exit `0`；禁止依赖扫描 PASS。AF2a/AF2b 的 Virtual Device 与 10 轮真实 Backend/PWA exactly-once 证据分别见对应报告。

## 阶段结论

AF2 的主机故障门禁通过，尚未声明 `APP_FIRST_MVP_LOOP=PASS`。下一步 AF3 只做设备薄适配与 IDF BUILD ONLY；仍不 app-flash、不擦除、不改分区。只有 AF4 全链验收通过后才通知用户连接设备。
