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

## 3. 架构拓展：零拷贝思想的引入 (高难度, 待选)
- [ ] 目前的 `ByteStream `和 `Reassembler` 大量使用了 `std::string 的 append` 和 `substr`，这会产生大量的内存拷贝。
- [ ] 拓展目标：模拟 Linux 内核 `sk_buff` 或 `DPDK mbuf` 的设计。
- [ ] 具体做法：设计一个 Buffer 类，使用引用计数管理内存。
- [ ] 在协议栈各层之间传递 Buffer 的智能指针，而不是拷贝字符串。
- [ ] 在头部添加协议头（IP Header, TCP Header）时，使用 prepend 逻辑或者“iovec”分散/聚集 I/O，避免为了加个头把整个包拷贝一遍。

# 应用层拓展