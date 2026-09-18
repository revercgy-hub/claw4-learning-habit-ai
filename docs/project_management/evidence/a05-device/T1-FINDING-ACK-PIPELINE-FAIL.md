# A05-DEVICE-T1 — 现场发现（已定根因，wire 级证据）：ACK 收到但设备永不清理 outbox（`ACK_PIPELINE_FAIL`）

**时间**：2026-09-18 19:41–20:17 (+08:00)
**Candidate**：`claw4-v53-m0-a05-2c8f58f`（`c035e1c09ebe472f5f14b490c93844aa3e298cd11b4e30f7fe1055e5b9278d36`）
**基线提交**：`d5e64c5`（远端 `workbuddy/a05-build-m0`）
**本轮改动**：**无**（未改源码 / CMake / sdkconfig / partition / 设备 NVS；未 rebuild；未 reflash）
**脱敏**：不含 `device_secret` / token / Wi-Fi 密码（捕获器**只记录请求摘要与响应体**，从不记录 `Authorization`）
**状态**：根因已用 **wire 级端到端证据 + Host 侧判据**证实（不是推断）

---

## 1. 环境（T1 §三/§四）已具备

| 项 | 实测 |
|---|---|
| 测试主机 IP | **192.168.3.26/24**（WLAN，网关 192.168.3.1，AP `505`） |
| 防火墙 | 已有入站放行 `Claw4 Device Backend Relay 18765`（Private + Public） |
| 后端 | `E:\workbuddy\claw4-l1-ready\backend`（uvicorn **127.0.0.1:8000**）；起始库 sha256 `90793ef0…`（已本地备份 `.pre-T1-20260918.bak`） |
| relay（正式） | 本仓库 `tools/dev/run-device-backend-relay.py` → **192.168.3.26:18765 LISTEN** |
| relay（取证用） | `E:\workbuddy\_t1_logging_relay.py`（**本地临时**，与仓库脚本同转发语义，额外记录 `/api/v1/events/batch` 的**请求摘要**与**响应体**；不记录凭据） |
| 依赖 | 复用现成 venv（Py3.13.14 / fastapi 0.141.1 / uvicorn 0.52.4 / sqlalchemy 2.0.52，与 `requirements.txt` 锁定值一致），未装任何包 |

> 注意：**ICMP 扫 /24 找不到设备**（只有 10 台存活、无 Espressif OUI），但设备一直在网上。不要用扫描法判断设备是否在线。

## 2. 现象（服务端日志 + wire 双证，权威）

起服务后设备自动完成 `challenge → auth → tasks/today → events/batch`，每 ~15 s 一轮：

```
batch: accepted=7 duplicates=0 rejected=13 gaps=0 ack=20     <- 第 1 轮（服务端 ack 13 -> 20）
batch: accepted=0 duplicates=7 rejected=13 gaps=0 ack=20     <- 其后每轮逐字相同（连续 >= 12 轮）
```

**设备端实际发出的字节**（取到 9 条完整捕获，`request_summary` 全部一致）：

```
last_acked_sequence = 0                         <- 设备自己的本地 ACK，从未推进
events = 20 条，sequence = [1..20]，event_id 完全相同，逐轮不变
types  = task.started / study.session.started / task.completed / study.session.completed
         / task.paused / task.resumed ... （5 个会话的事件序列）
```

**服务端实际返回的字节**（设备收到的原样 body，3364 B）：

```
{"last_acked_sequence":21,"server_time":...,"results":[
   seq 1..13  -> {"status":"conflict","http_status":409,
                  "reason":"sequence_already_used_by_other_event"}   (FIX-08)
   seq 14..20 -> {"status":"duplicate","http_status":200} ],
 "accepted":0,"duplicates":7,"rejected":13,"gaps":0}
```

⇒ 服务端**已成功返回**、并把这 7 条判为 `duplicate`（= 已落库）；设备**一条都不删、也不推进 ACK**，
下一轮**重发完全相同的 20 条**。这就是任务书 §8 的 **`ACK_PIPELINE_FAIL`**。

## 3. 根因（**已证实**，两层都不是猜的）

### 3.1 先否证"解码器有罪"——用**设备实际收到的原样字节**喂设备自己的解码器

- 源码：仓库已跟踪的 `firmware/main/sync/wire_codec.cpp`（与隔离构建树里 LF 归一化后**逐字节相同**，此前 sha 差异只是 CRLF/LF）
- harness：w64devkit g++，`-std=c++17 -I firmware/main`，链接 `wire_codec.cpp`（`E:\workbuddy\_t1_decode_probe.cpp`）
- 输入：从捕获里抽出的**真实响应体**（`_t1_resp_actual.json`，3364 B，未经任何改动）

```
DecodeBatchResponse = TRUE
  error_class = 0   http_status = 200   last_acked = 21   server_time = 1789733565
  results = 20
    seq=1..13   outcome=2 (Conflict)  http=409  eligible_for_ack=no
    seq=14..20  outcome=1 (Duplicate) http=200  eligible_for_ack=YES
```

⇒ **解析路径完全正常**：`last_acked_sequence` / `server_time` / `results[].status` / `eligible_for_ack` 全部正确映射。
（更早一次用"同后端产出的重建响应体"做同类测试，结果一致 ⇒ 两次独立输入都否证了 H1。）

### 3.2 真根因：`scopeBatchToSent()` 的连续前缀走查 + 队首被拒行 = 永久卡死

`learning/application/coordinator.cpp:298 scopeBatchToSent()`：

```cpp
for (const auto& ev : envelope.request.events) {          // 按发送顺序
  const auto it = rows.find(ev.sequence);
  if (it == rows.end()) break;                             // 缺结果
  const sync::PerEventResult& row = it->second;
  if (row.outcome != Accepted && row.outcome != Duplicate) break;   // <-- 本次在此处断
  any_confirmed = true;
  scoped.last_acked_sequence = ev.sequence;
}
if (!any_confirmed) scoped.last_acked_sequence = envelope.request.last_acked_sequence;  // = 0
```

本次队列 `seq=1..20`，服务端对 `seq=1..13` 全部返回 **`conflict`(409)**（这些序号槽位已被**别的 event_id** 占用）：

⇒ **循环第一次就 break** ⇒ `any_confirmed=false` ⇒ `scoped.last_acked_sequence = 0`（设备自己的值）
⇒ `OutboxCore::applyBatchResult` 只清理 `sequence <= 0` 的行 ⇒ **一条都不删**，`NvsOutboxStorage::removeAcked` 不被调用 ⇒ 本地 ACK 停在 0
⇒ 设备状态不变 ⇒ 下一轮**重发完全相同的批次**，livelock 永续。

**`dead_letter=0` 也解释清了**：`applyBatchResult` 只给 `outcome == Rejected && 400<=http<500 && 非 401/403` 打标记；
本次 13 行是 **`Conflict`**（`EventOutcome::Conflict`），不是 `Rejected` ⇒ 不打标记 ⇒ 与 §3.1 的映射完全一致。

**"只有一个发送方"已排除其它可能**：全树 `prepareSync()` 只有 2 处引用（定义 + `sync_executor.cpp:62` 调用），
`request.events.push_back` 只有 `coordinator.cpp:385` 一处，`/api/v1/events/batch` 只有 `backend_client.cpp:109` 一处 ⇒ **单一发送路径**。

### 3.3 一般化后果（这才是真正的产品缺陷）

- **任何"服务端永久拒绝"的行都会把 outbox 永久钉死**：`conflict` 行直接断掉走查；
  即使是 `rejected` 的业务 4xx 行，打上 dead-letter 后**行被保留且不跳过**（`prepareSync` 仍会带上它），
  走查每轮在同一位置断掉 ⇒ **前缀永远无法前进**。
- 卡死位置之后的所有新事件**永远排不到**（`prepareSync` 只从 `last_acked+1` 连续取）。
- 设备 UI 仍显示 **已同步 / 已恢复**（`R|1`，`SyncOutcome::Synced`，因为 `applyBatchResult` 返回 Committed）⇒ **静默失败**。
- 无任何自恢复路径（无跳过 / 驱逐 / 基线重同步）。
- 触发条件：设备持久化序号基线（`A=0`，pending `1..20`）≠ 后端库 ACK 基线（起始 `13`）—— 见 `T1-PRECHECK.md §4`（开测前已预判）。

## 4. 第二个发现：学习运行时不在启动路径上 ⇒ 复位后同步**不自愈**（已实测确认）

- `LearningRuntime::Init()`（内部第 224 行调用 `StartBackendWorker()`，即那个每 15 s 的同步 worker）
  **全树只有一个调用点**：`display/screen/learning_screen/learning_screen.cc:607  rt.Init();`
- `LearningRuntime::Instance()` 也只被学习页 UI 引用；构造函数不启动 worker。
- ⇒ **学习运行时（及其后台同步）只在"学习页被打开过"之后才存在**。
- **实测双向确认**：
  - 我 19:46:57 起连续读 flash（每次读都会复位设备）之后，到 **20:00 后端再无任何请求**（日志 mtime 停在 19:46:48），尽管后端与 relay 一直监听；
  - 你用设备打开学习页后，**3 秒内**设备立即恢复 `challenge → auth → batch`（记录版 relay 当场抓到捕获）。
- 影响：设备重启后（含掉电、OTA 重启），**局域网学习同步不会自愈**，必须有人进学习页。对"每日任务/后台同步"场景是可用性缺陷。

## 5. 设备侧核心证据（§7.2）：`outbox removed = 0`、`last_acked` 未推进

NVS 权威解码（`decode_outbox_blob.py`，2026-09-18 20:16 快照 `_t1f-nvs.bin`）：

```
hdr = T|2  S|0  E|20  Q|21  A|0  F|0  R|1
pending_rows = 20   sequences = [1,2,...,20]      (逐条 event_id 与设备每轮发送的完全一致)
```

| 判据 | 要求 | 实测 | 结论 |
|---|---|---|---|
| `outbox removed` | `> 0` | **0**（仍 20 条 pending） | ✗ |
| `last_acked_sequence` 推进 | 相对测试前推进 | 测试前 `A=0` → 测试后 **`A=0`** | ✗ |
| `Q`（下一个待分配序号） | — | 21（未因清理而变化） | — |
| `dead_letter` | — | 0（符合 §3.2 的 `Conflict` 语义） | — |

⇒ §7.2 的 Device evidence **不成立**，§7 三层证据**不可能同时满足** ⇒ **T1 不通过**。

## 6. 服务端证据（§7.1）

| 项 | 值 |
|---|---|
| HTTP 端点 | `POST /api/v1/events/batch`（经 relay，`200 OK`） |
| 前置 | `POST /api/v1/devices/challenge` 200、`POST /api/v1/devices/auth` 200、`GET /api/v1/children/child-1/tasks/today` 200 |
| 首轮 | `accepted=7`（seq 14..20）`rejected=13` `ack=20` |
| 后续各轮 | `accepted=0 duplicates=7 rejected=13 ack=20/21`（≥12 轮，逐字相同） |
| 库 `devices.last_acked_sequence` | **13 → 20**（设备自身事件被接受）；`study_sessions` 4 → 5 |
| 请求时间 | 首轮 19:41 起；每 ~15 s 一轮，持续到取证时刻 |
| ACK 结果 | 单事件级 `duplicate` / `conflict`，整批 `200` |

## 7. ⚠️ 必须披露的取证副作用（诚实记录）

诊断用回放脚本 `_t1_replay_capture.py`（为了拿到"真实响应体"）**对历史后端库做了一次写入**：

- 它以设备身份走完整 `challenge/auth`，然后 POST 了 **2 个探针事件**：`seq=1`（预期被拒，实测 `conflict`）与 `seq=21`（预期 accepted）；
- 结果：`seq=21` 被 **accepted**，库 `last_acked_sequence` **20 → 21**，并落库 1 条**合成事件**（event_id 非设备产生）；
- 这解释了后端日志里那两条看起来"不是设备发的"的 2 事件批次（`accepted=1 rejected=1 ack=21`、`accepted=0 duplicates=1 rejected=1 ack=21`）——**它们是我自己的探针流量，不是固件的第二个发送方**。

**影响与补救**：
1. **不改变任何结论**（设备侧 `A=0`、`removed=0` 与我的探针无关；设备的批次构成逐轮不变）。
2. 但它**占用了服务端 `(device, seq=21)` 槽位**（FOREIGN event_id）⇒ 若设备将来真产生 seq=21 的新事件，服务端会判 `conflict`。
3. 该库的 **pre-T1 完整备份**在 `E:\workbuddy\claw4-db-backup-20260913\.claw4_host_mvp.db.pre-T1-20260918.bak`（起始 sha256 `90793ef0…`），**可一键回滚**。
4. 若要做"干净基线"复测（`rejected=0`、`removed=N`），建议按 T1-PRECHECK §4 的备选方案：经 Codex 同意后准备一个 **`last_acked_sequence=0` 的干净库**（只插设备身份行，不改 API/schema/契约）。

## 8. 候选完整性（§5 / §9）✅

| 项 | 结果 |
|---|---|
| `otadata` sha256 | `8ba3b110139f45443d4f268d1a3373ef99a1718b71d51664531b83ee2d4b91a3` == D0 值 ⇒ 未变（仍 ota_0） |
| `ota_0` 全量 9,271,760 B sha256 | `c035e1c09ebe472f5f14b490c93844aa3e298cd11b4e30f7fe1055e5b9278d36` == 冻结值 |
| `ota_1` 头 64 B | `1992dd1a…` 与 D0 持平 ⇒ 未发生 OTA 写入 |

⇒ **候选未被上游自动 OTA 替换**；本轮**未 rebuild、未 reflash**。

## 9. 分类与判定（任务书 §8 / §31）

```
分类：ACK_PIPELINE_FAIL
（Backend 收到请求、返回成功，但设备 Outbox 不删除、ACK 不推进）

T1 结果：NOT PASSED
```

- 不属 `ENVIRONMENT_FAIL`（Host Gate 通过、TCP 可达、协议完全匹配）
- 不属 `CONTRACT_FAIL`（设备解码器对真实字节解析正确，契约一致）
- 不属 `DEVICE_SYNC_FAIL`（无 panic / WDT / reboot / event loss / sequence regression **发生在设备自身序号上**；服务端侧的 `conflict` 是**既有基线错位**，不是设备写坏序号）

按 §31：这是**需要改源码的真实产品缺陷** ⇒ 本 Candidate 应冻结为 **FAILED DEVICE CANDIDATE**，
走「新源码修复 → Host Gate → Cold Build → app size vs `ota_0` → CP3-like link audit → 新 Candidate → 重跑 A05-DEVICE」。

**需要 Codex 裁定**：
1. 缺陷归属：修 `scopeBatchToSent`（允许跳过永久拒绝行 / 用服务端 ACK 直接推进可清理范围）还是补"基线不一致时的自恢复"，或两者都要。
2. §8 分类是否认可 `ACK_PIPELINE_FAIL`，以及**序号基线错位**（历史 provisioning 差异）是否要求固件必须自恢复。
3. 第 4 条（学习运行时不在启动路径 ⇒ 同步不自愈）是否同批修复，还是另开任务。
4. 复测环境：用还原后的旧库（`ack=13`，仍会有 13 条 conflict）还是授权准备 `ack=0` 的干净库。

## 10. 复现材料（仅本地，未入库）

```
E:\workbuddy\_t1_logging_relay.py         记录版 relay（请求摘要 + 响应体；不记录 Authorization）
E:\workbuddy\_t1_batch_responses.jsonl    wire 捕获（含 request_summary；无凭据）
E:\workbuddy\_t1_batch_resp_v1.jsonl      打开学习页之前的 5 条捕获
E:\workbuddy\_t1_decode_probe.cpp/.exe    Host 判据 harness（设备解码器 × 真实响应字节）
E:\workbuddy\_t1_resp_actual.json         设备实际收到的响应体原样字节（3364 B）
E:\workbuddy\_t1_replay_capture.py        回放脚本（见 §7 副作用）
evidence/a05-device/decode_outbox_blob.py 设备侧 outbox 权威解码器
evidence/a05-device/backend_ack_baseline.py 只读探库 ACK 基线
evidence/a05-device/_t1-final-evidence.txt  最终设备侧证据（含候选完整性）
```

**当前环境状态**：设备仍运行冻结候选；后端（`127.0.0.1:8000`）与记录版 relay（`192.168.3.26:18765`）仍在监听。
