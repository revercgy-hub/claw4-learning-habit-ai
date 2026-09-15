# A05 CP0 REVISION 2 复核结论

审查日期：2026-09-15。审查人：Codex。被审提交：`6d49c72`（远端与 `E:/claw4-a05-build-m0` 本地一致，工作树clean）。本次变化仅为实施报告；产品代码仍为 `19fd979d4222093ff4ce7464e5b58407586594a2`。

**结论：A05 CP0 ACCEPTED（前置取证与报告范围）。** 上次四项审查意见已关闭，不要求WorkBuddy再次重复整改这四项。该结论不等于WB-V53-NEXT-001整流ACCEPTED，不等于CP1放行，更不等于IDF构建或真机通过。

## 四项关闭证据

| 审查项 | 本次核验 | 结论 |
| --- | --- | --- |
| 验收状态被误写 | REVISION 2正文明确更正“可以启动”不等于“已验收”，撤回先前错误声明 | 关闭；本文件首次正式给出CP0 ACCEPTED |
| pin来源误判缺失 | 真实来源为 `E:/workbuddy/学习习惯培育AI/vendor/MetalioClaw4` @ `ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950`；前次Codex已独立验证四文件patch正反向及hash，本次修订明确采用此来源 | 关闭；不必重新寻找或下载上游 |
| 组件仅比对记录 | Codex本次独立用YAML解析锁文件，并直接调用本机IDF组件管理器 `validate_hash_eq_hashdir` 对实际目录内容核验；82/82成功、错误0、多余目录0，只跳过idf伪条目 | 关闭；不是复用 `.component_hash` 文本比较 |
| SingleFlight被允许裁剪 | 模块表与§13.3改为Compiled+Referenced+最终链接证据；设备 `metalio_http_transport.cpp:9/52` 确实include并构造该传输 | 关闭；不得以COMPILED_NOT_WIRED/GC_DISCARDED验收此模块 |

内容核验使用 `E:/workbuddy/claw4-idf-tools/python_env/idf5.5_py3.12_env/Scripts/python.exe` 和其安装的组件管理器，直接读取 `E:/c/dependencies.lock` 及 `managed_components`。实际输出：

```text
unexpected_dirs= []
verified= 82 expected= 82 errors= []
exit=0
```

这证明当前受组件管理器算法覆盖的内容符合锁文件，不证明其与历史某次构建逐字节相同。历史集合缺独立快照不再作为无期限阻塞：后续把当前已核验集合登记成新候选的明确输入，不能称“原历史产物已复现”。原始全文件清单仍按构建任务书冻结。

## 既有证据与本次范围

前次已核对CP0的29/29 Host日志、49头/5实现/1契约语法日志和5个受阻程序的实际文件hash。6d49c72相对0046aae只改报告，未改变测试输入；本次没有重复编译全部Host套件。保留Codex早前24/29、5项启动被应用控制阻止的历史记录，不改成当时通过，也不声称环境故障已永久解决。

本次未执行IDF configure/build、Flash、真NVS操作，未修改实现或构建脚本。WorkBuddy仍为具体实施者。

## 执行规则裁定

1. 采纳输入分类、Host证据复用和生成分区表反解三项方向，并在任务书落实。外部输入不强制入Git，必须manifest冻结与构建前复验。
2. Host复用必须覆盖源码、测试、门禁、实际参数及工具链身份；报告中的两个g++启动器hash不能单独证明完整工具链未变。新增指纹脚本本身进入输入集后，应重建基线，不能沿用旧指纹冒充相同。
3. 分区CSV保持原字节，不补写空白offset；使用相同分区表offset与Flash大小解析生成物，比较完整布局。app≤4MiB只能说明两个槽的容量均可容纳，**不能据此断言双槽OTA功能可用**；OTA流程/启动选择/回滚能力不在本包验收内。
4. 接受本地 `workbuddy-a05-build-m0` / 远端 `workbuddy/a05-build-m0` 的命名偏差；不要求为恢复斜杠分支反复试验。所谓“引用被外部删除”的原因仍未证实，不将推测当根因；禁止重置/覆盖现有工作来试修环境。
5. CP1把现有组件校验工具整理入白名单时，空输入、缺目录、缺锁hash、异常及任一不匹配必须非零退出，只允许跳过明确的idf伪条目。本次数据实际82/82通过，但现有临时脚本的退出条件未包含全部这些失败类别，不能原样当正式门禁。

## 当前放行状态

CP0已验收，三条执行规则已落任务书。**CP1仍为QUEUED**，剩余前置是Codex对WB-V53-NEXT-001的整流验收；本文件没有用CP0数据盘点替代整流代码审查。WorkBuddy保持等待，不再重复四项CP0整改，也不开始CP1/CP2。下一审查焦点应转回前序整流，而不是继续循环取证。
