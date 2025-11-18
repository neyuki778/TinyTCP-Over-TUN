### TCPPeer 握手/挥手管理思考

- **TCPPeer 角色**：组合 TCPSender 和 TCPReceiver，管理一个 peer 的全双工收发。没有独立的状态机模块。
- **握手/挥手实现**：状态变化在 sender_/receiver_ 内部自然完成（e.g., sender 发 SYN/FIN，receiver 生成 ACK）。
- **调度机制**：
  - `receive()`：拆分消息给 receiver_/sender_，设置 need_send_ 触发 ACK。
  - `push()`：驱动 sender_ 发送段，补上 receiver_ 的 ACK/window。
  - `tick()`：时间驱动重传，更新 cumulative_time_。
  - `active()`：基于 sender_active/receiver_active/linger 判断连接存活。
- **linger 逻辑**：模拟 TCP 延迟关闭，基于时间差和流结束状态。