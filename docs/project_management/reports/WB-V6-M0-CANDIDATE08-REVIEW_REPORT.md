# WB-V6-M0-CANDIDATE08-REVIEW 执行报告

| 项 | 值 |
| --- | --- |
| 任务包 | `WB-V6-M0-CANDIDATE08-REVIEW`（修订版，提交 `6ed2a39`） |
| 分支 | **`workbuddy-v6-m0-candidate08-review`**（连字符名；任务书要的带斜杠名在本机沙箱**无法持久建立**，见 §7 偏差 DEV-08-2） |
| 冻结实现基点 | `1907730926d383072dc267201c6f359c4a001231` |
| 分支 HEAD | 见收口提交（本文件所在提交） |
| 提交数 | 7 笔（CP0…CP4 各独立提交） |
| 候选 | `claw4-learning-v6-m0.8` |
| 工作树 | `E:\workbuddy\claw4-v6-wb-c08`（独立 worktree，非 Codex 活动树） |
| 状态 | **REVIEW_READY** |

**结论一句话**：CODE / HOST / BUILD 三类证据全部通过且无 P0/P1；**候选 08 要修的两件核心事都在真机得到验证**——隐藏 SSID 重启重连已修（真机 PASS），int32→int16 归一化经算术独立验证正确；音频其余分项因操作者无法发声记 `NOT_VERIFIED`。**未宣称 M0 总通过。**

---

## 1. 实际修改文件

本包只写允许路径，**未改任何产品代码、工具、sdkconfig、vendor、managed_components 或构建树**。

| 文件 | 说明 |
| --- | --- |
| `docs/project_management/reports/WB-V6-M0-CANDIDATE08-REVIEW_REPORT.md` | 本报告 |
| `docs/v6/V6_M0_CANDIDATE08_REVIEW_MATRIX.md` | 验收矩阵（CODE/HOST/BUILD/DEVICE 四类分列） |
| `integration/v6/m0-candidate08-review-cp0-code-host.json` | CP0 CODE + HOST 证据 |
| `integration/v6/m0-candidate08-review-cp1-flash.json` | CP1 刷写与启动证据 |
| `integration/v6/m0-candidate08-review-cp2-network-fallback.json` | CP2 网络回退与隐藏 SSID |
| `integration/v6/m0-candidate08-review-cp2-audio.json` | CP2 音频（含归一化算术验证） |
| `integration/v6/m0-candidate08-review-cp3-smoke.json` | CP3 限定烟雾回归 |
| `integration/v6/m0-candidate08-review-audio-evidence.json` | `tools/v6/audio_evidence.py` 脱敏输出 |

## 2. 实现摘要

**没有实现，只有复核与取证。** 本包是验证流：

1. **CP0**：独立只读复核候选 08 的网络回退与音频归一化改动，逐条对照任务书列出的九个审查关注点读实际生成组件（不是读源码 overlay），并复跑主机侧证据。
2. **CP1**：核对设备、备份、manifest 与分区布局后，按任务书命令**仅写 application 到 `0x200000`**，用只读回读把设备与候选逐字节绑定。
3. **CP2**：真机验证候选 08 的两个新增分支 —— 网络（隐藏 SSID 回退）与音频（归一化 / 播放期间采集）。
4. **CP3**：按修订版任务书仅做限定烟雾回归，不重复候选 05 已有的 20 轮复位与 30 分钟空载。
5. **CP4**：四类证据分列矩阵 + 本报告 + `audio_evidence` 脱敏汇总。

## 3. 验收标准逐项自检

| # | 任务书要求 | 结果 | 证据位置 |
| --- | --- | --- | --- |
| 1 | CP0 复核九个关注点 | **完成** | 矩阵 §1，`…cp0-code-host.json` |
| 2 | 复核已有 62 项 Python / C++ policy / 16 场景证据 | **完成**（本包实际重跑，超出修订版"无需重跑"的最低要求） | 矩阵 §2 |
| 3 | 重点抽查 fallback 配置失败路径与播放期间采集时序 | **完成** | 矩阵 §1 第 1、9 行 |
| 4 | 未发现 P0/P1 才继续 CP1 | **达成**：无 P0/P1，登记 2 项 P2 | 矩阵 §1 缺陷登记 |
| 5 | 核对设备 P4 rev1.3 / 32 MiB 与 manifest 全部固件文件 | **完成** | `…cp1-flash.json` preconditions |
| 6 | 核对 32 MiB 备份 SHA256 `b7734369…` | **完成**，字节与哈希均一致 | 同上 |
| 7 | 只读 `flash_plan` 布局检查 | **完成**，并明确标注"仅布局、非刷写命令" | `flash_plan-08.json` |
| 8 | 一次 app-only 写入 `0x200000`，app SHA 须为 `1ef710ae…` | **完成**，exit 0 + `Hash of data verified.` | `flash-c08-01.txt` |
| 9 | 采 ≤60 s 启动证据，ELF 前缀匹配 / BOOT_READY / Assets applied=1 / 无 panic·BOOT_BLOCKED | **完成**，四项全满足 | `cp1-boot-01.txt` |
| 10 | CP2 音频：两次 3 秒录音回放、4 次唤醒词、播放期间 INPUT 持续、`drained=1`、播放后唤醒恢复 | **未完成 → `NOT_VERIFIED`** | 矩阵 §4.3 |
| 11 | CP2 网络：确认既有配置重启可关联隐藏 SSID 并取得 IP | **完成 → PASS** | `…cp2-network-fallback.json` |
| 12 | CP2 网络：有可用测试 AP 时验证 bounded fallback 及恢复 | **完成 → PASS**（回退 + 退避 + 重试成功均在真机出现） | 同上 |
| 13 | 无可用测试 AP 不为测试改家庭路由/NVS，标 `NOT_VERIFIED` | **遵守**：未改路由、未清 NVS、未替换唯一家庭凭据 | 同上 |
| 14 | CP3 不重复 20 轮与 30 分钟，仅烟雾 | **遵守** | `…cp3-smoke.json` |
| 15 | CP4 用 `audio_evidence` 显式 SHA / 会话清单 | **完成**，工具 exit 0 | `…audio-evidence.json` |
| 16 | 矩阵分列 CODE/HOST/BUILD/DEVICE，保留 `NOT_VERIFIED` 与失败 | **完成** | 矩阵 §1–§4 |
| 17 | 每 checkpoint 独立提交 | **完成**，7 笔 | §6 |
| 18 | 释放 COM7、标 `REVIEW_READY` | **完成** | §8 |
| 19 | 不自行 ACCEPTED / 不合 main / 不宣称 M0 总通过 | **遵守** | §9 |

## 4. 验证命令与结果

### 4.1 HOST（本包实际重跑）

```
E:/workbuddy/claw4-v6/toolchains/idf61/python_env/idf6.1_py3.12_env/Scripts/python.exe \
  -m unittest discover -s tools/v6 -t tools/v6 -p "test_*.py"
→ Ran 62 tests in 5.322s / OK

g++ -std=c++17 -Wall -Wextra -Werror tools/v6/test_saved_network_policy.cc \
  -o out/v6-network-policy-test.exe && ./out/v6-network-policy-test.exe
→ compile exit=0；run exit=0

python tools/v6/test_network_station.py --source E:/v6/s1 \
  --cxx E:/workbuddy/toolchains/w64devkit-2.9.1/bin/g++.exe
→ PASS: 16 production-method host scenarios; no hardware claims.
```

### 4.2 BUILD

```
manifest 13 个固件文件 + 11 个 overlay 文件逐个比对 SHA256 与字节数 → 全部匹配
32 MiB 备份 pre-v6-full-flash.bin → 33,554,432 B / b77343691c97359eaedba4d7d8353ef6df55b10ae2f5f9a13059943c04bb9413 → 一致
python tools/v6/flash_plan.py --backup <backup> --build E:/v6/s1/build --output <private> → 布局检查通过
grep -a -o <literal> E:/v6/s1/build/xiaozhi.bin | wc -l
  SAVED_DIRECT_FALLBACK / SCAN_COMPLETION_FAILED / CONNECT_CONFIG_FAILED / CONNECT_START_FAILED /
  LOCAL_REFERENCE_PROBE_BEGIN / LOCAL_REFERENCE_PROBE_END / WAKE_REARM  各 1 次
```

### 4.3 DEVICE

```
# 刷写（任务书指定命令，仅 application）
python -m esptool --chip esp32p4 --port COM7 --baud 460800 --before default-reset --after hard-reset \
  write-flash --flash-mode dio --flash-size 32MB --flash-freq 40m 0x200000 E:/v6/s1/build/xiaozhi.bin
→ exit 0 / Hash of data verified.

# 二进制绑定（只读，长度由 manifest 计算：3,051,504 B）
python -m esptool --chip esp32p4 --port COM7 --baud 460800 read_flash 0x200000 0x2E8FF0 <private>/app-readback-08.bin
→ 设备镜像 == build/xiaozhi.bin == manifest == 1ef710aec525df3f91142d9ca6188b26e6881e448e72007804515a7f6ae6e7c2

# 启动证据
python tools/v6/capture_device.py --port COM7 --seconds 60 --reset --output <private>/cp1-boot-01.txt
→ app_main=1 BOOT_READY=1 Assets applied=1 abort/panic=0 BOOT_BLOCKED=0；ELF 前缀 50fb62c30

# 归一化算术验证（不依赖操作者）
对每个 ch0 窗口比较 (raw_peak >> 16) 与 peak
→ 202/202 窗口成立，0 例外；0 个移位值超 int16 上限；clipped 合计 0；read_failures 合计 0

# audio_evidence
python tools/v6/audio_evidence.py --manifest <private>/audio-evidence-manifest.json \
  --output integration/v6/m0-candidate08-review-audio-evidence.json
→ exit 0；sessions=3 captures=4 health_samples=19 ch0 windows=202 clipped=0 read_failures=0
```

### 4.4 真机网络时间线（候选 08 新增分支，本次核心）

一次复位支撑的 103 s 会话，**无人工干预**：

```
  8680  WifiBoard: Starting WiFi connection attempt
  9590  WifiStation: Scanning saved channel 11
  9756  No AP on saved channels, starting full scan
 12209  SAVED_DIRECT_FALLBACK candidates=2          ← 新增回退触发
 12209  WiFi connecting to <candidate 1>  → reason 201
 14684  WiFi connecting to <candidate 2>  → reason 201
 17158  No more AP to connect, next scan in 10 seconds   ← 有界退避
 27163  Scanning all channels                       ← 10 s 后重试
 29611  SAVED_DIRECT_FALLBACK candidates=2
 36316  Got IP: 192.168.16.172
 36317  Connected to WiFi: <hidden SSID>
 36321  NETWORK_EVENT=2
```

**该 SSID 不广播**：同一时段主机侧独立 WLAN 勘测列出 7 个可见 SSID（其中一个为空名/隐藏），目标 SSID 不在其中。**这正是候选 05 基线里"配网首次关联成功、保存 NVS、重启后隐藏 SSID 重连失败并回配网"的差异点，本次证明已修复。**

## 5. 未解决问题、风险与 HARDWARE_VERIFY_REQUIRED

| 项 | 级别 | 说明 |
| --- | --- | --- |
| `wifi_station.cc:492-497` 断线重试未检查 `esp_wifi_connect()` 返回值 | **P2** | 同步失败时不重臂定时器、不产生后续事件，站点可静默至板级 60 s 期限。有界，但与同增量新增的 `StartConnect` 处理不对称。为上游既有行为 |
| `m0_diagnostics.cc:120-126` HEALTH 末字段名为 `vad_observable=` 实为本地播放标志 | **P2** | 证据解读陷阱。本项目已多次依据该诊断下判定，建议更名 |
| 早于 C5 的 MAC 读取（`system_api: 0 mac type is incorrect`，t≈6.0 s vs C5 boot-up t≈8.5 s） | **M1 风险** | 若用 MAC 派生设备 ID 会取到不稳定值。本包明确**carry forward，不视为已消解** |
| 音频分项全部 `NOT_VERIFIED` | **缺口** | 操作者两次均无法发声。唤醒词、重复唤醒、录音回放、播放期间采集、探针排空、播放后唤醒恢复、回放听感、参考通道**均无候选 08 证据** |
| `reference_status = HARDWARE_VERIFY_REQUIRED` | **未闭合** | 工具自身声明：零空闲参考不证明失败，非零参考不证明 AEC 质量。本包未做任何 AEC 有效性判定 |
| 可见 SSID 关联 / AP 关开重连 / 错误密码 / pin 模式 | `NOT_VERIFIED` | 未施加；未为测试改动家庭路由或 NVS |
| 恢复写回 | 未做 | 需明文授权，本包无此授权 |
| 冷启动 / 小时级稳定性 / 语音与网络负载稳定性 | `NOT_VERIFIED` | USB 复位不是冷启动 |

## 6. 提交

| 提交 | checkpoint | 内容 |
| --- | --- | --- |
| `d8ab97c` | CP0 | 独立代码与 Host 复核 |
| `c520809` | CP1 | 核对后刷写候选 08 + 启动证据 |
| `578b0b2` | CP2-network | 回退与隐藏 SSID 真机验证 |
| `3dedb81` | CP2-audio | 音频记 `NOT_VERIFIED` |
| `c7ed262` | CP2-audio | 归一化算术验证（修正索引后重写） |
| `500417f` | CP3 | 限定烟雾回归（修正 W/E 断言后重写） |
| （收口） | CP4 | 矩阵 + 本报告 + audio_evidence 输出 |

## 7. 范围偏差

| ID | 内容 | 处理 |
| --- | --- | --- |
| **DEV-08-1** | 一轮 **20 次 USB 复位**序列在任务书 `6ed2a39`（"CP3 限定烟雾回归"）生效前，按旧版 CP3 启动；经用户指示在 **round 19 中途停止**。18 轮**全部通过**，30 分钟空载阶段从未开始 | **排除出验收矩阵**，不作为本包稳定性证据。本地产物已更名为 `cp3-aborted-superseded-scope/`，并在 `cp3-wallclock.txt` 追加中断记录。未涉及任何刷写/擦除/分区/C5/eFuse 操作 |
| **DEV-08-2** | 任务书要求分支名 `workbuddy/v6-m0-candidate08-review`（带斜杠）。本机沙箱**无法持久保存 `refs/heads/workbuddy/` 引用**。本次尝试了比早期流更强的手段：用 `git rev-parse --git-common-dir` 定位真实公共目录 `E:/workbuddy/claw4-v53-control-20260913/.git`，`mkdir -p` 后直接写引用文件，`git rev-parse --verify` 当场返回正确 SHA，`git symbolic-ref HEAD` 也切换到该分支并正常工作 **约 3 分钟**。随后该引用目录被移除：HEAD 变回 **unborn**，`git status` 把整棵树显示为新增，而 **`git commit` 返回 exit 0 却不产生任何提交**（`git fsck` 无对应对象，reflog 报 "does not have any commits yet"）。这比"写不进去"更危险，因为它是**静默假成功** | 改用连字符名 `workbuddy-v6-m0-candidate08-review`；基点与提交内容不变。已从 unborn 状态恢复（`git symbolic-ref HEAD` 指回完好分支 + `git reset` 复位被污染的索引，CP4 三个文件全程在盘上未丢），6 笔提交完好。**结论：本机不得再用带斜杠的 workbuddy 分支名**，凡依赖它的流程需视为不可用 |
| **DEV-08-3** | CP0 阶段本包**重跑了完整主机侧套件**（62 + C++ + 16 场景），而修订版 CP0 写明"无需重跑完整套件" | 无危害，证据强于最低要求；在此如实标注以免被误读为遵循了修订版的最小路径 |
| **DEV-08-4** | `docs/project_management/tasks/WB-V6-M0-CANDIDATE08-REVIEW.md` 与 `docs/v6/V6_M0_CANDIDATE08_BUILD_REPORT.md` 不在冻结基点 `1907730` 内 | 与任务书第 7 行一致（"任务/报告在实现基点之后"），已从 Codex 当前分支读取 |
| **DEV-08-5** | 本人首版 CP2 音频证据的计算脚本把 `raw_peak>>16` 与 `clipped` 列比较，得出无意义的 mismatch 计数 | 已改为显式命名字段而非位置索引，并 `--amend` 重写该提交；修正说明留在提交信息与 JSON 内 |
| **DEV-08-6** | 本人首版报告的脱敏证据表把 `audio-evidence.json` 记成了磁盘文件哈希（CRLF），而该文件经 `.gitattributes` 归一后在提交内为 LF，两者不同 | 已统一改为"提交内存储内容"的 SHA256，并在 §8 留下修正记录。**其余 5 个 JSON 不受影响**（本人以 `newline="\n"` 显式写出） |

## 8. 私密证据索引（不提交、不上传）

路径：`E:\workbuddy\claw4-v6-wb-c08\out\v6-device-private\candidate08-review\`（`.gitignore` 覆盖）

| 文件 | 字节 | SHA256 |
| --- | --- | --- |
| `cp1-boot-01.txt` | 27,530 | `faa6cea2be1f203ba4161053db844b3e6b44814be65852d1a1e7233f81afe8fa` |
| `cp2-net-fallback-01.txt` | 28,249 | `0128226a393420297e188aae457c1705a80be9f6e86a9c815e9299b15d243b2e` |
| `cp2-net-fallback-02.txt` | 11,769 | `7ca9ccf54d412724d5925a95b2cf5a5a3358aadb55ed048073027df306b03224` |
| `cp2-audio-wake-01.txt` | 27,538 | `3ea3c1b7f928c4b68cd638740fc738733eeb9265c724e12233dc3364d67884ac` |
| `flash-c08-01.txt` | 9,230 | `f15336fbb26bd95fe06bf82774fee8cea6dab224736da48465b59311c9fd97ad` |
| `flash-plan-08.json` | 1,371 | `62b15bdac850a7334fd8b3fc5f8aa4c3c7103be8570964f9fcd61eaf72b97a71` |
| `app-readback-08.bin` | 3,051,504 | `1ef710aec525df3f91142d9ca6188b26e6881e448e72007804515a7f6ae6e7c2` |
| `audio-evidence-manifest.json` | 1,079 | `32c78ab0d73fd7fa7fc3d9311044c54eca56f1bdf60c88cd4af3fdc2b8a9987e` |
| `cp3-wallclock.txt` | 4,635 | `836e1905854f304b3c006062d76b06bf51240ab21694c8335e67b42637eef630` |
| `cp3-aborted-superseded-scope/` | 20 个文件 | 排除出验收矩阵（见 DEV-08-1） |

**隐私边界**：原始串口日志含 WiFi SSID 名称与 NVS 状态，仅存本地；HLS 全量备份（32 MiB 原厂镜像）与其他候选的日志都在 `.gitignore` 覆盖下，未提交。本报告与矩阵只引用文件名、哈希与已在前序报告中公开过的 SSID 名。**未提交任何凭据、音频或固件二进制。**

已提交的脱敏证据（`integration/v6/`）。**下表 SHA256 一律取"提交内存储内容"**（即 `git show <commit>:<path> | sha256sum`）；`.gitattributes` 规定 `*.json text eol=lf`，故磁盘上若出现 CRLF 与提交内容不同属正常：

| 文件 | SHA256（提交内内容） |
| --- | --- |
| `m0-candidate08-review-cp0-code-host.json` | `38fed2b605366550820305de30595f829b9cdedb1fd4af41f2f1871533a1311e` |
| `m0-candidate08-review-cp1-flash.json` | `efb9765e8f23f2ce79cf28b69fb4411a725cd9863a850f0b8084361afb2e0541` |
| `m0-candidate08-review-cp2-network-fallback.json` | `030c7341eacad53bd800bfb12382365766120926107f2963b9f23049144b0ee3` |
| `m0-candidate08-review-cp2-audio.json` | `73cad08563fd7b62c25f91e1e09c8c0b48b7508fd0e3e3ee6f14633462ce12a9` |
| `m0-candidate08-review-cp3-smoke.json` | `67f337dc33b8185b0dbe9eeaff40935eafe3a628a9dd3b7fe0ec1443361b181c` |
| `m0-candidate08-review-audio-evidence.json` | `35f39c1ea794341984333699cee676729e592f8c160a0bb69674276117dcd187` |

> 修正记录：本表初版给 `audio-evidence.json` 记的是磁盘文件哈希 `bf0933a8…`。该文件由 `tools/v6/audio_evidence.py` 用 Python 文本模式写出，磁盘上是 CRLF，而提交经 `.gitattributes` 归一为 LF，两者必然不同。已改为提交内内容哈希。前 5 个文件由本人以 `newline="\n"` 显式写出，磁盘与提交一致，故未受影响。

## 9. 我不宣称的（防止从本报告外推）

1. **不宣称 M0 总通过**，也不宣称候选 08 可发布。SD 卡 / Camera / 电源键、故障注入、恢复写回均未覆盖。
2. **不宣称音频通过。** 操作者未能提供刺激，本轮**没有**任何唤醒、回放或播放期间采集的候选 08 证据。无刺激窗口不是失败率，也绝不是通过。
3. **不宣称参考通道或 AEC 有结论。** 候选 05 那条关于参考通道的说法已被 Codex 以 `SUPERSEDED` 撤回（旧 harness 在 Playback 阶段关闭了输入消费者，"入队之后"被误当成"播放期间"）；本包未对候选 08 做任何参考通道测量。
4. **不宣称"正常音量无削顶"已验证。** 只能说：在最大输入达 ADC 满量程 **78.92%** 的窗口里 `clipped=0`，且 `peak == raw_peak >> 16` 在 202/202 窗口精确成立。**未施加受控的正常音量语音刺激。**
5. **不把候选 05 的结论当候选 08 结果。** 20 轮复位与 31.3 分钟空载仅作基线引用；6 次唤醒与清晰回放同样只作历史基线，不重复声称。
6. **不宣称冷启动、小时级稳定性或负载稳定性。** USB 复位不是冷启动；三会话各仅 60–100 s。
7. **不宣称网络稳定。** 只验证到本地关联与 IP；未测 NAS 协议，未施加断连刺激。
8. **不宣称那 6 行 W/E 日志无害化处理完毕。** 其中"早于 C5 的 MAC 读取"被明确列为 M1 风险继续跟踪。
9. **不宣称 pin 模式已验证。** 未测。
10. **不宣称 HOST 通过等于真机通过。** Host shim 不模拟异步事件先后、`Stop` 竞争、RF、C5 或 AP 实际行为（工具自身声明）。

## 10. 建议 Codex 的复检重点

按优先级：

1. **`esp_wifi_connect()` 返回值未检查**（`wifi_station.cc:492-497`）—— 是否应与同增量新增的 `StartConnect` 路径对齐？
2. **`vad_observable=` 命名**（`m0_diagnostics.cc:120-126`）—— 建议改为本地播放标志，避免后续依据该字段下错误判定。
3. **音频分项如何闭合** —— 需要一个操作者可发声的窗口；协议已写在 `…cp2-audio.json` 的 `how_to_close`。请确认是否由 Codex 侧安排，或改期重测。
4. **`reference_status = HARDWARE_VERIFY_REQUIRED` 的归属** —— 参考通道应由编解码器回环提供，还是修改声明？这是纯源码/编解码器配置问题，不需要设备。
5. **本轮"回退只在全信道无匹配后触发"是否满足你的设计意图** —— 真机序列与读码结论一致，但**真机这次只覆盖了"无匹配"这一条路径**，`last_scan_used_saved_channels_` 的其他取值未被真机走到。
6. **DEV-08-1 的处理是否认可** —— 18/18 通过的那半截 20 轮序列被排除出验收矩阵，只作旁证。如果你希望它计入，需要你明确指示。
7. **早于 C5 的 MAC 读取** —— M1 若用 MAC 派生设备 ID 会取到不稳定值，请确认修法（延后读取或改用持久化 ID）。

## 11. 交接

- 分支：`workbuddy-v6-m0-candidate08-review`（基点 `1907730926d383072dc267201c6f359c4a001231`，7 笔提交）
- **推送到远端请用连字符名**（任务书原名在本机建不住，见 DEV-08-2）：
  `git -C E:\workbuddy\claw4-v6-wb-c08 push -u origin workbuddy-v6-m0-candidate08-review`
- 报告入口：本文件
- 矩阵：`docs/v6/V6_M0_CANDIDATE08_REVIEW_MATRIX.md`
- **COM7 已释放**，无残留进程占用；构建树 `E:/v6/s1` 只读使用，未重建
- 状态：**REVIEW_READY**，由 Codex 决定 `ACCEPTED` / `CHANGES_REQUIRED`
