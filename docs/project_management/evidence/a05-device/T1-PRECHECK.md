# A05-DEVICE-T1 — 执行前预检（T1-PRECHECK，只读，未开始测试）

**基线**：`d5e64c54b0a8fb6327bb2e9f023b58bcb5e8bb13`（== 远端 `workbuddy/a05-build-m0`）
**Candidate**：`claw4-v53-m0-a05-2c8f58f`（`c035e1c0…8d36`）
**本文件状态**：预检结论 + 前置条件 + 一个**任务书未预判到的基线错位问题**。**尚未改环境、未起后端、未做任何写入。**
**脱敏**：全篇不写 `device_secret` / token / Wi-Fi 密码；凭据只以 sha256[:16] 参与比较。

---

## 1. 结论

| 项 | 结论 |
|---|---|
| 是否存在"现有、已审查的 Learning Backend" | **存在** —— `E:\workbuddy\claw4-l1-ready\backend`（`app/main.py` 含全部设备端点），即 2026-09-12 L3 验收同一实体 |
| 后端库是否含本设备凭据 | **是，且匹配**（见 §3）⇒ `/devices/challenge` + `/devices/auth` 可过 |
| relay 是否现成 | **是** —— 本仓库 `tools/dev/run-device-backend-relay.py`（已审查；只转发 `/api/v1/*`） |
| 运行依赖 | `claw4-l1-ready/backend/requirements.txt` 锁定 fastapi 0.141.1 / uvicorn 0.52.4 / sqlalchemy 2.0.52（Py3.13）；**该目录当前无 venv**，需新建 `backend/.venv`（gitignore 已排除） |
| 网络前提 | **不满足**：需要一条 `192.168.3.0/24`，主机占 `192.168.3.26`、设备同 L2（见 §5） |
| 任务书 §7 通过条件可达性 | **可达，但有前提** —— 必须选对后端库（见 §4），否则会**伪失败** |
| 本轮是否动过设备/源码/配置 | **否**（全部只读：esptool read_flash / 只读 sqlite / 只读源码） |

## 2. 已完成的只读取证（可复现）

```
设备侧 NVS 快照（已有，14:55 最新）      _now2-nvs.bin  sha256 3261ec5e…
设备 outbox 结构解码                     decode_outbox_blob.py（本目录）
后端库 A  E:\workbuddy\claw4-l1-ready\backend\.claw4_host_mvp.db        mtime 2026-09-13 11:54
后端库 B  E:\workbuddy\claw4-db-backup-20260913\....db.bak-20260913-0025 mtime 2026-09-13 00:19
```

## 3. 设备身份 ↔ 后端库匹配（关键前提，已证）

```
库 A/B  devices 行: device_id = 988566fb-…
                    model=metalio-claw-4  fw_version=2.0.51
                    installation_id=claw4-com7-20260906
                    该行 device_secret 的 sha256[:16] = 0a754135217fb83f
设备 NVS learning_cfg.device_secret 的 sha256[:16] = 0a754135217fb83f   ⇒ MATCH
device_children 绑定 = (988566fb-…, child-1)                            ⇒ 与设备 child_id 一致
设备 outbox 引用的 task_id = 4f6f26c8-…, 06b4af3d-…                     ⇒ 库中存在（child-1 的任务）
```
后端对未知 `task_id` 不会拒绝（`store.project_event`：未知 task "record nothing"），
故 `demo-math-001` 这类 demo 任务不会造成 rejected。

## 4. ⚠️ 硬问题：ACK 序号基线错位（**开测前必须先定调**）

### 4.1 两端语义（源码依据）
```
后端 app/main.py:_process_batch
   expected = dev.last_acked_sequence + 1
   sequence == expected        -> accepted（并 advance_ack）
   sequence  > expected        -> gap        (409)
   sequence  < expected        -> rejected   (sequence_regression, 409)
   同一 event_id 但 payload/sequence 不同 -> conflict (409)

设备 firmware main/learning/sync/outbox_core.cpp:applyBatchResult + ports/nvs_outbox_storage.cpp:removeAcked
   只对 Accepted/Duplicate 且 sequence <= 响应里的 last_acked_sequence 的行做清理
   removeAcked(up_to) 删除所有 sequence <= up_to 的 pending 行，并把本地 last_acked 推进到 up_to
```

### 4.2 实测基线
```
设备 NVS   hdr:  E|17  Q|18  A|0      ← 当前 A（本地 last_acked）= 0，pending 行序号从 1 连续编号
           历史:  E|13/Q|14/A|0 → E|14/Q|15/A|0 → E|17/Q|18/A|0   （本地序号被“从 1 重置”过）

库 A（l1-ready） devices.last_acked_sequence = 13
                events 表 seq 1..13 全部 accepted，但其 event_id 与设备现存 outbox **不同源**
库 B（备份）     devices.last_acked_sequence = 20
                events seq 16..20 的 event_id 与设备现存 outbox **有交集**
```

### 4.3 后果（决定用哪个库）
| 选库 | 首批结果 | 设备 outbox | §7.2 判定 |
|---|---|---|---|
| **库 A（ack=13）** | seq 1..13 → `sequence_regression`；seq 14..N → `accepted`（首批 accepted = N-13） | `removeAcked(N)` ⇒ **移除全部 N 行**；本地 `A|` 0→N | `removed > 0` ✓、`last_acked` 推进 ✓、身份三方一致（accepted 的那些）✓ ⇒ **可 PASS** |
| 库 B（ack=20） | 设备全部序号 < 21 ⇒ **全部 regression、0 accepted** | 不移除 | **removed = 0 ⇒ 伪失败** |

⇒ **必须用库 A**。库 A 的代价是首批发会有一批 `sequence_regression` 拒绝：
**这是"设备本地序号基线 vs 服务端 ACK 基线"的历史错位，不是候选固件缺陷**，
任务书 §8 的四类失败里没有这一类别，**需先报 Codex 确认该现象按"预期基线差异"记录**，
避免被误判为 `ACK_PIPELINE_FAIL`。

可选更干净的方案（需 Codex 同意，属**服务端数据准备**，不改 API/schema/契约）：
新建一个库，仅插入该设备的身份行（device_id + secret 来自设备 NVS）且 `last_acked_sequence = 0`，
则设备 pending 序号 1..N 全部按序 accepted ⇒ `rejected = 0`、`removed = N`、`A: 0→N`，证据最干净。
两种方案我都会把"服务端起始 ack / 设备起始 A / 双方 event_id 集合"完整记录在报告里。

## 5. 环境前置（**我做不了，需要你或管理员**）

目标拓扑（任务书 §三 方案 A/C 的落地版）：
```
Claw4 设备  ──DHCP──▶  192.168.3.0/24  ──▶  主机 192.168.3.26:18765 (relay) ──▶ 127.0.0.1:8000 (后端)
```
- **必须在同一 L2**：设备用 `/24` 掩码，若 `192.168.3.26` 不在自己的网段内就会丢给网关；
  家宽路由器没有 `192.168.3.0/24` 回程路由 ⇒ 仅在 PC 上加一个 `192.168.3.26` 别名**无效**
  （实测判据：本机 `Find-NetRoute 192.168.3.26` 走的是 Tailscale 出口，说明该网段不存在于本 LAN）。
- 方案 A（推荐）：独立路由器/AP，LAN 设 `192.168.3.1/24`；PC 接入并把该网卡静态设为 `192.168.3.26/24`（需管理员）；设备经 NetworkScreen 连该 AP。
- 方案 B：现有 LAN 临时改为 `192.168.3.0/24`（先记录原配置，测完恢复）。
- 方案 C：静态路由/VLAN 让设备可达 `192.168.3.26:18765`（DNS/NAT/代理不得改变设备看到的目标地址与协议语义）。
- 设备侧配网允许（`SsidManager` 落盘），**不碰 Learning NVS**。
- 该测试 LAN 可以**没有外网**：学习后端是 LAN 直连；上游 OTA/MQTT 离线不影响 T1。
  但注意候选 `application.cc:179` 的自动 OTA 路径——测试前后仍按 §5/§9 复核 `otadata`+`ota_0` 全量 sha256+`ota_1` 头。

## 6. 我（主机侧）可以自动完成的部分
1. 建 `E:\workbuddy\claw4-l1-ready\backend\.venv`（Py3.13）+ 按 `requirements.txt` 锁定版本安装。
2. `uvicorn app.main:app --host 127.0.0.1 --port 8000`（CWD = `claw4-l1-ready/backend`，即默认用库 A）。
3. `python tools/dev/run-device-backend-relay.py --bind 192.168.3.26 --port 18765 --target-port 8000`（本仓库脚本）。
4. Host Gate 自检：`Get-NetTCPConnection -LocalPort 18765 -State Listen` + 从同网段第二台机（你的手机/另一台 PC）验证 `192.168.3.26:18765` 可达。
5. 测试前/后：`otadata`、`ota_0` 全量 sha256、`ota_1` 头 64B 复核（只读，**每次会复位设备一次**，故放在事件产完之后）。
6. 测试后：NVS 只读取证 → `E|`（pending 数）下降、`A|` 推进、服务端 `devices.last_acked_sequence` 推进、`events` 表新增行 → 三方身份一致比对。
7. 产出 T1 报告（任务书 §12 的状态块 + §7 三层证据表）。

## 7. 待你裁定的 3 件事
1. 网络方案选 **A / B / C**（以及是否要我给可直接粘贴的单行命令）。
2. 是否先让我把 venv + 依赖装好（不需要任何网络改动，可现在就做）。
3. 是否先把 §4 的"ACK 基线错位"报给 Codex 定调（用库 A 接受回归拒绝，或授权建基线 0 的干净库）。
