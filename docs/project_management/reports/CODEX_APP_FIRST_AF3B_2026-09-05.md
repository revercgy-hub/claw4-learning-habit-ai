# CODEX-APP-FIRST-001 AF3b 报告：诊断 DTO 与候选 manifest

- 任务：`CODEX-APP-FIRST-001 / AF3b`
- 分支：`codex/app-first-mvp-loop`
- 范围：纯 C++ 诊断 view-model + repo/build manifest；未连接设备。

## 交付

- `firmware/main/ui/sync_diagnostics.h/.cpp`
  - 统一 build ID、network、auth pause、pending、last ACK、last error、last sync epoch 字段。
  - 空值显示 `unknown/offline/ready/none/never`，不伪造硬件状态。
- `firmware/tests/unit/ui/sync_diagnostics_tests.cpp`
  - 正常与空状态 12 项断言。
- `tools/dev/write-app-first-manifest.ps1`
  - 记录 Git SHA、关键 source SHA-256、构建 binary SHA-256 和永久禁区列表；manifest 输出在仓库外。

## 验证

- host/interface gate：`15/15 PASS`、interface exit `0`。
- manifest：`E:\b\xiaozhi.bin` 9,175,856 B，SHA-256 `6d27653a7baa690bdb63e7288a27a5b2b5ad0b347ef5f1b9354fe84b5722aa1f`。
- IDF build：AF3a 已记录 exit `0`；AF3b 不改官方 Metalio 源码、分区、bootloader 或 ota_1。

## 阶段结论

AF3 的 host 诊断和候选 manifest 已就绪。AF4 将做干净环境下的在线→离线→重启→恢复→ACK→PWA 全链、5 轮 App E2E、50 次重启和 3 轮浏览器核对；AF4 通过前不安排真机。
