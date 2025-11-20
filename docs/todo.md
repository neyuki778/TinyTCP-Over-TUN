# 基础架构优化

## 1. 网络层优化：从 Trie 到工业级查找算法
- [x] 准备benchmark, 设计unitest, 记录不同算法实现的表现
- [x] 目前的 router.cc 是通过遍历 vector 实现 LPM，复杂度是 $O(N)$。
- [x] 基础拓展：实现一颗二叉前缀树 (Binary Trie)，将查找复杂度降为 $O(W)$ (W为地址长度，32)。
- [ ] 进阶拓展（可选）：实现 DIR-24-8 算法 或 LC-Trie。
- [ ] 理由：这是 Linux 内核和很多硬件路由器实际使用的算法。DIR-24-8 利用空间换时间，通过两次访存就能完成查找，性能极高。

## 2. 传输层补全：实现拥塞控制 (重要)

- [x] 目前的 tcp_sender.cc 主要处理了超时重传和流量控制（依据 window_size）。
- [x] 拓展目标：实现 TCP Reno 或 TCP Cubic（简化版）。
- [x] 具体做法：在 TCPSender 类中增加 cwnd (拥塞窗口) 和 ssthresh (慢启动阈值) 状态变量。
- [x] 实现状态机：Slow Start (慢启动) -> Congestion Avoidance (拥塞避免) -> Fast Retransmit/Recovery (快重传/快恢复)。

## 3. 架构拓展：零拷贝思想的引入 (待选)
- [ ] 目前的 `ByteStream `和 `Reassembler` 大量使用了 `std::string 的 append` 和 `substr`，这会产生大量的内存拷贝。
- [ ] 拓展目标：模拟 Linux 内核 `sk_buff` 或 `DPDK mbuf` 的设计。
- [ ] 具体做法：设计一个 Buffer 类，使用引用计数管理内存。
- [ ] 在协议栈各层之间传递 Buffer 的智能指针，而不是拷贝字符串。
- [ ] 在头部添加协议头（IP Header, TCP Header）时，使用 prepend 逻辑或者“iovec”分散/聚集 I/O，避免为了加个头把整个包拷贝一遍。

# 应用层拓展

### 技术实现路径：从 VPS 到 Chrome

- [ ] **用户动作**：在 Mac 的 Chrome 浏览器输入 `http://<VPS_IP>:8080`。
- [ ] **VPS 内核层**：
  - [ ] Linux 内核收到 TCP SYN 包。
  - [ ] **关键点**：需要配置一条 `iptables` 规则（端口转发），告诉内核：“凡是访问 8080 端口的包，都转发给 `tun144` 接口背后的那个 IP（比如 `169.254.144.9`）”。
- [ ] **链路层接口**：
  - [ ] 内核通过 `TUN` 设备将 IP 数据报“写入”用户态程序。
  - [ ]  `TCPOverIPv4OverTunFdAdapter` 读取到这些原始字节。
- [ ] **TCP 协议栈处理**：
  - [ ] `TCPReceiver` 接收 SYN，建立连接（三次握手）。
  - [ ] **亮点**：如果网络有抖动， `Reassembler` 会自动处理乱序， `TCPSender` 会处理丢包重传。
- [ ] **应用层响应**：
  - [ ]  `minnow-httpd`（修改自 `apps/tcp_ipv4.cc`）调用 `accept()` 拿到连接。
  - [ ] 构造一个简单的 HTTP 响应头（`HTTP/1.1 200 OK...`）加上 HTML内容。
  - [ ] 调用 `socket.write()` 将数据发回去。