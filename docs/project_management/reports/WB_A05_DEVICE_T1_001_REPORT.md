# WB-A05-DEVICE-T1-001 报告：Learning Backend 真机同步往返补充验证

**任务性质**：A05-DEVICE 补充环境验证
**基线提交**：`d5e64c54b0a8fb6327bb2e9f023b58bcb5e8bb13`（远端 `workbuddy/a05-build-m0`）
**Candidate**：`claw4-v53-m0-a05-2c8f58f`
**执行时间**：2026-09-18 19:35 – 20:17 (+08:00)
**最终状态**：**NOT PASSED** — 分类 `ACK_PIPELINE_FAIL`（详见 `evidence/a05-device/T1-FINDING-ACK-PIPELINE-FAIL.md`）
**修复方案 / 裁定请求**：见 `reports/WB_A05_DEVICE_T1_001_FIX_PROPOSAL.md`（含**主机侧确定性复现**与三类修法对比）

---

## §12 输出块（按任务书格式，实际结果）

```
A05-DEVICE-T1 NOT PASSED

Candidate:
claw4-v53-m0-a05-2c8f58f

Backend:
192.168.3.26:18765 REACHABLE            (Host Gate PASS：LISTEN + LAN 自检 200)

New events generated:
0   (设备本轮未产生新事件；outbox 内 20 条为更早产生的会话事件，逐轮原样重发)

Backend received:
20  (每 ~15 s 一轮，共 >= 12 轮)

Events ACKed (server side, 本设备自身):
7   (seq 14..20, 首轮 accepted；库 last_acked 13 -> 20)

Outbox removed:
0   <<< 不满足 "> 0"

last_acked_sequence (device local):
NOT ADVANCED   (A=0 -> A=0)

Event identity correlation:
FAIL   (设备发出的 seq1..13 被服务端以 "sequence_already_used_by_other_event" 拒为 conflict：
        服务端该槽位属于别的 event_id，三方 identity 无法一致)

Candidate pre-test SHA256:
MATCH   (c035e1c09ebe472f5f14b490c93844aa3e298cd11b4e30f7fe1055e5b9278d36)

Candidate post-test SHA256:
MATCH   (同上，otadata / ota_0 / ota_1 头 均未变)

Source changes:
NONE

NVS config changes:
NONE    (learning_cfg.base_url 等未改；设备侧 NVS 变化只来自固件自身运行)

Rebuild:
NONE

Flash:
NONE
```

## 1. 三层证据逐项核对（§7）

| 层 | 判据 | 结果 | 证据 |
|---|---|---|---|
| **1. Backend** | 服务端真实收到设备事件 | ✅ 部分成立 | `POST /api/v1/events/batch` 200（≥12 轮）；首轮 `accepted=7`（seq 14..20），库 `last_acked_sequence` 13→20、`study_sessions` 4→5 |
| **2. Device persistence** | `outbox removed > 0` 且 `last_acked_sequence` 推进 | ❌ **不成立** | NVS 权威解码：`T|2 S|0 E|20 Q|21 A|0 F|0 R|1`，`pending_rows=20`、`A` 仍为 0 |
| **3. Identity correlation** | 设备 event_id = 服务端收到 = ACK 后 outbox 消失 | ❌ **不成立** | seq 1..13 服务端判 `conflict`（槽位被其它 event_id 占用）；已 accepted 的 7 条**从未**从设备 outbox 消失 |

⇒ §7 "三层证据必须同时满足" **不满足** ⇒ 不能宣称 `REAL LEARNING BACKEND ROUNDTRIP = PASS`。

## 2. 网络与 Host Gate（§三/§四）— 全部 PASS

| 项 | 实测 |
|---|---|
| 测试主机 IP | `192.168.3.26/24`（WLAN，网关 `192.168.3.1`，AP `505`）⇒ 与设备 NVS 内的 backend 地址同网段 |
| 后端 | `E:\workbuddy\claw4-l1-ready\backend`，uvicorn `127.0.0.1:8000`，`GET /health` 200 |
| relay | 本仓库已审查的 `tools/dev/run-device-backend-relay.py` → `192.168.3.26:18765` **LISTEN**；`GET /api/v1/__probe__` 200 |
| 防火墙 | 已有入站规则 `Claw4 Device Backend Relay 18765`（Private+Public） |
| 依赖 | 复用现成 venv（Py3.13.14 / fastapi 0.141.1 / uvicorn 0.52.4 / sqlalchemy 2.0.52 == `requirements.txt` 锁定值），**未安装任何包** |
| 设备身份匹配 | 库 `devices` 行 secret 的 sha256 == 设备 NVS `learning_cfg.device_secret` 的 sha256 ⇒ `challenge/auth` 通过（凭据只做 sha256 比较，未回显） |
| 采用的网络方案 | **方案 B（现有 LAN 临时切换）** —— 事实上 PC 本就在 `192.168.3.0/24`，无需调整；未改任何网络配置 |

> 设备无需额外配网：设备 NVS 内已存有该 AP 的凭据（`wifi:ssid/password`，前序 A05-DEVICE 配置），上电后自动连上本网段。

## 3. 缺陷摘要（逐条指向证据）

### 3.1 `ACK_PIPELINE_FAIL`：ACK 收到但 outbox 永不清理（**阻塞级**）

- **现象**：设备每 ~15 s 原样重发同一批 20 条；服务端已 `200` + `duplicate`；设备 `removed=0`、`A` 不动。
- **根因（已证实）**：`learning/application/coordinator.cpp:298 scopeBatchToSent()` 只沿"连续前缀"推进 ACK，
  **遇到第一条非 `Accepted`/`Duplicate` 的结果立即 `break`**；本次队首 `seq=1` 被判 `conflict(409)` ⇒
  第一轮即 break ⇒ `any_confirmed=false` ⇒ `scoped.last_acked_sequence` 回落为设备自己的 `0` ⇒
  `applyBatchResult` 只清 `seq<=0` ⇒ 一条不删 ⇒ 永久 livelock。
- **否证"解码器有罪"**：用设备**实际收到的原样响应字节**（3364 B）喂设备自带的 `DecodeBatchResponse()`
  （源码与冻结树 LF 归一化后逐字节相同）⇒ `TRUE / last_acked=21 / results=20 / 映射全对`。
- **一般化**：任何被服务端永久拒绝的行都会把 outbox 永久钉死（`conflict` 断走查；业务 4xx 即使打 dead-letter 也保留且不跳过）；
  卡死点之后的新事件永远排不到；UI 仍显示"已同步/已恢复" ⇒ **静默失败**；无任何自恢复路径。

### 3.2 学习运行时不在启动路径 ⇒ 复位后同步不自愈（**可用性缺陷**）

- `LearningRuntime::Init()`（内含每 15 s 同步 worker 的启动）**全树唯一调用点**是 `learning_screen.cc:607`。
- 实测双向确认：读 flash 复位设备后 → 后端**再无请求**（日志 mtime 停在 19:46:48）；
  用户打开学习页后 → **3 秒内**设备恢复完整 `challenge→auth→batch`。
- 影响：掉电/OTA/重启后，局域网学习同步必须有人进学习页才会恢复。

## 4. 失败分类（§八）

| 候选分类 | 是否 | 判据 |
|---|---|---|
| `ENVIRONMENT_FAIL` | ✗ | Host Gate PASS、TCP 可达、同一 `/24`、防火墙放行 |
| `CONTRACT_FAIL` | ✗ | 设备解码器对真实响应字节解析正确；请求/响应 schema 完全匹配 |
| **`ACK_PIPELINE_FAIL`** | **✓** | 后端收到并成功返回，设备 outbox 不删除、ACK 不推进 |
| `DEVICE_SYNC_FAIL` | ✗ | 全程无 panic / WDT / reboot loop / 事件丢失；设备自身序号未回归（服务端 `conflict` 源于**既有基线错位**，非设备写坏序号） |

**未混写为"网络不好"**：网络层已逐项证伪。

## 5. 候选完整性二次守护（§九）✅

| 项 | 测试前（D0/§5） | 测试后 | 一致 |
|---|---|---|---|
| `otadata` sha256 | `8ba3b110…91a3` | `8ba3b110139f45443d4f268d1a3373ef99a1718b71d51664531b83ee2d4b91a3` | ✅ |
| `ota_0` 全量 sha256 | `c035e1c0…8d36` | `c035e1c09ebe472f5f14b490c93844aa3e298cd11b4e30f7fe1055e5b9278d36` | ✅ |
| `ota_1` 头 64 B | `1992dd1a…` | `1992dd1a9c18f63ad600bb996c4cf123a2abbd4ea8d64dec7b075e6a0d47a026` | ✅ |

⇒ 本 Candidate **未被上游自动 OTA 替换**；无 `CANDIDATE INTEGRITY FAIL`。

## 6. §十 / §十一 未解决事项（保持原状态）

```
云语音完整闭环            DEVICE_VERIFY_REQUIRED     (未在本轮范围)
20-round audio            DEVICE_VERIFY_REQUIRED     (未在本轮范围)
严格 UI latency           DEVICE_VERIFY_REQUIRED     (未在本轮范围)
NETWORK_TOTAL_DEADLINE    DEVICE_VERIFY_REQUIRED     (未在本轮范围)
TCP loopback              DEVICE_VERIFY_REQUIRED     (未在本轮范围)

真实时间源 / /today / time_synced / 首页离线徽标解除   -> 属 C01，本轮未动
DomainState.time_synced   : 未修改
```

**§11 说明**：学习页的 `BackendSessionDiagnostics.network_online` 与首页 `presenters.cpp::isOffline()` 是本轮**观测对象而非判定依据**。
本轮已证明"学习后端可达但同步失败"，因此**首页/学习页的离线显示不能作为 T1 成败的判据**；首页离线徽标含 `!time_synced`（缺真实时间源），按 §11 记为 `EXPECTED UNTIL C01`，**未据此判 FAIL**。

## 7. 对 Candidate 的结论（§31）

本轮**未改任何源码 / CMake / sdkconfig / partition / 设备 NVS，未 rebuild、未 reflash**。
3.1、3.2 两项均为**需要修改源码的真实产品缺陷**，且 3.1 使 §6 要求的"产生 ≥3 个新事件 → 观察 `removed>0`"在本状态下**不可能观察到**。

⇒ 建议本 Candidate 冻结为 **FAILED DEVICE CANDIDATE**，按 §31 走：
`新源码修复 → Host Gate → Cold Build → app size vs ota_0 → CP3-like link audit → 新 Candidate → 重跑 A05-DEVICE`。

## 8. 待 Codex 裁定

1. §8 分类是否认可 **`ACK_PIPELINE_FAIL`**；本次结论是否可直接进入 §31 流程。
2. 修复归属：`scopeBatchToSent` 允许跳过永久拒绝行 / 直接用服务端 ACK 推进可清理范围 / 增加"基线不一致时的自恢复（resync）"——三选几。
3. **序号基线错位**算"历史 provisioning 差异（环境）"还是"固件必须自恢复"。
4. 第 3.2 条（学习运行时不在启动路径）是否同批修复，或另开任务。
5. 复测环境：还原旧库（`ack=13`，仍会有 13 条 `conflict`）还是授权准备 `ack=0` 的干净库。

## 9. 取证副作用披露（诚实记录）

为拿到"设备实际收到的真实响应字节"，我使用了一个**仅本地**的取证 relay（与仓库已审查 relay 同转发语义），
并运行了一个**回放脚本**（走完整 `challenge/auth` 后 POST 2 个探针事件）。该回放：
`seq=1` 被拒（`conflict`），`seq=21` 被 **accepted** ⇒ 历史后端库 `last_acked_sequence` **20→21**，并落库 1 条**合成事件**。

- 这解释了后端日志中两条"非设备发出的 2 事件批次"（`accepted=1 rejected=1 ack=21`）——**是我自己的探针流量，不是固件的第二个发送方**。
- **不影响任何结论**（设备侧 `A=0`、`removed=0`、每轮批次构成与探针无关）。
- 副作用：服务端 `(device, seq=21)` 槽位被外来 event_id 占用；如需干净复测请用 §8.5 的库选择。
- **完整 pre-T1 备份**：`E:\workbuddy\claw4-db-backup-20260913\.claw4_host_mvp.db.pre-T1-20260918.bak`（起始 sha256 `90793ef0…`），可回滚。

## 10. 提交边界（§十三）

本轮**只新增**：本 T1 报告 + 现场发现文档 + T1 预检文档 + 设备侧/服务端只读证据与取证脚本。
未修改任何产品源码、未新增 Candidate、未改契约。

**脱敏**：`device_secret` / token / Wi-Fi 密码 / MQTT 凭据 / 完整 NVS dump **均未入库**；含凭据产物已 gitignore（仅本地）；报告只记哈希与契约级标识符。
