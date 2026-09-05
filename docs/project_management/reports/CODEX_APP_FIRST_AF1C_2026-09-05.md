# CODEX-APP-FIRST-001 AF1c 报告：C++ wire ↔ Backend 跨语言闭环

- 任务：`CODEX-APP-FIRST-001 / AF1c`
- 分支：`codex/app-first-mvp-loop`
- 当前状态：`CHECKPOINT_READY`。
- 实现提交：`18ff69f`（已普通 push 到 `origin/codex/app-first-mvp-loop`）。
- 范围：本地 throwaway SQLite + FastAPI TestClient；没有公网监听、真实账号或真机。

## 证据链

新增：

- `firmware/tests/host/backend_wire_fixture.cpp`
  - 只使用产品 `wire_codec` 生成 challenge/auth/batch JSON；把 Backend Today/ACK response 重新交给产品 decoder。
- `tools/dev/verify-cpp-backend-wire.py`
  - 创建临时 Backend 数据库，注册/配对设备，家长创建任务，调用真实 challenge/auth/today/events/batch endpoint。
  - Python 仅负责本地 HTTP 调用与测试签名；不构造或改写设备事件 JSON。

命令：

```powershell
$env:Path = "E:\workbuddy\toolchains\w64devkit-2.9.1\bin;" + $env:Path
& E:\workbuddy\学习习惯培育AI\backend\.venv\Scripts\python.exe `
  tools/dev/verify-cpp-backend-wire.py `
  --repo E:\workbuddy\claw4-l1-ready `
  --compiler E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe `
  --out-dir E:\workbuddy\学习习惯培育AI\out\app-first-af1c
```

结果：`AF1C_CPP_BACKEND_WIRE=PASS`；C++ decoder 解析 Today；第一批事件 `ACK 1 1`；同一 event_id/sequence 原样重发后 Backend 返回 `duplicates=1`、ACK 仍为 1。

## 限定结论

- Auth challenge/auth、Today、Events/ACK 的字段与真实 Backend schema 已经跨语言锁定。
- 这是 wire/endpoint contract loop，不是完整 LearningApp HTTP runner；AF2 仍需把真实 coordinator/outbox 接到网络端口，并完成断网、重启、恢复和 PWA exactly-once 矩阵。
- 真机仍不需要连接；AF0~AF4 未完成前禁止进入设备验收。
