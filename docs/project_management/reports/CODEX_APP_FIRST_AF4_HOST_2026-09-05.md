# CODEX-APP-FIRST-001 AF4 主机侧验收报告（未进真机）

## 1. 范围与结论

- 工作流：`CODEX-APP-FIRST-001 / AF4`
- 执行分支：`codex/app-first-mvp-loop`
- 当前头：`a18563a`（本报告提交）
- 结论：`HOST_APP_GATE=PASS`；真实浏览器 smoke 3/3 通过；`APP_FIRST_MVP_LOOP` 暂不标记 `PASS`。
- 真机状态：不需要连接 COM3；未执行 flash、erase、monitor、分区或 bootloader 操作。

本批主机侧完整链路已经跑通。尚未满足阶段收口的一项边界是：受当前工具链所有权/导出元数据限制，无法在全新 IDF build 目录复现 cold build；此前 AF3a 已有同一 `E:\c` 镜像的 `idf.py build` exit=0 证据，未发现产品源码构建回归。

## 2. 本次提交

| 提交 | 内容 |
|---|---|
| `5f5f375` | 修复虚拟设备 runner 在重启后重置事件/会话 ID，改为进程生命周期共享确定性计数器；真实设备仍使用熵源 ID。 |
| `a156980` | Full Gate 支持绝对日志目录、显式 Python/PWA 依赖路径，并把 C++ gate 输出写入同一外部日志树。 |
| `85129a5` | 浏览器真实回归发现并修复 unbound `fetch`；Backend 增加仅限 loopback 开发端口的 CORS 白名单。 |

## 3. 验证证据

### 3.1 故障与重启压力

- `verify-virtual-device-fault-matrix.ps1`：`virtual_fault_rounds=5/5`。
- 自定义 50 次进程重启场景：`restarts=50 complete_ok=True pending=0 active=`，退出码 0。
- 覆盖：在线 seed、Start、离线 pending、网络丢失、重启恢复、Complete、重复投递、ACK 收敛。
- 此前 50 次场景暴露 runner-only 的 ID 复用缺陷；修复后复测通过，未改动产品 ID 语义。

### 3.2 Full Gate

命令使用外部依赖目录（不写入仓库）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/dev/run-app-first-gate.ps1 `
  -Mode Full `
  -CompilerPath E:\workbuddy\toolchains\w64devkit-2.9.1\bin\g++.exe `
  -PythonPath E:\workbuddy\学习习惯培育AI\backend\.venv\Scripts\python.exe `
  -FrontendPath E:\workbuddy\学习习惯培育AI\frontend `
  -LogDir E:\workbuddy\学习习惯培育AI\out\app-first-af4-full-fixed3
```

结果：

- C++ host：PASS；
- Virtual Device App：PASS；
- Backend：`70 passed`；
- PWA：typecheck PASS、Vitest PASS、production build PASS；
- E2E 编排器：`E2E RESULT: PASS`，无残留进程。

### 3.3 浏览器 smoke（真实 Chrome）

- 浏览器：Chrome，PWA dev server `http://127.0.0.1:4174/`，Backend `http://127.0.0.1:8000/`。
- 每轮均执行：开发会话登录 → 概况 → 今日任务 → 学习记录 → 设备；退出并重新登录后进入下一轮。
- 连续 3 轮：`3/3 PASS`。
- 首轮发现的两个真实浏览器问题已在 `85129a5` 修复：原生 `fetch` receiver 绑定、loopback CORS 预检。

### 3.4 固件候选与 manifest

- 已有 AF3a IDF build：exit=0，`E:\b\xiaozhi.bin`，`9,175,856 B`。
- SHA-256：`6d27653a7baa690bdb63e7288a27a5b2b5ad0b347ef5f1b9354fe84b5722aa1f`。
- 最新 manifest：外部 `out\app-first-af4-manifest-final`，记录提交 `a156980`、源码 SHA 和固件 SHA。
- 既有构建报告记录 `ota_1` overflow；本批不改 partition，且仅允许在 `ota_0` 候选边界内继续。

## 4. 未完成项与下一步门禁

1. 全新 IDF build：当前外部 IDF tools 的 `idf_tools.py export` 报 installed versions 元数据缺失，且新目录无法找到 Ninja/交叉编译器；不得通过修改项目配置绕过。应修复工具链环境后再跑一次 cold build。
2. cold IDF build 完成并生成唯一候选后，才向用户发出连接 COM3 的 AF5 屏侧验收通知。

本次尝试的失败证据已保留在外部 `out\app-first-af4-idf-cold*`：全新目录先后因 `idf_tools.py export` 无已安装版本、Ninja/交叉编译器不可导出而停止；使用既有 `E:\b` 构建树的增量命令又因该目录权限拒绝，无法把日志写回。两者均是外部工具/目录权限问题，不应通过修改仓库配置或分区规避。

## 5. 复检重点

- 复核 `5f5f375` 只影响 host 测试 runner，不改变设备生产代码；
- 复核 `a156980` 的路径参数不引入仓库外写入或产品逻辑；
- 复核 cold IDF build 在补齐后再把状态提升为 `APP_FIRST_MVP_LOOP=PASS`。
