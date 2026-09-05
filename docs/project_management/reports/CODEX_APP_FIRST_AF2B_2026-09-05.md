# CODEX-APP-FIRST-001 AF2b 报告：Backend/PWA exactly-once 重复矩阵

- 任务：`CODEX-APP-FIRST-001 / AF2b`
- 分支：`codex/app-first-mvp-loop`
- 范围：本地 throwaway SQLite + FastAPI TestClient；C++ wire fixture 生成所有设备事件 JSON；未连接真机。

## 验证场景

`tools/dev/verify-cpp-backend-exactly-once.py --rounds 10`：

1. 注册、配对、challenge/auth；
2. 家长创建今日任务；
3. C++ 生成 `study.session.started`、`study.session.completed`、`task.completed`；
4. 每个相同 event_id/sequence 重复提交 10 次；
5. 用 C++ decoder 解析每个 ACK；
6. 通过家长 study-sessions 和设备 today API 核对最终投影。

结果：`AF2B_EXACTLY_ONCE=PASS rounds=10 accepted=3 duplicates=27 sessions=1 task_status=completed`。说明三条业务事实各只落库一次，PWA 侧只看到一个 completed session。

## 限定结论

- Backend 幂等、连续 ACK、事件投影和 PWA exactly-once 已有真实跨语言证据。
- AF2 尚未全部收口：还需把认证暂停、存储提交失败、ACK 缺口与连续进程重启纳入同一批次报告；这些属于下一步 AF2c。
- 真机仍不需要连接。
