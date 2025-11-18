### 2025-11-18 记录

- **socket.write → TCPSender**：`CS144TCPSocket` 把写入交给 `TCPMinnowSocket` 内部的 socketpair，事件循环的三条规则分别负责网络入段、用户写入推送和读取重组数据，最终 `_tcp->push()` 调用 `TCPSender` 发段，由 `_datagram_adapter` 写出；ACK/数据回程由 rule1 → `TCPPeer::receive` → `TCPSender`/`Reader` 处理。
- **apps/endtoend.cc**：`NetworkInterfaceAdapter` 把 TinyTCP 接到用户态 Router/Internet 模拟，主线程跑应用读写，`network_thread` 的 `EventLoop` 在 host/router/internet 四个方向搬帧（含调试输出、tick），实现 client/server 均可运行的端到端验证。
- **scripts/tun.sh**：为 `tun144/145` 做生命周期管理：`start` 创建 TUN 设备、配置 `169.254.X.1/24`、设路由 `rto_min 10ms`、增加基于 connmark 的 NAT 并开启 `ip_forward`；`stop` 清理、`restart` 重建、`check` 只修复异常。`./scripts/tun.sh start 144` 是运行 TCP 栈前的必备步骤，赋予用户态栈发送原始 IP 报文的通路。
### TCPPeer 握手/挥手管理思考

- **TCPPeer 角色**：组合 TCPSender 和 TCPReceiver，管理一个 peer 的全双工收发。没有独立的状态机模块。
- **握手/挥手实现**：状态变化在 sender_/receiver_ 内部自然完成（e.g., sender 发 SYN/FIN，receiver 生成 ACK）。
- **调度机制**：
  - `receive()`：拆分消息给 receiver_/sender_，设置 need_send_ 触发 ACK。
  - `push()`：驱动 sender_ 发送段，补上 receiver_ 的 ACK/window。
  - `tick()`：时间驱动重传，更新 cumulative_time_。
  - `active()`：基于 sender_active/receiver_active/linger 判断连接存活。
- **linger 逻辑**：模拟 TCP 延迟关闭，基于时间差和流结束状态。