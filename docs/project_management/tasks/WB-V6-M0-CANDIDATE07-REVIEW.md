# WB-V6-M0-CANDIDATE07-REVIEW

> 后续源码增量：Codex新增网络失败恢复，见 `docs/v6/V6_M0_NETWORK_FAILURE_REPORT.md`。本包仍只验证固定 f47afda / candidate07，不能将新源码或16场景结果说成07已包含。增量可另行记录代码复核意见，DEVICE验收等待后续统一候选；不要用当前分支的freeze/build工具覆盖E:/v6/s1。

状态 HOLD / SUPERSEDED：用户要求合入网络失败恢复统一构建，当前构建树已升级，禁止再执行本包07刷写命令；后续转候选08复核包。以下旧任务保留。用户要求 Codex 完成一阶段再统一复核；网络恢复与统一候选阶段已完成源码/Host/BUILD，现交复核。05旧流待修项和06未执行测试统一承接；06包 HOLD，不再执行旧刷写命令。

## 输入与职责

从 `codex/v6-foundation @ f47afda7a7fc2f2175e25a10f40fd65e9268d08b` 建干净独立分支 `workbuddy/v6-m0-candidate07-review`。另读Codex当前分支上的本任务书、根AGENTS、`docs/v6/V6_M0_NETWORK_STAGE_REPORT.md`（任务/报告在实现基点之后），以及 `CODEX_V6_CANDIDATE05_REVIEW_AND_06.md`、`integration/v6/network/README.md`、candidate07 manifest、05报告§8/§9。

允许修改：本流 `docs/project_management/reports/WB-V6-M0-CANDIDATE07-REVIEW_REPORT.md`、`docs/v6/V6_M0_CANDIDATE07_REVIEW_MATRIX.md`、脱敏 `integration/v6/m0-candidate07-review-*.json`。可只读审代码并在自己的工作区运行Host测试；发现缺陷给文件/行号/复现，交Codex修复，不直接改产品、工具、sdkconfig、vendor、managed_components或构建树。

串口COM7与构建产物E:/v6/s1须独占，不能与其它进程并发。原始日志留自己worktree的 `out/v6-device-private/candidate07-review/`，不提交，不上传音频/网络凭据。Codex本阶段未开串口、未刷候选07。

## CP0 READY：独立代码与Host复核

重点：回退触发是否局限全信道无匹配；可见AP优先、pin限制、凭据边界和三项上限；direct状态在首次失败/获得IP/重启站点后的变化；异步断线与配网停止边界；overlay完整性及CMake实际使用路径；音频Playback消费者/本地丢包/恢复wake逻辑。

运行62项Python unittest、C++ policy测试与 `test_network_station.py` 七场景（命令见阶段报告）。使用 `E:/workbuddy/claw4-v6/toolchains/idf61/python_env/idf6.1_py3.12_env/Scripts/python.exe`。组件实码测试以只读 `--source E:/v6/s1` 产生临时Host文件，不重建固件。

发现P0/P1、凭据泄漏、串口占用或产物不一致则停止受影响的设备队列，标CHANGES_REQUIRED并报告。未发现阻塞问题才继续CP1。

## CP1 QUEUED：核对后一次app-only准备刷写

既有用户普通刷机授权继续适用，本包限定一次candidate07 application写入0x200000。不刷其它镜像，不擦NVS，不动分区/C5/eFuse/安全策略，不做恢复写回。核对实际设备P4 rev1.3/32MiB及manifest全部固件文件；核对32MiB备份 `E:/workbuddy/claw4-v6/out/v6-device-private/pre-v6-full-flash.bin` SHA256 `b77343691c97359eaedba4d7d8353ef6df55b10ae2f5f9a13059943c04bb9413`，运行只读flash_plan检查布局。

仅在前述一致且COM7无人占用时，以实际解释器执行：

```text
-m esptool --chip esp32p4 --port COM7 --baud 460800 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size 32MB --flash-freq 40m 0x200000 E:/v6/s1/build/xiaozhi.bin
```

app SHA256须为 `6d1e169a09778a76012c2b945a05e7d4fabe1985baa468eaf6e8b2b1c05960d1`，3,051,184字节。完整ELF见manifest。保留hash verified，采≤60秒启动证据，ELF前缀匹配、BOOT_READY、Assets applied=1、无panic/BOOT_BLOCKED后继续。不匹配/失败立即停止，不尝试其他地址或候选。

## CP2 QUEUED：一次协调音频与网络刺激

音频承接06包：10秒安静、20–30cm正常音量4次唤醒（间隔≥3秒）、至少2次录3秒/回放。记录用户实际刺激和听感、事件/rearm、分路INPUT、tx_overlap、PROBE_END drained状态；播放时INPUT须持续，结束恢复唤醒。没有用户刺激不计算失败率，无重合样本不判参考硬件失败。0/4、无声、read_failures、drained=0或崩溃，停止设备后续队列并留证，不调增益。

网络与用户协调在可用的测试AP进行：可见SSID启动关联+IP；同一保存凭据的隐藏SSID重启连接；短时关AP再恢复观察重连；不存在AP/错误凭据下是否有界回到配网。仅用户愿意提供/操作的测试网络，不自行修改家庭路由器或打印密码；不清NVS，不替换唯一家庭凭据。缺少测试AP或用户不便时相应行NOT_VERIFIED，不阻塞独立观察。

每个场景分别记录网络设置由谁确认、刺激起止、候选启动身份、fallback标志、关联/IP、配网事件。扫描无匹配不独立证明AP不存在。未测试pin模式不写已验证；隐私原日志不得提交。网络验证只到本地关联/IP，本候选没有NAS协议连接。

## CP3 QUEUED：复位与观察

CP1通过且CP2无阻塞失败时，candidate07执行20轮USB复位（repeat_boot），随后30份60秒空载观察，不模拟硬件故障。记录wall-clock/uptime/采集缺口；180健康样本只是期望，不补造。异常立即停止；USB复位不称冷启动，空载不称语音负载稳定。无联网时不称网络稳定通过。

## CP4 QUEUED：汇总交接

使用audio_evidence的显式SHA/会话清单，每个启动会话首段有匹配ELF，续段不能复位/重叠。事件总量不采用旧hw_matrix的max或last-first；工具通过不能代替操作者确认会话。矩阵分别列CODE/HOST/BUILD/DEVICE证据，保留NOT_VERIFIED和失败。

每checkpoint单独提交，报告修改路径、命令/结果、私密日志文件名和SHA、异常/范围偏差/未覆盖项。最终释放COM7，标REVIEW_READY，给分支HEAD和报告入口；Codex复检后决策。不能自行ACCEPTED、合main或宣称M0总通过。没有开始NAS/M1、SD/Camera/电源键或恢复写回的授权扩展。
