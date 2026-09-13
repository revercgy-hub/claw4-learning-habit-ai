# A03 Runtime/网络并发设计审查

日期：2026-09-13；主agent：Codex；状态：设计方向已审定，派发前按A02最终提交复核白名单。不是设备实时性PASS。

## 1. 当前证据

产品基线7dd6511：`CreateMetalioHttpTransport()`把同步`PerformRequest`交给`Application::Schedule`；`RunOnlineCycle()`在整个`backend_->runOnlineCycle()`期间持`state_mutex_`；学习屏渲染和命令也取此锁。把网络请求放到worker还不能消除第二层UI阻塞。

只读检查本地E:/c依赖得到以下额外事实（需A01将依赖身份写入最终构建manifest）：

- `main/boards/metalio-claw-4/metalio-claw-4.cc`继承DualNetworkBoard。当前API不限于Wi-Fi；不能对所有网络实现一概认定线程安全。
- `DualNetworkBoard::GetNetwork()`转发到current_board；当前SwitchNetworkType保存选择后重启。Wi-Fi分支返回静态EspNetwork。
- `EspNetwork::CreateHttp()`创建独立HttpClient；CreateTcp/CreateSsl分别创建独立对象。可以据此设计Wi-Fi下独占client的worker，但不能把一个Http对象跨任务共享或并发Close。
- `HttpClient::SetTimeout()`仅赋值供读取/响应等待使用；`EspTcp::Connect()`直接`gethostbyname`和阻塞`connect`，没有继承HTTP timeout的显式参数。因此不能声明“设timeout=3秒就保证整个DNS/connect/request在3秒内结束”。
- `EspTcp::Disconnect()`可能等待接收任务退出10秒。销毁或Close也属于网络I/O阶段，不能放回UI锁内。
- `ScheduledHttpTransport`的wait_for超时不会取消排队回调；回调仍可能以后执行。shared_ptr只避免其等待状态UAF，不保证业务取消或请求队列有界。

依赖文件SHA256：esp_network.cc=`c6aada5d37c8a3ce8efb24b5777e8f9355510a9adf305d22a8cfb53e9e2bba3a`；esp_tcp.cc=`0e3647b95261a98a1dc952df41fcb4606fa51bbfb508ce0a802bc07d6b70e11b`；http_client.cc=`ef9085d917650c5fafed0ee56bb914e129ceee5a23a8d55f12c2144ed6cb52a8`。这是源码读取得到的事实，不是本轮硬件测量。

## 2. 决策：分离准备、I/O、应用结果

保持同步client内部易测试的HTTP接口，但其调用只能在独占网络worker。核心状态修改只能在短临界区或唯一状态任务串行执行；不要在解锁后继续拿指向mutable Domain/Outbox的引用。

```text
状态所有者短事务：
  校验bootReady/authPaused
  捕获generation、device_id、child_id
  捕获此次连续pending前缀（稳定event_id/sequence）
  返回不可变request envelope

网络worker（无runtime/UI锁）：
  独占session/client/transport
  authenticate / request / read / close
  生成result envelope；不调用coordinator/LVGL

状态所有者短事务：
  校验generation与请求scope
  校验结果只作用于此次实际发送的事件
  原子保存ACK/deadletter/状态，保留请求后新增pending
  发布不可变diagnostics/UI snapshot
```

最小API方向：把`AppCoordinator::runSyncOnce()`拆出prepare/apply步骤，保留原同步wrapper供Host旧调用兼容；BackendSession负责网络侧身份与编解码，不能继续握住可在网络等待中访问的coordinator引用。具体命名由实施者选择，但原auth_pause、reauth一次、backoff、批次ACK与存储失败不丢事件契约必须回归。

不能只对现有runSyncOnce的transport调用前后unlock/lock：函数中跨I/O保留的引用、reauth callback、diagnostics与session销毁都需显式所有权。

## 3. 并发边界

- 一台设备最多一个在途同步；超时逻辑结果作废后，尚未完成的底层操作仍计入在途，不能再启动无限多worker。
- generation在重配/停用/重建Runtime会话时递增；迟到结果不影响新会话。worker拥有对象直至I/O与回调退出，不能靠删除task释放有回调对象。
- 新pending在请求等待中可以提交，应用结果只处理已发送并被有效确认的连续前缀。拒绝越界ACK、错误设备或重复应用导致的删除。
- auth状态变化不能绕过主agent已审查的auth_pause短路；重新配置授权与网络重试不是一回事。
- UI读取快照，不能等待网络对象清理。NVS本身仍可能有有限提交延迟，需分开测量，不能声称绝对零阻塞。
- 网络队列与结果队列有明确容量，合并周期性刷新请求；UI刷新/语音队列每tick有处理预算，不无限drain。

## 4. 超时与网络适配分层

A03首先交付“网络失败不堵本地交互”的Host确定性证据。Wi-Fi worker复用现有独占HttpClient可以消除主循环阻塞，但HTTP超时不代表DNS/connect的总截止时间。若无法在现有适配层实现可证明取消/总deadline，报告明确此限制，并由主agent另定最小transport扩展；不能偷偷修改managed component、sdkconfig或BSP。

完整A03设备Gate需要测DNS、connect、headers、body、close的耗时/资源边界。unsupported网络类型应显式拒绝并显示诊断，不能把Wi-Fi验证扩大到4G。TLS/证书路径不能因迁移worker而降低验证。

## 5. 白名单与顺序

A02先合入；A03独占runtime/http transport/backend session。主agent准许本ADR对应的`firmware/main/application/coordinator.{h,cpp}` prepare/apply拆分及其测试进入A03白名单；A04必须等待A03完成，不同时修改。学习屏仅允许将state/diagnostics读取迁移为新快照入口，禁止借机改voice/UI外观。全局测试脚本由主agent整合。

如采用新纯C++请求/结果DTO头，可放`firmware/main/sync/`，实现者在报告逐一解释；不允许新增vendor补丁。实际网络运行、镜像build及设备测量留候选门禁。

## 6. 验收矩阵

1. fake transport用barrier挂起，期间触控Start/Pause/Complete和UI快照仍推进；不用等真实20秒来证明。
2. pending批次发出后产生新事件，返回旧ACK只移除旧确认前缀，新事件不丢。
3. 配置切换/退出后旧响应返回，generation拒绝，老client安全回收。
4. 认证失败、仅一次reauth、auth_pause后的transport零调用；再授权后可恢复。
5. 网络超时/重复回调/响应丢失、无效ACK、存储失败、重新进入页面，事件幂等/失败可恢复。
6. 旧wrapper Host测试、wire tests、Virtual Device故障矩阵不回归。
7. 同一候选设备网络离线下触控P95<100ms并记录最大停顿；不把Host并发测试当设备延迟结果。
