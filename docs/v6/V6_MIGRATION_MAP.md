# V6 源码迁移清单

源树：Learning `5657ebed64ad5962c889fcf90f0979f5891ab862`。以下是源码核对，不是旧分支整体验收。

| 模块 / 当前路径 | 决策 | 新边界 / 验证 |
| --- | --- | --- |
| `firmware/main/learning_domain/` | 保留 | 纯 C++17 reducer；原领域单测 |
| `firmware/main/application/coordinator.*` | 保留 | 状态/事件事务唯一入口；存储失败与快照合并 |
| `firmware/main/sync/` | 保留核心 | outbox、session lease、无锁网络阶段；设备 HTTP/HMAC 另适配 |
| `firmware/main/interaction/dispatcher.*` | 保留策略 | 命令确认边界；不能把远程来源标记 Touch |
| `firmware/main/interaction/stt_mapper.*` | 参考，不纳入新 core | 确定性语义迁到 server pre-LLM router；不从 UI 调用 |
| `firmware/main/interaction/interaction_arbiter.*` | 保留思想/Host | M2.5 接 upstream 音频仲裁，不接管 VoiceSession |
| `firmware/main/time/time_authority.*` | 保留 | M2.5 补 RTC/离线可信时间策略 |
| `firmware/main/reminder/reminder_core.*` | 迁移候选 | Stale/Unsynced 抑制行为必须对 V6 离线要求重验 |
| `firmware/main/ui/presenters.*` | 保留 | 只生成 ViewState |
| `firmware/main/mcp/learning_mcp_host.*` | 契约参考 | M1.5 核对鉴权、幂等、确认和异步结果；暂不链接 |
| `integration/metalio_claw4/host_glue/` | 候选复用 | Domain/Coordinator 组合，不能整体复制设备 runtime |
| `integration/metalio_claw4/device/app/learning_runtime.*` | 拆分 | 保留同步模式；移除 MetalioVoiceSessionPort 成员及屏幕控制语音 |
| `integration/metalio_claw4/device/core/outbox_codec.*` | 保留格式后适配 | NVS 版本、损坏与断电恢复回归 |
| `integration/metalio_claw4/device/ports/metalio_hmac_signer.*` | 重写设备适配 | IDF6 PSA Crypto；签名测试向量 |
| `integration/metalio_claw4/device/ports/metalio_voice_session.*` | 淘汰新路径 | 不进入 V6 链接 |
| `firmware/main/ports/voice_session_port.h` | 淘汰新路径 | 官方 Application / AudioService 所有权 |
| `integration/metalio_claw4/device/learning_screen/` | 重写 UI 接线 | 删除 STT Queue/OnVoicePhrase/abortCloudReply 路由 |
| `integration/metalio_claw4/patches/` | 不重放至新版 | 旧 patch 绑定 ca3aa3fa；新板独立 identity |
| `backend/`, `frontend/` | 保留 | M2 真实任务链；后续扩展计划/错题/记忆 |
| `firmware/tests/`, `tools/dev/` | 保留证据 | 本轮新 CMake 只运行其选定子集，不冒充全部回归 |

Metalio `main/boards/metalio-claw-4/` 的两个 LCD 驱动、GT911、IOExpander、I2S/BT codec、SD、供电需逐个移植。旧板继承 DualNetworkBoard，新基线以 Wi-Fi 为范围；4G/GPS/USB 虚拟盘不随 BSP 顺带启用。不能从其他 P4 开发板复制 GPIO 或 ES8311 假设。

新实现入口：`integration/v6/learning_core/`（已实现）、`integration/v6/baseline.lock.json`（已实现）、`tools/v6/baseline.py`（已实现）；Board Port 目录待 M0-2 创建，尚不存在可刷 V6 board。
