# CODEX-APP-FIRST-001 AF1b 报告：BackendClient transport boundary

- 任务：`CODEX-APP-FIRST-001 / AF1b`
- 分支：`codex/app-first-mvp-loop`
- 实现提交：`9ecd247`（已普通 push 到 origin）
- 范围：C++ 端点客户端与可替换 HTTP 端口；未连接真机、未刷写 Flash。

## 交付

- `firmware/main/sync/http_transport.h`
  - 定义只含 method/url/headers/body/status 的平台无关 `HttpTransport`。
- `firmware/main/sync/backend_client.h/.cpp`
  - 真实端点顺序：challenge → auth → today → events/batch。
  - C++ 只调用 `wire_codec` 生成/解析 JSON；HTTP/TLS/调度由注入端口负责。
  - 统一网络、401/403、业务 4xx、5xx、解析失败分类；认证 token 只保存在 client 内。
- `firmware/tests/unit/sync/backend_client_tests.cpp`
  - 验证请求 URL、方法、Bearer 头、签名字段、批量事件原样 body、ACK 解析及错误分类。

## 验证

命令输出：`E:\workbuddy\学习习惯培育AI\out\app-first-af1b-host\host_result.txt`

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/dev/verify-host-cpp-tests.ps1 `
  -CompilerPath E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe `
  -CrossCompilerPath E:\workbuddy\claw4-idf-tools\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin\riscv32-esp-elf-g++.exe `
  -OutputDir E:\workbuddy\学习习惯培育AI\out\app-first-af1b-host
```

结果：native unit `13/13 PASS`（新增 `backend_client_tests: all PASS`）；interface cross-check exit `0`；禁止依赖扫描 PASS。

## 边界与下一步

该 checkpoint 锁定了真实 Backend 的 URL/headers/JSON 语义，但尚未把真实 C++ runner 接入 HTTP relay，也未声明跨语言 E2E 通过。下一步 AF1c 将实现 JSONL relay runner：C++ 生成请求和事件 JSON，Python 只负责原样转发给现有 Backend 并返回原始 response，再接入 PWA 恰好一次验收。真机仍不需要连接。
