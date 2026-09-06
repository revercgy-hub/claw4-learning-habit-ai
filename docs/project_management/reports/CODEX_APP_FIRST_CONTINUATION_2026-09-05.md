# App-first 继续开发与证据修正

分支：codex/app-first-mvp-loop。基线：5885ad7。

2026-09-06 更新：connected HTTP 与浏览器业务链已补实测；冷构建结束但配置门禁未通过。最新结论见 [真实链路复核](CODEX_APP_FIRST_CONNECTED_2026-09-06.md)。下文待办为当时状态。

## 已实现

PWA App 初始化时把 sessionStorage 中的会话令牌同时传给 ApiClient，解决刷新后界面显示已登录、HTTP 请求却缺少 Authorization 的问题。新增 App 集成测试验证带令牌加载概况、退出清理、再次挂载回到登录页。

直接在活动工作树 frontend 执行 typecheck、lint、test、build：全部通过，31/31 测试。node_modules 使用指向现有依赖的 junction，不再复制修改另一工作树的产品源码来代跑测试。

## 冷构建根因

esp_wifi_remote/Kconfig 按 ESP_IDF_VERSION 选择 Kconfig.idf_v*.in。之前人为指定 5.5.4，但安装目录只有 5.5 等匹配文件，orsource 静默跳过，造成 CONFIG_WIFI_RMT_* 缺失。改为 export 一致的 5.5 后，新构建 config/sdkconfig.h 已生成 CONFIG_WIFI_RMT_STATIC_RX_BUFFER_NUM=16。

本次使用现有 ASCII IDF 路径 E:/workbuddy/esp-idf-5.5.4-ascii，输出 E:/workbuddy/claw4-idf-cold-20260905c，SDKCONFIG 指向输出目录副本。未刷写设备。构建最终结果另行登记；符号恢复不等于最终固件通过。

## 当前仍需实施

1. 将 LearningApp/真实 outbox 接到 Backend HTTP 的同一 connected runner；验证离线→持久化恢复→ACK 收敛。
2. 浏览器创建任务并核对同一场景完成记录；现有三轮导航 smoke 不替代此验收。
3. AF3 设备网络适配、同步诊断和构建镜像映射仍未完成；现有 L1 演示固件不得冠以 L2/L3 候选。
4. 完成上述项目及冷构建、配置一致性、尺寸核查后，才通知用户连接设备。

历史 AF4 报告已标 SUPERSEDED。测试件数量不合并成完整业务链路通过声明。
