# WorkBuddy 当前执行入口

2026-09-14更新：A01/A02/B01子agent批次已完成代码与Host范围验收。后续由WorkBuddy实施，Codex负责代码审查、架构和疑难问题。旧WB-001入口归档于 `INSTRUCTIONS_PRE_V53_20260914.md`，仅作历史参考，不得领取。

当前唯一工作流：**WB-V53-NEXT-001 / READY**。开始前完整阅读：

1. 根 `AGENTS.md` 与 `docs/project_management/TASK_BOARD.md`。
2. `docs/project_management/reports/CODEX_V53_WAVE1_CLOSEOUT_2026-09-14.md`。
3. `docs/project_management/tasks/WB-V53-NEXT-001.md` 和其中所有必读输入。

fetch `origin/codex/v53-foundation-wave1`，记录交接HEAD完整SHA；核对相对冻结代码 `e70c2830d1f7ea43629a61ce01a33f78b6f11128` 只有本次文档收口更新。建立独立 `workbuddy/v53-next-001-reliability` 分支/工作区，不从落后main或原HEAD损坏目录开始，不占用E:/c。

按任务书CP0至CP5顺序执行。每checkpoint验证、独立提交、普通push并更新本流报告；Codex按不可变SHA审查。通过本项后可继续下一已预授权checkpoint；失败、越范围或架构冲突时停止受影响依赖。CP5交REVIEW_READY后停止，不自标ACCEPTED、不合main、不扩展下一包。

不做真机/Flash/真实NVS/数据库清理，不改vendor/BSP/sdkconfig/partition/bootloader/ota_1/C5/srmodels/eFuse，不上传儿童数据。旧报告里的刷机、reset、清库和整文件checkout命令仅为历史证据。工具链位置与当前验证命令见收口报告，不凭PATH或旧memory推断工具未安装。

本入口是可领取说明，不表示Codex已经向WorkBuddy应用发送消息或启动执行。
