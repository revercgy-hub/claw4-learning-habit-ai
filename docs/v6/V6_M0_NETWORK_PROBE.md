# V6 M0 网络链路探针（第 5 轮实机取证）

日期：2026-09-22 10:10–10:12
执行：WorkBuddy（真机只读串口取证，无刷写、无写入 Flash）
候选：`m0-candidate-04.json`（与第四轮同一固件，未重刷）
设备：ESP32-P4 rev1.3，COM7，SER `80:F1:B2:D2:ED:14`

## 1. 目的

验证 `V6_M0_DEVICE_REPORT.md` 与 `WB-V6-M0-AUDIT-2026-09-22.md` §2 的推断：
设备 NVS 里带着旧 SSID，扫不到匹配 AP，**60 秒后应自动进入热点配网模式**。
此前四轮取证最长只到 45 秒，从未跨过该阈值。

## 2. 方法

复用项目自带 `tools/v6/capture_device.py`（其上界为 60 s），连续跑两段跨越阈值：

```bash
python tools/v6/capture_device.py --port COM7 --seconds 60 --reset \
       --output out/v6-device-private/netprobe-m0-01.txt
python tools/v6/capture_device.py --port COM7 --seconds 50 \
       --output out/v6-device-private/netprobe-m0-02.txt
```

总观测 **110.7 秒**，覆盖 `t=0`（复位）到 `t=110695 ms`。

## 3. 原始证据（私密目录，不提交）

| 文件 | 字节 | SHA256 |
| --- | --- | --- |
| `netprobe-m0-01.txt` | 11,373 | `022a6186f6ebc5ae91243956113f1f98282cb94bda0ece2322d3338782e52b2d` |
| `netprobe-m0-02.txt` | 1,710 | `b75efe4403826f396ed8ba5122ae454fd1497096010307e5357d4b412b48f1d0` |

## 4. 时间线（日志原文）

```text
I (8677)  WifiBoard: Starting WiFi connection attempt        ← have_ssid==true
I (9603)  WifiStation: Scanning all channels
I (12061) WifiStation: No AP found, next scan in 10 seconds
I (22061) WifiStation: Scanning all channels                 ← 退避 10 → 20
I (24508) WifiStation: No AP found, next scan in 20 seconds
I (44508) WifiStation: Scanning all channels                 ← 退避 20 → 40
I (46955) WifiStation: No AP found, next scan in 40 seconds
W (68677) WifiBoard: WiFi connection timeout, entering config mode   ← 8677 + 60.000 s
I (68677) WifiManager: Stopping station
I (68733) V6M0: NETWORK_EVENT=3                              ← NetworkEvent::Disconnected
W (68736) StateMachine: Invalid state transition: unknown -> wifi_configuring
I (68748) WifiManager: Starting config AP
I (68752) DnsServer: Starting DNS server
I (69131) RPC_WRAP: ESP Event: softap started
I (69151) WifiConfigurationAp: Access Point started with SSID Xiaozhi-79D9
I (69175) esp_netif_lwip: DHCP server started on interface WIFI_AP_DEF with IP: 192.168.4.1
I (69191) WifiConfigurationAp: Web server started
I (69210) WifiBoard: WiFi config mode entered
I (69210) V6M0: NETWORK_EVENT=4                              ← NetworkEvent::WifiConfigModeEnter
```

`t=8677 → t=68677` 恰好 **60.000 秒**，与 `main/boards/common/wifi_board.cc:27` 的
`CONNECT_TIMEOUT_SEC = 60` 逐毫秒吻合。推断被实测证实。

进入配网模式后设备持续运行（`rpc_req: Scan start Req` → `StaScanDone` 在
82080 / 94960 / 107839 反复出现，即配置页在周期性刷新可用 AP 列表），至 110.7 s
**无断言、无重启**。

## 5. 结论

**`CONFIG_PORTAL_REACHABLE = PASS`。**

| 判定 | 结果 |
| --- | --- |
| 60 s 连接超时 → 自动进配网 | ✅ 实测 |
| C5 softap 启动（射频**发射**通路可用） | ✅ `softap started` |
| 设备热点 SSID | `Xiaozhi-79D9` |
| 配置页 DHCP | `192.168.4.1`（`WIFI_AP_DEF`） |
| 配置 HTTP 服务 | ✅ `Web server started` |
| 真实联网 / 取得 IP | ❌ 仍未验证（`NETWORK_EVENT` 只到 4） |

⚠️ **成立范围**：热点能起来只证明 C5 的射频**发射**通路可用。
**关联（association）能力仍未被证明** —— 拿到 IP 才算。

## 6. 下一步（需要用户操作，零改写、零刷机）

1. 设备保持通电（当前正处于配网模式；重新上电后需再等约 70 s 才会重新进入）；
2. 手机连 WiFi 热点 **`Xiaozhi-79D9`**；
3. 打开配置页（通常自动跳转，或访问 `http://192.168.4.1`），填家里 2.4 GHz 的 SSID 与密码；
4. 提交后抓串口，确认出现 `Connected to WiFi: <ssid>` 与 `V6M0: NETWORK_EVENT=2`
   （`NetworkEvent::Connected`）。

拿到这两行，M1「NAS 连续语音 20 轮」的网络前提即告闭合。

## 7. 本轮附带发现

### 7.1 `StateMachine: Invalid state transition: unknown -> wifi_configuring`

M0 诊断入口 `main/boards/metalio/claw4-learning-v6/m0_diagnostics.cc` 直接调
`board.StartNetwork()`，绕过了上游 `Application` 的状态机初始化，因此
`SetDeviceState(kDeviceStateWifiConfiguring)` 被拒绝。

- 对 M0 无功能影响（配网 AP 与配置页照常工作）。
- 对 **M1 有影响**：M1 会切回上游 `main.cc`，届时需要确认状态机初值正确，
  否则设备进了配网模式但 UI 不提示，用户会不知道要配网。
- 建议 Codex 在 M0→M1 切换时把这一条列为回归检查项。

### 7.2 HEALTH 采样节奏得到确认

零交互的 110 秒里，HEALTH 严格每 10 秒一次
（18579 / 28584 / 38593 / 48603 / 58613 / 68622 / 78628 / 88637 / 98643 / 108653）。
这印证了审计报告 §5 第 4 条：第四轮缺失 HEALTH 是**点击阻塞了 `ticks` 增长**，
不是采样逻辑坏了。把 `ticks % 10` 改成按时间判定可让交互期也保留采样。

### 7.3 内存基线与 RGB888 改动一致

| 轮次 | free | PSRAM free | 显示方案 |
| --- | --- | --- | --- |
| 第二轮 | 28,027,643 | 27,878,604 | 通用 RGB565 partial-transfer |
| 本轮 | 27,134,019 | 26,841,788 | 原生 RGB888 + 双 panel framebuffer |

PSRAM 少约 **1.04 MB**，与"两个 panel framebuffer + 每像素 3 字节"量级吻合，
是显示修正的预期代价，非泄漏。110 秒内数值稳定（进入配网后仅降 ~16 KB，属正常分配）。

### 7.4 一条与 M1 相关的风险

`E (6004) system_api: 0 mac type is incorrect (not found)` 每轮必现：应用在
`t≈6.0 s` 读 WiFi MAC，而 C5 在 `t≈8.5 s` 才 `Coprocessor Boot-up`。
M1 接后端时若用 MAC 派生设备 ID，会拿到不稳定值。建议把 MAC 读取挪到
`Coprocessor Boot-up` 之后，或改用设备侧持久化 ID。
