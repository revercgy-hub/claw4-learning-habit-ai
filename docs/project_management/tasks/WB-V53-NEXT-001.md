# WB-V53-NEXT-001：首批收口后的WorkBuddy实现与Codex审查

日期：2026-09-14。用户最新决定：当前A01/A02/B01由既有Luna/medium子agent完成；后续由WorkBuddy实施，Codex负责代码审查、架构与疑难问题。

本任务包由Codex准备，**不表示已向WorkBuddy应用发送消息或启动外部执行**。领取条件和最终基线以当前看板为准；CODEX-V53-WAVE1未收口前不得领取。

## 1. 基线与工作方式

输入分支：`origin/codex/v53-foundation-wave1`；以Codex首批收口报告记录的不可变SHA为代码基线。领取时fetch核对并记录实际Base SHA；若发现该SHA后有新代码，先报告差异，不能自行更换任务基线。新建独立 `workbuddy/v53-next-001-reliability` 分支/工作区；不写原HEAD损坏目录，不占用现有E:/c镜像。

必读：根AGENTS、TASK_BOARD、ARCHITECTURE_V5_3、首批收口报告、A01/A02/B01报告、`V53_A03_RUNTIME_CONCURRENCY_ADR.md`、旧工作包对应A03/A04/B02/B03，以及实际源码和测试。历史报告中的Flash/reset/清库命令只是证据，不执行。

顺序实施下列checkpoint，每项单独提交和普通push，报告Base/New SHA、文件、测试命令/结果、已知限制。通过本项验证后可以继续本包下一预授权checkpoint，Codex按不可变提交异步审查；发现测试失败、越范围或架构冲突，停止受影响依赖，不跳过失败。WorkBuddy不得自标ACCEPTED、合main或自行扩展下一包。

主agent对普通实现缺陷可在独立codex分支直接修复；跨模块接口/并发/数据恢复问题由主agent审定。全局构建脚本在WorkBuddy本流内是单写者，不能同时另开一个agent修改。

## 2. CP0：领取与证据复核

- 确认首批三个包审查通过及准确SHA，运行现有Host Gate；读取A01已形成的补丁清单和仍待A05验证的硬件边界。
- 锁定A03要改的接口和UI读取入口，对照并发ADR给出精确文件列表；确认无需修改vendor/BSP/sdkconfig。
- 交付`docs/project_management/reports/WB_V53_NEXT_001_REPORT.md`初始段与源基线。若Gate环境缺失，使用报告里的实际编译器路径，不凭PATH查询认定未安装。

## 3. CP1：网络请求与状态提交分离（A03）

目标：断网/DNS/连接/读取等待不能阻塞学习状态与UI。必须同时移除Application主循环同步I/O、Runtime共享锁跨网络等待两个问题。

允许：`firmware/main/application/coordinator.{h,cpp}`；`firmware/main/sync/`中http transport、scheduled transport、backend session和必要请求/结果DTO；`integration/metalio_claw4/device/app/learning_runtime.{h,cpp}`、`device/ports/metalio_http_transport.{h,cpp}`；学习屏仅状态/diagnostics读取入口；对应tests/fakes和验证脚本注册。

要求：prepare不可变请求→无状态锁I/O→校验generation/scope后应用结果；独占client、单飞、有界队列；旧会话回应不得写新状态；ACK只能删除实际已发送且被合法确认的连续前缀，新pending保留。保留旧Host wrapper兼容，不能把auth_pause/重新认证/死信写失败契约删掉。

验收：barrier挂起网络期间本地命令/UI快照继续；并发新增事件+旧ACK；切换会话后的迟到响应；401/重试/backoff；内存/存储失败不丢数据。当前upstream HTTP timeout不能直接约束DNS/connect全过程；在报告拆分“UI不堵”与“底层总deadline”证据，未解决不得宣称完整Device Gate PASS。不得通过改底层managed component或关闭TLS完成本包。

## 4. CP2：服务端快照不覆盖离线执行终态（A04）

允许：coordinator、backend session、必要wire codec与对应tests。先写回归：本地离线Complete/Skipped、pending未ACK、服务端旧Ready快照先到、上传失败/成功；确认现状后最小修复。

验收：Completed不复活；active改期/删除不丢会话；空权威快照与请求失败不同；旧响应/401/重复ACK/重启保持幂等；最终收敛一致。仅调换GET和POST顺序不作为完整修复。后端新增完整TodayPlan schema留C01，不在此阶段大改数据库。

## 5. CP3：音频与提醒仲裁Host（B02）

允许：`firmware/main/interaction/interaction_arbiter.{h,cpp}`、voice port必要兼容扩展、fakes/tests。

验收：Critical优先；普通TTS等待本地提醒最多3秒预算；不双播；迟到session消息拒绝；用户明确意图且scope匹配才允许Snooze/ACK，ACK不是Complete；主动AI不能伪造授权。纯Host，不接官方AudioService或播放真实音频。

## 6. CP4：Reminder Host与持久化恢复（B03）

允许：`firmware/main/reminder/`、平台无关ReminderWakePort、fake store/output/wake与tests；使用首批B01最终TimeAuthority接口，不重新创建另一套Clock。

验收：TaskDue/Snooze/FocusEnd/DailyReview；稳定实例ID；rebuild不重置Snooze；改期/删除/完成取消旧提醒；跨日/静默期/grace/多提醒合并/上限；保存失败与呈现前后掉电的有界恢复；时间失信/回拨/前跳不重响历史。不能承诺物理铃声严格恰好一次；关键任务事件的容量不能被提醒遥测挤占。

## 7. CP5：主机联合Gate与审查交付

把CP1–CP4纳入既有Host验证清单，必要扩展Virtual Device合成fixture；使用隔离数据与loopback，不连接真实家庭后端。测试Startup→离线学习→旧快照→同步/ACK→提醒状态恢复；明确每个mock与实际C++路径。

交付完整报告、所有checkpoint SHA、已知风险与下一候选建议，状态REVIEW_READY。**到这里停止实现**，由Codex完成代码审查后再安排C01/C02/C03或单一设备候选。不得自行开始SNTP设备集成、Light Sleep、NAS部署、MCP设备注册、AI增强。

## 8. 禁止事项与审查重点

- 不操作Flash/真实NVS/数据库清理；不改partition/bootloader/ota_1/C5/srmodels/eFuse；不改vendor/BSP/sdkconfig或占用共享镜像目录。
- 不引入付费服务、不上传儿童数据、不降级证书/鉴权；测试数据仅合成。
- 不把编译/Host PASS写成真机结果；exe被Windows应用控制阻断时不得用旧退出码假通过或绕过策略。
- Codex重点审：所有权与锁、generation生命周期、ACK边界、pending终态保护、时间质量与误差预算、提醒实例幂等、故障测试是否真正覆盖生产代码、产物清单是否完整。

## 9. 给WorkBuddy的简短启动说明

“请从Codex首批收口报告指定SHA建立独立工作分支，完整阅读AGENTS、看板和WB-V53-NEXT-001，按CP0到CP5顺序实施，每checkpoint验证、单独提交并push，Codex按SHA审查。不要重复开发已验收的A01/A02/B01，不做真机/Flash/vendor配置操作，CP5后停止并交REVIEW_READY。”
