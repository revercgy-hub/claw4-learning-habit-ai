# Claw4 学习习惯养成终端——Codex 项目总控 AGENTS.md

## 角色

你现在是本项目的首席软件架构师、ESP32-P4 嵌入式工程师和全栈开发负责人。

项目目标不是开发一个通用 AI 玩具，而是基于 Metalio Claw 4 打造一套面向初中生的：

**“AI 学习计划与学习习惯养成终端”**

系统由以下部分组成：

1. 学生端：Metalio Claw 4
   - ESP32-P4 + ESP32-C5
   - 720×720 触摸屏
   - 麦克风 / 扬声器
   - 摄像头
   - Wi-Fi
   - microSD
   - 其他硬件根据实际 SKU 自动检测，不可假定全部存在。

2. 家长端
   - 初期优先 Web / PWA
   - 后期可封装 Android/iOS

3. 家庭后端
   - 部署在局域网 NAS / Docker 环境

4. AI 服务
   - 后端统一调用
   - 可以配置：
     - 本地 Ollama
     - OpenAI-compatible API
     - DeepSeek-compatible API
     - 其他兼容模型
     - 云端 ASR/TTS/VLM

5. Metalio 官方固件
   - 必须优先继承和复用，不允许没有充分理由就重写 BSP

官方参考资料：

- Metalio 官方文档：https://metalio.cloudzao.cn/docs
- Metalio Claw4 官方 GitHub：https://github.com/CloudZao/MetalioClaw4

开发过程中：

**官方文档 > 官方 GitHub 当前代码 > Espressif 官方资料 > 第三方资料**

遇到文档与代码冲突时：

**以实际代码、sdkconfig、实机检测结果为准。**

禁止根据电商描述自行假定硬件参数。

---

## 一、项目产品定义

本产品不是：

- 儿童手机
- 通用聊天机器人
- 视频监控设备
- 手机替代品
- 游戏机
- 本地运行大语言模型的终端

本产品核心定位：

**“放在孩子学习桌上的主动型 AI 学习伙伴”**

核心价值：

**计划 → 开始 → 专注 → 完成 → 反馈 → 复盘 → 调整**

建立学习行为闭环。

所有设计优先服务于：

1. 降低开始学习的阻力
2. 建立每日学习计划
3. 把复杂任务拆成小步骤
4. 帮助学生进入专注状态
5. 记录真实完成情况
6. 建立正向反馈
7. 家长了解情况但减少过程干预
8. AI 辅助，而不是代替孩子思考

---

## 二、架构原则

必须按照以下逻辑开发：

```text
Claw4
  ↓
Device API / Sync
  ↓
家庭后端
  ↓
业务服务
  ↓
AI Gateway
  ├── ASR
  ├── LLM
  ├── TTS
  └── Vision
  ↓
数据库
```

同时：

```text
家长端
  ↓
家庭后端
```

禁止：

**家长端直接访问 ESP32。**

Claw4 只负责：

- UI
- Touch
- 本地任务状态
- 番茄钟
- 本地提醒
- 麦克风
- 扬声器
- Wake Word / VAD / AEC
- Camera capture
- 网络通信
- 离线缓存
- 设备状态管理

NAS / Backend 负责：

- 用户
- 孩子
- 设备
- 学习计划
- 任务
- 学习 session
- 学习统计
- 积分
- 习惯
- 家长权限
- AI Prompt
- LLM
- ASR
- TTS
- VLM/OCR
- 数据存储
- 日志
- 版本配置

---

## 三、最重要的开发原则

不要一开始开发所有功能。

必须按照：

**MVP → V1 → V2**

迭代。

第一目标永远是：

**“完整闭环可运行”**

而不是：

**“功能多”**

---

## 四、MVP 唯一核心链路

第一阶段必须首先实现：

```text
Claw4 开机
↓
Wi-Fi 联网
↓
设备注册/认证
↓
连接家庭后端
↓
获取当前孩子
↓
GET 今日任务
↓
首页展示今日任务
↓
学生点击“开始”
或语音“开始数学”
↓
创建 StudySession
↓
进入专注计时
↓
显示：
- 任务名称
- 倒计时
- 当前阶段
- 暂停
- 完成
↓
完成任务
↓
生成本地事件
↓
同步 Backend
↓
家长端立即显示：
- 已完成
- 实际学习 XX 分钟
- 完成时间
```

这个链路全部跑通以后：

**MVP 才算完成。**

---

## 五、MVP 暂时禁止开发

除非我明确要求，否则不要提前开发：

- 4G
- GPS
- Zigbee
- Thread
- Matter
- 连续录像
- 持续摄像头监控
- 复杂数字人
- 本地大模型
- 大型游戏系统
- 排行榜
- 社交
- 在线课程
- OCR 批量识别
- 自动批改作业
- 情绪识别
- 人脸识别
- 云端实时视频
- 复杂 Agent 自主操作

---

## 六、固件架构

不要重写 Metalio BSP。

先阅读官方仓库。

必须识别：

```text
main/
components/
boards/
display
audio
camera
protocol
network
storage
power
mcp
openclaw
espclaw
```

等现有结构。

然后建立自己的业务层。

建议结构：

```text
firmware/

  main/

  components/

    metalio_bsp/
      保留官方 BSP

    device_services/
      network/
      audio/
      camera/
      storage/
      power/
      ota/

    learning_domain/
      task/
      study_session/
      timer/
      habit/
      reward/
      reminder/

    sync/
      api_client/
      event_queue/
      offline_store/
      retry/
      time_sync/

    ui/
      screens/
      components/
      models/
      theme/
      assets/

    assistant/
      wake/
      voice_session/
      command_router/

    telemetry/
      metrics/
      crash/
      memory/
```

任何业务层代码禁止：

**直接操作 GPIO。**

业务层只能通过：

**Device Service / BSP abstraction**

访问硬件。

---

## 七、设备状态机

至少建立以下一级状态：

```text
BOOTING
PROVISIONING
CONNECTING
ONLINE_IDLE
OFFLINE_IDLE
TASK_READY
FOCUSING
PAUSED
BREAK
VOICE_LISTENING
AI_PROCESSING
AI_SPEAKING
SYNCING
LOW_BATTERY
UPDATING
ERROR
```

设备业务状态不得完全依赖 UI。

UI 是状态的表现层。

---

## 八、学习任务模型

Task：

```text
id
child_id
title
subject
description
task_type
estimated_minutes
priority
status
scheduled_date
scheduled_start
deadline
source
parent_created
ai_created
created_at
updated_at
version
```

status：

```text
pending
ready
in_progress
paused
completed
skipped
```

---

## 九、StudySession

StudySession 表示：

**孩子真正发生的一次学习行为。**

字段：

```text
id
task_id
child_id
device_id
planned_duration
actual_duration
started_at
ended_at
pause_count
pause_duration
status
completion_type
created_at
updated_at
```

completion_type：

```text
normal
manual
timeout
parent_confirmed
auto_saved
```

重要原则：

**Task ≠ StudySession。**

一个 Task 可以产生多个 StudySession。

---

## 十、事件驱动同步

Claw4 不允许：

**每次直接覆盖服务器 JSON。**

必须设计 Event Sync。

统一事件结构：

```json
{
  "event_id": "uuid",
  "device_id": "...",
  "child_id": "...",
  "sequence": 123,
  "timestamp": 123456789,
  "type": "study.session.completed",
  "payload": {}
}
```

事件至少包括：

```text
device.booted
device.online
device.offline
task.started
task.paused
task.resumed
task.completed
task.skipped
study.session.started
study.session.completed
timer.started
timer.finished
reward.earned
sync.failed
sync.recovered
```

---

## 十一、离线优先

必须支持：

**Wi-Fi 断开时继续学习。**

断网期间 Claw4 仍然可以：

- 查看今日任务
- 开始任务
- 番茄计时
- 暂停
- 完成
- 记录 StudySession
- 获得本地反馈

所有事件写：

**Local Event Queue**

恢复网络：

自动重试。

必须：

**At-least-once delivery**

服务端幂等。

`event_id` 用于去重。

`device sequence` 用于排序。

---

## 十二、API 设计

建议 Backend：

`/api/v1`

DEVICE：

```text
POST /devices/register
POST /devices/pair
POST /devices/auth
GET  /devices/{device_id}/config
POST /devices/{device_id}/heartbeat
```

TASK：

```text
GET  /children/{child_id}/tasks/today
GET  /tasks/{id}
POST /tasks
PATCH /tasks/{id}
```

SESSION：

```text
POST /study-sessions
PATCH /study-sessions/{id}
POST /study-sessions/{id}/finish
```

SYNC：

```text
POST /events/batch
GET  /sync
```

ASSISTANT：

```text
POST /assistant/query
```

WebSocket：

```text
/ws/device
```

上传：

```text
POST /media/image
```

后续：

```text
POST /vision/analyze
```

---

## 十三、家长端 MVP

优先做：

**Responsive Web / PWA**

不要第一版先做 Native App。

页面：

### 1. Dashboard

今天：

- 计划任务
- 完成任务
- 完成率
- 专注分钟
- 当前是否正在学习

### 2. 今日任务

创建任务字段：

- 科目
- 任务
- 预计分钟
- 优先级

### 3. 学习记录

显示：

- 时间
- 任务
- 用时
- 暂停情况
- 完成状态

### 4. 设备

显示：

- 在线状态
- 电量
- 固件版本
- 最后同步时间

MVP 不做复杂统计。

---

## 十四、Claw4 UI

720×720。

UI 风格：

- 极简
- 儿童友好
- 但不能幼稚

主要页面：

```text
HOME
FOCUS
BREAK
DONE
VOICE
OFFLINE
SETTINGS
```

首页参考：

```text
你好

今天还有 4 个任务

数学
完成练习册 P32
预计 25 分钟

[ 开始 ]

英语
背 20 个单词
预计 15 分钟

[ 开始 ]
```

Focus 页面：

```text
数学

练习册 P32

24:36

专注中

[暂停]

[完成]
```

不要复杂全屏粒子动画。

不要频繁 720×720 全屏 alpha blend。

优先：

- 卡片
- 局部动画
- 静态图标
- 局部刷新

---

## 十五、性能约束

注意：

Metalio 仓库当前实际配置和不同硬件 revision 可能存在差异。

不要硬编码：

**ESP32-P4 480 MHz**

必须启动时记录：

```text
CPU revision
CPU freq
PSRAM size
PSRAM freq
Flash
free heap
largest block
```

默认按：

**360MHz 级设备**

做性能预算。

目标：

```text
正常 UI：>= 30 FPS
重负载：>= 15 FPS
Touch feedback：P95 < 100ms
Wi-Fi 恢复：< 5s
24 小时：无 crash
PSRAM 稳态至少留 6MB 安全余量
```

---

## 十六、摄像头策略

MVP：

**摄像头关闭。**

V1 才加入：

**“拍题”**

流程：

```text
用户点击“拍题”
↓
摄像头开启
↓
低分辨率 preview
推荐 640×480
↓
用户点击拍照
↓
临时高分辨率 capture
↓
JPEG
↓
上传 NAS
↓
VLM/OCR
↓
返回结果
```

禁止：

- 常态 1080p Preview
- 持续录制
- 后台持续监控孩子

摄像头必须有明显状态提示。

---

## 十七、语音 AI

语音分为两层。

本地：

```text
Wake Word
VAD
AEC
```

服务器：

```text
ASR
LLM
TTS
```

音频链：

```text
Mic
→ AEC
→ VAD
→ Opus
→ WSS
→ ASR
→ LLM
→ TTS Streaming
→ Claw4 Speaker
```

第一阶段语音只实现少数能力：

- 今天有什么任务？
- 开始数学
- 暂停
- 继续
- 完成
- 还有多久？

优先：

**Intent Command**

而不是所有操作都经过 LLM。

例如：

```text
START_TASK
PAUSE_TASK
RESUME_TASK
COMPLETE_TASK
QUERY_TODAY_TASKS
QUERY_REMAINING_TIME
```

直接进入业务 Router。

---

## 十八、AI Gateway

后端不得绑定某个模型。

定义统一接口：

```text
LLMProvider
ASRProvider
TTSProvider
VisionProvider
```

例如：

```text
providers/
  ollama.py
  openai.py
  deepseek.py
  custom.py
```

环境变量：

```text
LLM_PROVIDER=
LLM_BASE_URL=
LLM_MODEL=
LLM_API_KEY=

ASR_PROVIDER=
TTS_PROVIDER=
VISION_PROVIDER=
```

允许后续后台修改。

---

## 十九、AI 的核心职责

AI 不只是聊天。

未来主要用于：

1. 自动拆学习任务
2. 自动生成学习计划
3. 调整预计时间
4. 学习复盘
5. 鼓励和提醒
6. 解答问题

例如：

“今晚数学做试卷”

拆：

```text
准备文具 2 分钟
选择题 15 分钟
填空题 10 分钟
大题 20 分钟
检查 10 分钟
```

但：

**AI 不得自行把任务标为完成。**

任务完成必须由：

**孩子或家长**

触发。

---

## 二十、激励体系

第一版保持简单。

完成任务：

`+ XP`

连续学习：

`Streak`

专注完成：

`Focus XP`

不要第一版做：

- 复杂商城
- 抽奖
- 排行榜
- PK

反馈形式：

简单动画，例如：

```text
+10 XP

今天已经完成 3 个任务！
```

---

## 二十一、儿童隐私

这是项目最高优先级之一。

默认：

- 不持续录像
- 不持续上传麦克风
- 不做人脸识别
- 不做情绪识别

设备空闲时：

麦克风只允许：

**Wake Word / 本地 VAD**

真正语音数据：

仅在用户主动触发 AI 对话时上传。

图片：

只有用户主动拍摄时上传。

默认不永久保存：

**语音原文件**

图片设置：

**TTL**

家长必须可以：

- 查看
- 删除
- 关闭摄像头和 AI 功能

---

## 二十二、安全

必须使用：

```text
HTTPS
WSS
```

必须验证：

```text
CA
hostname
```

禁止：

`verify=false`

禁止：

所有设备共用永久 API Key。

每台设备：

```text
device_id
+
device secret / private key
```

Backend：

签发短期 token。

正式版规划：

```text
Secure Boot
Flash Encryption
Signed OTA
```

---

## 二十三、OTA

不要立即改官方 partition table。

先分析现有：

**partition CSV**

以及：

**OpenClaw / ESPClaw dual mode**

输出报告：

当前 `ota_0`、`ota_1` 分别实际承担什么。

然后提出：

### A
保持官方双系统

### B
改真正 Firmware A/B OTA

两种方案。

在我确认之前：

**禁止改 partition table。**

---

## 二十四、开发步骤

### PHASE 0：Repository Audit

必须先 Clone / 阅读 MetalioClaw4。

输出：

```text
docs/CLAW4_AUDIT.md
```

内容：

- 目录
- 组件
- BSP
- Display
- Audio
- Network
- Camera
- Storage
- Power
- OTA
- Protocol
- OpenClaw
- ESPClaw
- 哪些可以复用
- 哪些需要修改
- 哪些禁止修改

同时输出：

```text
docs/HARDWARE_ASSUMPTIONS.md
```

所有：

```text
已确认
推测
未知
```

分别标记。

禁止把推测写成事实。

### PHASE 1：Build Baseline

要求：

官方项目能够编译。

建立：

```text
docs/BUILD.md
```

记录：

- ESP-IDF
- toolchain
- dependencies
- sdkconfig
- build command
- flash command
- monitor command

### PHASE 2：Learning Domain

建立：

```text
Task
StudySession
Timer
Reward
Event
```

等纯业务模型。

要求：

这些模块不能依赖 LVGL、Wi-Fi，也不能直接依赖 Metalio BSP。

必须可单元测试。

### PHASE 3：Mock Backend

在没有真实后端时：

先实现 Mock Server。

至少：

```text
GET today tasks
POST events
POST session
```

这样 Claw4 UI 开发不被 Backend 阻塞。

### PHASE 4：MVP UI

先做：

```text
Home
Focus
Done
Offline
```

四页。

使用 Mock data。

### PHASE 5：Network Integration

实现：

```text
HTTPS
REST
WebSocket
device auth
tasks sync
event sync
```

### PHASE 6：Offline

实现：

```text
Local Event Queue
retry
idempotency
offline task cache
```

### PHASE 7：Parent PWA

实现：

```text
创建任务
查看今日任务
查看 StudySession
```

### PHASE 8：Voice

MVP 闭环稳定之后再接：

```text
Wake Word
ASR
Intent
TTS
```

---

## 二十五、后端推荐技术

除非代码环境已有其他约定：

推荐：

```text
FastAPI
PostgreSQL
Redis（可选）
WebSocket
SQLAlchemy / SQLModel
Alembic
Pydantic
Docker Compose
```

家长端：

```text
React
TypeScript
Vite
PWA
```

后端目录：

```text
backend/

  app/
    api/
    models/
    schemas/
    services/
    repositories/
    ai/
    device/
    sync/
    websocket/
    core/

  migrations/
  tests/
```

---

## 二十六、数据库最低模型

至少实现：

```text
users
children
devices
parent_child
tasks
study_sessions
events
rewards
device_configs
```

后期：

```text
habits
plans
reminders
ai_conversations
media
```

---

## 二十七、Docker

必须支持：

```bash
docker compose up -d
```

建议：

```text
postgres
redis
backend
frontend
```

AI 模型：

不强绑定 compose。

通过配置：

接外部 Ollama / API。

---

## 二十八、日志

日志必须结构化。

Device：

```text
DEVICE
NETWORK
SYNC
TASK
AUDIO
CAMERA
POWER
ERROR
```

后端：

```text
request_id
device_id
child_id
event_id
```

禁止记录：

- API Key
- token
- 完整语音
- 儿童敏感信息

---

## 二十九、Telemetry

Claw4 定时采集：

```text
free internal heap
largest internal heap
free PSRAM
largest PSRAM block
CPU
task stack watermark
network
battery
temperature if available
FPS
WebSocket reconnect count
API latency
```

MVP：

只写日志。

V1：

上传 Backend。

---

## 三十、测试要求

不得只测试 happy path。

必须测试：

- Wi-Fi 中断
- 服务器关闭
- 服务器重启
- DNS 错误
- Token 失效
- 任务为空
- 时间同步失败
- local queue 满
- SD 不存在
- 设备突然掉电
- 重复 event
- 重复点击完成
- 番茄钟期间重启
- WebSocket 断开
- 后端 API 500
- UI 快速切换

---

## 三十一、首个 Release 验收标准

必须满足：

```text
设备开机
↓
联网
↓
认证
↓
10 秒级进入可用首页
↓
正确获取任务
↓
开始 Task
↓
运行计时
↓
暂停
↓
恢复
↓
完成
↓
形成 StudySession
↓
发送 Event
↓
Backend 入库
↓
家长 Web 页面刷新后可看到
```

同时：

```text
断网
↓
完成任务
↓
记录不丢失
↓
恢复网络
↓
自动同步
```

满足上述全部要求：

**才可以标记 MVP。**

---

## 三十二、禁止行为

Coding Agent 禁止：

1. 未读官方代码就大规模重构
2. 擅自改变 partition table
3. 擅自升级 ESP-IDF 大版本
4. 删除官方 BSP
5. 把 GPIO 写死在业务代码
6. 把 LLM API Key 放进 firmware
7. 把 NAS 地址硬编码
8. 使用 HTTP 明文
9. 把摄像头默认持续开启
10. 把 AI 作为 Task 完成的最终判断者

---

## 三十三、Codex 工作规则

每一个任务开始前：

先读取：

```text
README
docs/
相关源码
```

不要猜。

如果发现：

官方文档和代码冲突：

**必须报告。**

如果发现：

硬件能力不确定：

标记：

```text
HARDWARE_VERIFY_REQUIRED
```

不能直接写死实现。

每完成一个 Task：

必须输出：

1. 修改文件
2. 核心修改
3. 如何测试
4. 测试结果
5. 仍存在的问题
6. 下一步建议

不能一次性修改大量无关文件。

每个 Commit：

只解决一个明确问题。

---

## 三十四、Git 工作方式

建议：

```text
main
develop
feature/*
fix/*
```

每个重要阶段创建 tag。

例如：

```text
baseline-metalio
mvp-ui
mvp-device-sync
mvp-complete
```

不要把官方原始基线弄丢。

---

## 三十五、第一批必须执行的任务

现在不要立刻写大量业务代码。

按顺序执行：

### TASK 001

完整扫描 MetalioClaw4 仓库。

输出：

```text
docs/CLAW4_AUDIT.md
```

### TASK 002

确认：

```text
ESP-IDF
CPU config
PSRAM
Flash
partition
LVGL
display
touch
network
audio
camera
SD
power
OTA
```

实际实现位置。

输出：

```text
docs/CLAW4_PLATFORM_MAP.md
```

### TASK 003

建立：

```text
docs/ARCHITECTURE.md
```

描述：

```text
Claw4
↔
Backend
↔
Parent
↔
AI
```

### TASK 004

建立项目目录：

```text
learning_domain
sync
ui
assistant
telemetry
```

先建立 interface。

不要立刻填充复杂实现。

### TASK 005

定义：

```text
Task
StudySession
DeviceEvent
DomainState
```

并建立单元测试。

### TASK 006

建立：

**Mock Backend**

返回固定今日任务。

### TASK 007

开发：

**Claw4 Home 页面**

展示 Mock Tasks。

### TASK 008

开发：

**Focus 页面**

实现本地倒计时。

### TASK 009

实现：

```text
task.start
task.complete
```

事件。

### TASK 010

实现：

**Local Event Queue**

执行完 TASK 001～010 后：

**停止。**

生成：

```text
docs/MVP_STAGE1_REPORT.md
```

向我汇报：

- 完成内容
- 项目结构
- 运行方法
- 截图/日志
- 问题
- 风险
- 下一阶段任务建议

不要自行继续开发 Voice、Camera、AI。

---

## 三十六、最终开发目标

最终系统体验：

```text
早晨：

设备：
“今天有 6 个学习任务。”

放学：

孩子：
“今天先做什么？”

AI：
“建议先做数学练习，预计 25 分钟。
完成后休息 5 分钟，再背英语单词。”

孩子：
“开始。”

设备：
进入 Focus。

25 分钟结束：

“完成得不错，要休息 5 分钟吗？”

完成：
记录 StudySession。

家长端：
看到：
数学
25 分钟
已完成。

晚间：
系统生成今日学习总结。
```

但永远记住：

最终产品目标不是：

**“让孩子一直和 AI 聊天”**

而是：

**“让 AI 帮助孩子逐渐建立不依赖 AI 也能持续执行的学习习惯。”**

---

## 当前执行指令

现在首先执行：

**TASK 001**

在修改任何核心代码之前：

先完成仓库审计。

如果你当前无法访问完整 MetalioClaw4 仓库：

立即停止编码，并告诉我需要：

```text
Clone URL
本地路径
或者 GitHub 仓库访问权限
```
