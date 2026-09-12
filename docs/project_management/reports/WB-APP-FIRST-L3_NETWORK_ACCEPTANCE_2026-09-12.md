# WB-APP-FIRST-L3 — 设备 Backend 真机链路验收报告（L3 NETWORK 收口）

> 任务 ID：`WB-APP-FIRST-L3-ACCEPTANCE`
> 工作分支：`workbuddy/app-first-l3-acceptance`（基线 `codex/app-first-mvp-loop` @ `8bac329`）
> 日期：2026-09-12
> 设备：Claw4 / ESP32-P4 rev v1.3 / COM7 / MAC `80:f1:b2:d2:ed:14` / 设备 IP `192.168.3.49`
> 主机：Windows / WLAN `192.168.3.26`
> 待验收任务：`CODEX-APP-FIRST-002` P6「一次性真机验收」
> 验收决策：用户（按 2026-09-03 约定，取消 Codex 复检环节）

---

## 1. 结论摘要

| 项 | 结论 |
| --- | --- |
| P6 六项验收 | **6/6 通过**（含离线积压与恢复补传） |
| 端到端链路 | `设备 192.168.3.49 → relay 192.168.3.26:18765 → backend 127.0.0.1:8000` **PASS** |
| 事件一致性 | `accepted=20 / duplicates=0 / rejected=0 / conflict=0 / 非 accepted=0` |
| TASK_BOARD 6.28 `BLOCKED` | **解除**（原判定的「Windows 防火墙拦截」为误判） |
| 新发现问题 | 3 项，其中 **1 项为设备固件陈旧对象导致的真实缺陷**（见 §5） |
| 设备固件冻结 | 与 `E:\workbuddy\claw4-idf-cold-c5-20260906\xiaozhi.bin` **字节一致**；与 002 报告记录的候选**不一致**（见 §4） |

> **L3 NETWORK 阶段判据达成**：设备已完成 `challenge → auth → today → events/ACK` 全链闭环，
> 家长侧可读到会话记录与看板数据。

---

## 2. 环境与链路拓扑

```
Claw4 (192.168.3.49)
   │  HTTP  POST /api/v1/devices/challenge | /auth | /events/batch
   │        GET  /api/v1/children/child-1/tasks/today
   ▼  TCP 18765
LAN relay  (192.168.3.26:18765)   tools/dev/run-device-backend-relay.py
   ▼  仅转发 /api/v1/*
Backend    (127.0.0.1:8000)       backend/app/main.py (uvicorn)
   ▼
SQLite     backend/.claw4_host_mvp.db
```

- Backend 代码取 `claw4-l1-ready/backend`（app-first 分支，比旧 host-sync 分支多 67 行设备端点）。
- Relay 绑定 `192.168.3.26`，仅转发 `/api/v1/*`，不修改 Backend 回环监听门禁。
- 串口捕获：COM7 @115200，抗重枚举脚本（见 §6.1）。

---

## 3. P6 验收清单逐项自检

| # | 验收项 | 结果 | 证据 |
| --- | --- | --- | --- |
| 1 | Learning 显示候选 build / diagnostics | PASS | 用户读屏：`build app-first`、`pending` / `ACK` / `网络` / `auth` 随链路实时变化 |
| 2 | 在线拉取今日任务 | PASS | `GET /api/v1/children/child-1/tasks/today → 200`；屏侧任务标题「完成练习册 P32」 |
| 3 | **断网** Start→Pause→Resume→Complete | PASS | 停 relay 期间 4 次触控全部受理（见 §3.1） |
| 4 | 重启后任务状态与 pending 保留 | PASS | `LearningRt: boot with committed state: tasks=1 pending=0 active=1` |
| 5 | 恢复网络后 pending 归零、ACK 前进 | PASS | `pending 5 → 0`、`ACK 15 → 20`、全部 `accepted`（见 §3.2） |
| 6 | PWA 仅一条本次学习记录 | PASS | 家长侧 API 返回 2 条 `completed`（1 条 demo seed + 1 条本次真机会话），本次记录唯一、无重复（见 §3.3） |

### 3.1 离线场景（P6-3）— 串口证据

停用 relay 构造真离线（设备侧 TCP 连接失败，非超时）：

```
I (133291) LearningRt: boot with committed state: tasks=1 pending=0 active=1
I (139326) LearningScreen: load: learning_screen (real state)
I (141452) LearningNvs: save blob=388 bytes set=0 commit=0
I (141453) LearningScreen: touch kind=2 task=4f6f26c8-…-49bbc9cbd89d -> status=0 intent=0
I (145328) LearningNvs: save blob=514 bytes set=0 commit=0
I (145328) LearningScreen: touch kind=3 task=4f6f26c8-…-49bbc9cbd89d -> status=0 intent=0
I (149343) LearningNvs: save blob=641 bytes set=0 commit=0
I (149344) LearningScreen: touch kind=2 task=4f6f26c8-…-49bbc9cbd89d -> status=0 intent=0
I (150740) LearningNvs: save blob=839 bytes set=0 commit=0
I (150741) LearningScreen: touch kind=4 task=4f6f26c8-…-49bbc9cbd89d -> status=0 intent=0
E (151346) EspTcp: Failed to connect to 192.168.3.26:18765, code=0x71
E (172346) EspTcp: Failed to connect to 192.168.3.26:18765, code=0x71
E (193346) EspTcp: Failed to connect to 192.168.3.26:18765, code=0x71
```

- `status=0`＝受理成功，`intent=0`＝Accepted；**离线态 4 次交互全部成功，无卡死、无拒绝**。
- 每次交互均伴随 `save blob=` 成功落 NVS，blob 单调增长 `262 → 388 → 514 → 641 → 839` 字节
  （≈127 B/事件的 outbox 追加），**证明事务性 outbox 已持久化**。
- 失败码 `0x71` = 113 = `EHOSTUNREACH`，**每 21000 ms 一轮**（connect 约 6 s + 15 s 退避），持续重试不丢队列。

### 3.2 恢复网络补传（P6-5）— 后端证据

恢复 relay 后，设备下一轮 cycle 一次性补传：

```
I (217582) LearningNvs: save blob=839 bytes set=0 commit=0
I (217599) HttpClient: Established new connection to 192.168.3.26:18765
I (217790) LearningNvs: save blob=135 bytes set=0 commit=0      ← 队列清空
I (232806/247866/262911/277952/293005) … 每 15 s 一轮稳定轮询
```

后端落库（`delivered` 顺序即序列）：

| sequence | type | state |
| --- | --- | --- |
| 16 | `task.paused` | accepted |
| 17 | `task.resumed` | accepted |
| 18 | `task.paused` | accepted |
| 19 | `task.completed` | accepted |
| 20 | `study.session.completed` | accepted |

聚合计数：

```
events       15 → 20   （+5，与屏侧 pending 5 精确吻合）
challenges    3 → 4    （恢复后重新 challenge/auth）
非 accepted   0
duplicates    0 / rejected 0 / conflict 0
```

### 3.3 家长侧（P6-6）

`GET /api/v1/parents/me/children/child-1/study-sessions`（`Bearer mock-parent-token-1`）：

```json
[{"session_id":"sess-055867ca4df6b890","task_id":"4f6f26c8-…","task_title":"完成练习册 P32",
  "status":"completed","completion_type":"manual","actual_seconds":12,"xp":0},
 {"session_id":"sess-250f2c37e61888c9","task_id":"demo-math-001",
  "status":"completed","completion_type":"manual","actual_seconds":983,"xp":16}]
```

`GET /api/v1/parents/me/dashboard` → `{"date":"2026-09-12","planned_tasks":1,"completed_tasks":1,"completion_rate":100}`

- 本次真机会话**唯一**，无重复落库（重放/重复 ACK 幂等成立）。
- 第 2 条为 demo seed 演示记录，非本次产生。
- ⚠️ `actual_seconds=12` / `xp=0` 与真实学习时长不符 —— 见 §5.3。

---

## 4. 设备固件冻结（只读回读，不写不擦）

用 esptool 只读方式回读设备 `ota_0 @ 0x200000` 前 8 KiB 的 app 描述符，与本地候选逐字节比对：

| 对象 | 大小 (B) | 首 8 KiB SHA-256 前缀 | app_desc `elf_sha256` | 构建时间 |
| --- | --- | --- | --- | --- |
| **设备（回读）** | — | `2ba416689671d348ed90b288` | `7b0944d593581a9332519b9c6152a766024a9ada3a37a60c4cc47bc979319a58` | 18:42:40 Sep 6 2026 |
| `claw4-idf-cold-c5-20260906\xiaozhi.bin` | 9,255,536 | `2ba416689671d348ed90b288` | 同上（**完全一致**） | 18:42:40 Sep 6 2026 |
| `E:\b\xiaozhi.bin`（旧） | 9,175,856 | `39f77bb8a429ac5dd66ce20b` | `76b7a7a5…` | 22:06:58 Sep 3 2026 |

**结论**：设备实际运行固件 = `E:\workbuddy\claw4-idf-cold-c5-20260906\xiaozhi.bin`。

> ⚠️ **与既有文档不符**：`CODEX_APP_FIRST_002_P1P2/P3_P6` 报告记录已刷候选为
> `9,249,600 B / SHA-256 682FFBCB58A4F955340A100B2E76AE9BE1049919C25390F675294D4AF1075C44`，
> 与实机不符。该 682FFBCB 产物当前**不在本地任何镜像目录**中，无法复核。
> 本报告以**实机回读**为唯一事实源，建议在 TASK_BOARD 将旧记录的候选 SHA 标注 `SUPERSEDED`。

---

## 5. 新发现的问题（本轮实测暴露，均未在本阶段修复）

### 5.1 【缺陷】`study.session.completed` 未携带 `pause_count` → 家长端暂停次数恒为 0

**现象**：后端 `study_sessions.pause_count` 恒为 `0`，尽管离线期实测有 2 次 `task.paused`
（seq 16、18）。事件 payload 实收：

```json
{"actual_seconds":"12","completion_type":"manual",
 "session_id":"sess-…","task_id":"4f6f26c8-…"}          ← 无 pause_count
```

**排查链（四步排除，逐步收敛）**：

1. 后端无字段白名单 —— `schemas.py:68` `payload: Dict[str, Any]`，`main.py:375` `store_event(..., payload=dict(ev.payload))` 原样落库。
2. 设备 outbox codec 无字段白名单 —— `outbox_codec.cpp:148` `PayloadToStr(p.payload)` 遍历整个 map。
3. wire codec 无字段白名单 —— `wire_codec.cpp:401-410` 遍历 `event.payload` 全部键值。
4. 仓库源码**确实**会发送该字段 —— `firmware/main/learning_domain/reducer.cpp:189-190`：
   ```cpp
   {"actual_seconds", std::to_string(done.actual_seconds)},
   {"pause_count",    std::to_string(done.pause_count)}}));
   ```
   且 `integration/metalio_claw4/...` 镜像副本与仓库 blob **逐字节一致**（仅行尾 CRLF 差异）。

**根因（已在固件二进制层证实）**：

对设备同版本固件 `xiaozhi.bin` 做字符串检索：

| 字面量 | 结果 |
| --- | --- |
| `actual_seconds` | FOUND |
| `completion_type` | FOUND |
| `learning_cfg`（20:30 provisioning） | FOUND |
| **`pause_count`** | **MISSING** |

→ 设备固件里的 reducer **根本没有编译进 `pause_count`**。

再看构建目录的目标文件时间，根因明确：

| 目标文件 | 编译时间 | 说明 |
| --- | --- | --- |
| `learning_domain/reducer.cpp.obj` | **Sep 6 18:44** | 之后**再未重编** |
| `application/coordinator.cpp.obj` | Sep 6 19:36 | |
| `…/nvs_backend_provisioning.cpp.obj` | Sep 6 20:27 | 20:30 新增文件，正常编译 |

**结论：增量构建复用了 18:44 的陈旧 `reducer.obj`。**镜像同步沿用源文件原始 mtime
（`reducer.cpp` mtime 为 `Sep 6 09:27`，早于 `.obj` 的 18:44），ninja 判定「已是最新」而跳过重编；
而 20:30 之后新增的源文件（`nvs_backend_provisioning.cpp` 等）因为是新文件才被编译进去 ——
这完美解释了「设备有 provisioning/worker 功能、却缺 `pause_count`」这一矛盾现象。

**影响**：家长看板「暂停次数」永远为 0；`pause_seconds` 亦无处计算。
**建议**：清洁重建（或 `touch` 全部 learning 源文件后重建）→ 复核二进制含 `pause_count`
→ 重新 app-flash（需新批次授权）。

### 5.2 【设计基线】设备事件时间戳仍为「开机秒」，不是 epoch

`integration/metalio_claw4/device/ports/learning_clock.cpp`：

```cpp
int64_t EspTimerClock::epochSeconds() {
  // Unsynchronized: seconds since boot. L2 SNTP will switch this to the RTC.
  return esp_timer_get_time() / 1000000LL;
}
```

实测后果（`study_sessions` 行）：

```
started_at = 1692.0    finished_at = 149.0     ← finished < started
```

后端 `store.py:443/475` 直接 `float(ev.timestamp)` 落库，因此**家长端会话起止时间、按本地日归属、
`focus_minutes` 全部失真**（本次 `focus_minutes=0` 即由此而来）。

- 这是**已文档化的待办**（注释明确写「L2 SNTP will switch this to the RTC」），非回归缺陷。
- 建议：在 L4 之前接入 SNTP；或过渡期由后端以 `received_at` 兜底归一化，并在前端标注「设备本地时间」。

### 5.3 【健壮性】硬复位/断电会丢失当前 running segment 的时长

本次会话真实累计约 28 分钟，但落库 `actual_seconds=12`。
原因：`reducer.cpp:22` 的 `actual_seconds` 只在**状态迁移时**结算，
`learning_runtime.cpp:184` 的 `prepareAfterBoot` 在重启后把 `segment_start_monotonic_ms`
重新锚定到新 monotonic，因此**复位前未提交的 running 段无法找回**。

- 影响：断电/看门狗复位会低估专注时长与 XP（`xp = actual_seconds // 60`）。
- 建议：增加周期性（如每 30–60 s）`persistTransition` 结算，或改用可跨重启的单调基准。
- 说明：本次复位由我方的串口探测（pyserial 打开 COM7 拉 DTR）触发，属**测试副作用**，
  但也真实复现了「异常掉电」这一类场景。

---

## 6. 复现手册

### 6.1 串口可靠捕获

设备为 USB-Serial/JTAG（CDC）。pyserial 裸读在重枚举后会 `ClearCommError failed (winerror 22)`，
需用**遇错重开**的捕获器（`com7_run.py`：`list_ports` 按 `303A:1001` 定位 → 读失败即重开）。
注意：**打开 COM7 会拉 DTR 并复位 ESP32-P4**，取证时避免反复开关端口。

### 6.2 关键判据速查

| 场景 | 判据 |
| --- | --- |
| 设备连不上主机 | 设备侧 `0x71`(EHOSTUNREACH) **快速失败** + 主机 `netstat` 无记录 ⇒ 先查**路由/回程**，不是防火墙 |
| 本机装 Tailscale/VPN 时 | 禁用「ping 通＝局域网可达」；用 `tracert -d <ip>` 看首跳、`ping -S <本机LAN IP> <ip>` |
| 连接是否到达主机 | `netstat -ano -p TCP` 1 s 轮询记录 `SYN_SENT/ESTABLISHED/TIME_WAIT` 变化 |
| 固件是否真为某候选 | esptool 只读回读 `0x200000` 头部 8 KiB，比对 app_desc `elf_sha256` |
| 固件是否含某功能 | 直接在 `.bin` 里检索字面量（如 `pause_count`），比读屏/日志更硬 |

### 6.3 已知环境坑

- 本机装有 **Tailscale**，其 exit node + 子网路由广播会劫持 `192.168.3.0/24`；
  真机联调须 `tailscale down`，或 `tailscale set --exit-node-allow-lan-access=true`。
- Claw4 的 USB CDC 会偶发掉线（`CM_PROB_PHANTOM`），重插或断电重启即恢复。
- 设备 I2C 总线存在独立基线故障：偶发锁死（GT911/TCA95xx/BQ27220/LCD 同时报
  `i2c transaction failed`），**软复位清不掉，必须断电**。

---

## 7. 未解决问题与风险

| ID | 类型 | 说明 | 处置建议 |
| --- | --- | --- | --- |
| L3-F1 | 缺陷 | 陈旧 `reducer.obj` → `pause_count` 缺失 | 清洁重建 + 重新 app-flash（新批次授权） |
| L3-F2 | 基线待办 | 设备时间戳＝开机秒，家长端时间失真 | L4 前接 SNTP，或后端以 `received_at` 兜底 |
| L3-F3 | 健壮性 | 异常复位丢 running 段时长 | 周期化提交结算 |
| L3-R1 | 文档 | 002 报告候选 SHA（682FFBCB）与实机不符 | 标注 `SUPERSEDED`，改以实机回读为准 |
| L3-R2 | 硬件 | I2C 偶发锁死（与网络无关的独立风险） | 列 `HARDWARE_VERIFY_REQUIRED`，观察复现频率 |
| L3-R3 | 构建 | 镜像同步沿用源文件旧 mtime → ninja 漏编 | 构建脚本加 `touch` / 强制 clean / 校验关键字面量 |

---

## 8. 与既有文档的差异

| 文档 | 原记录 | 本报告实测 | 处理 |
| --- | --- | --- | --- |
| `CODEX_APP_FIRST_002_P3_P6_2026-09-06.md` §2 | 已刷候选 `9,249,600 B / 682FFBCB…` | 实机回读 = `9,255,536 B / elf 7b0944d5…`（= `claw4-idf-cold-c5-20260906` 产物） | 旧候选 SHA 建议标 `SUPERSEDED` |
| `CODEX_APP_FIRST_002_P3_P6_2026-09-06.md` §5 下一步 | 「管理员放行防火墙后复测」 | 防火墙规则始终正确，真因是 **Tailscale 劫持局域网路由** | 已在本报告 §6.2 记录正确判据 |
| TASK_BOARD 6.28 | `BLOCKED`（防火墙） | 阻塞不存在，链路已闭环 | 本报告同步更新看板 |

---

## 9. 范围与合规声明

- 本阶段**只做只读回读**（`esptool read_flash 0x200000 0x2000`），**未写、未擦、未改分区**；
  未触碰 bootloader / partition table / ota_1 / C5 / eFuse / Secure Boot / Flash Encryption。
- 未修改任何产品代码；本报告所在分支仅新增文档。
- 未提交：密钥、token、nonce、NVS dump、固件二进制、构建目录、儿童原始数据。
- 未发生范围偏差。

---

## 10. 建议后续动作（按优先级）

1. **【P0】清洁重建 + 重新冻结候选**：`touch` learning 源文件或 clean 构建 →
   校验新 `.bin` 含 `pause_count` 字面量 → 记录新 SHA-256 为唯一候选。
2. **【P0】app-flash 新候选**（`ota_0 @ 0x200000`，application-only，**需新批次授权**）→
   复测离线/重启/补传三场景，确认 `pause_count` 与 `actual_seconds` 均正确。
3. **【P1】接入 SNTP** 或在后端以 `received_at` 归一化会话时间轴。
4. **【P1】运行段周期化提交**，消除异常复位导致的时长丢失。
5. **【P2】看板 6.28 解除阻塞**，`CODEX-APP-FIRST-002` 置为真机闭环达成；同步更新 §8 差异项。
6. **【P2】推进 L4（语音/唤醒词）** —— 需新任务包与授权（002 任务包明确排除 Voice/STT/MCP/AI）。
