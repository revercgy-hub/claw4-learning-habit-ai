# WB-V6-M0-CANDIDATE08-REVIEW

状态 READY，当前唯一 WorkBuddy 活动流。候选05已完成大部分M0基础真机验收；本包只复核候选08新增修订，避免重复20轮复位和30分钟空载。候选08统一包含音频归一化/同步采集、隐藏SSID回退、网络失败恢复。06/07包HOLD且SUPERSEDED，不再执行旧候选刷写命令。

## 输入与职责

从 `codex/v6-foundation @ 1907730926d383072dc267201c6f359c4a001231` 建干净独立分支 `workbuddy/v6-m0-candidate08-review`。另读Codex当前分支上的本任务书、根AGENTS、`docs/v6/V6_M0_CANDIDATE08_BUILD_REPORT.md`、`docs/v6/V6_M0_NETWORK_FAILURE_REPORT.md`、`docs/v6/V6_M0_NETWORK_STAGE_REPORT.md`（任务/报告在实现基点之后），以及 `CODEX_V6_CANDIDATE05_REVIEW_AND_06.md`、`integration/v6/network/README.md`、candidate08 manifest、05报告§8/§9。候选05设备真机报告是基线证据，须按其范围沿用，不能当候选08结果。

允许修改：本流 `docs/project_management/reports/WB-V6-M0-CANDIDATE08-REVIEW_REPORT.md`、`docs/v6/V6_M0_CANDIDATE08_REVIEW_MATRIX.md`、脱敏 `integration/v6/m0-candidate08-review-*.json`。可只读审代码并在自己的工作区运行Host测试；发现缺陷给文件/行号/复现，交Codex修复，不直接改产品、工具、sdkconfig、vendor、managed_components或构建树。

串口COM7与构建产物E:/v6/s1须独占，不能与其它进程并发。原始日志留自己worktree的 `out/v6-device-private/candidate08-review/`，不提交，不上传音频/网络凭据。Codex本阶段未开串口、未刷候选08。

## CP0 READY：独立代码与Host复核

重点：配置失败不能沿用旧参数连接；同步连接失败必须有界退避且不abort；失败扫描完成事件不能误触发隐藏SSID回退；回退触发是否局限全信道无匹配；可见AP优先、pin限制、凭据边界和三项上限；direct状态在首次失败/获得IP/重启站点后的变化；异步断线与配网停止边界；overlay完整性及CMake实际使用路径；音频Playback消费者/本地丢包/恢复wake逻辑。

复核已存在的62项Python、C++ policy及16项生产方法Host证据；重点抽查网络fallback配置失败路径和播放期间采集时序。无需重跑完整套件，也不重建固件。仅当代码改动或复核发现问题时，运行相应定向用例。组件测试以只读 `--source E:/v6/s1` 产生临时Host文件。

发现P0/P1、凭据泄漏、串口占用或产物不一致则停止受影响的设备队列，标CHANGES_REQUIRED并报告。未发现阻塞问题才继续CP1。

## CP1 QUEUED：核对后一次app-only准备刷写

既有用户普通刷机授权继续适用，本包限定一次candidate08 application写入0x200000。不刷其它镜像，不擦NVS，不动分区/C5/eFuse/安全策略，不做恢复写回。核对实际设备P4 rev1.3/32MiB及manifest全部固件文件；核对32MiB备份 `E:/workbuddy/claw4-v6/out/v6-device-private/pre-v6-full-flash.bin` SHA256 `b77343691c97359eaedba4d7d8353ef6df55b10ae2f5f9a13059943c04bb9413`，运行只读flash_plan检查布局。

仅在前述一致且COM7无人占用时，以实际解释器执行：

```text
-m esptool --chip esp32p4 --port COM7 --baud 460800 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size 32MB --flash-freq 40m 0x200000 E:/v6/s1/build/xiaozhi.bin
```

app SHA256须为 `1ef710aec525df3f91142d9ca6188b26e6881e448e72007804515a7f6ae6e7c2`，3,051,504字节。完整ELF见manifest。保留hash verified，采≤60秒启动证据，ELF前缀匹配、BOOT_READY、Assets applied=1、无panic/BOOT_BLOCKED后继续。不匹配/失败立即停止，不尝试其他地址或候选。

## CP2 QUEUED：仅验候选08新增行为

候选05的6次唤醒/rearm和清晰回放作为历史基线，不重复声称为08结果。08只做定向音频差异验证：至少两次3秒录音/回放；确认正常音量下无明显削顶、回放可闻、`INPUT` 在播放时仍持续产生、`tx_overlap_n` 有足够采样、`LOCAL_REFERENCE_PROBE_END drained=1`，并确认播放后唤醒恢复。单次捕获中做4次唤醒词、间隔≥3秒，验证新增持续采集期间唤醒仍工作。记录实际用户刺激及分路日志。没有用户配合记NOT_VERIFIED；不重复20轮唤醒/长期回放，不调PGA。

网络只测候选08新增分支：确认既有配置下重启可关联隐藏SSID并取得IP（若原AP当前不广播）；有可用测试AP时验证扫描无匹配后的bounded fallback及恢复。若没有可安全切换的测试AP，不为测试修改家庭路由/NVS，标NOT_VERIFIED。候选05已有“配网首次关联成功、保存NVS、重启后隐藏SSID重连失败并回配网”基线；08要比对是否修复这一差异。

记录候选启动身份、刺激、fallback标志、关联/IP和配网事件。扫描无匹配不独立证明AP不存在；不测pin模式则标NOT_VERIFIED。隐私原日志不得提交。网络只验本地关联/IP，不验NAS协议。

## CP3 QUEUED：限定烟雾回归

候选05已有同镜像20/20次USB复位和约31.3分钟空载观察（Codex已独立复算日志哈希、180条HEALTH、35秒采集间隙），作为M0稳定性基线引用即可。本包不重复整套复位/长稳。candidate08刷入后已包含一次启动观察；若CP2无panic/BOOT_BLOCKED/明显内存问题，不再跑20轮和30分钟。若发现启动或内存异常，停止并只做定位所需的最小额外捕获。USB复位不能称冷启动。

## CP4 QUEUED：汇总交接

使用audio_evidence的显式SHA/会话清单，每个启动会话首段有匹配ELF，续段不能复位/重叠。事件总量不采用旧hw_matrix的max或last-first；工具通过不能代替操作者确认会话。矩阵分别列CODE/HOST/BUILD/DEVICE证据，保留NOT_VERIFIED和失败。

每checkpoint单独提交，报告修改路径、命令/结果、私密日志文件名和SHA、异常/范围偏差/未覆盖项。最终释放COM7，标REVIEW_READY，给分支HEAD和报告入口；Codex复检后决策。不能自行ACCEPTED、合main或宣称M0总通过。候选05已覆盖的基础启动/复位/空载项目引用原报告，不重新测试。没有开始NAS/M1、SD/Camera/电源键或恢复写回的授权扩展。
