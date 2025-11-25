### 核心任务：掌握阻塞 / 非阻塞 / 回调式 I/O 思维

**目标：** 通过一组小 demo，从最原始的阻塞 echo 到回调驱动的事件循环，体会"阻塞 vs 非阻塞"、忙轮询的代价，以及回调如何解耦业务逻辑。

---

## 🎯 前置知识检查

**如果你已经完成了 `demo/learn_epoll` 的学习路径，建议采用以下快速路径：**

### 快速路径（推荐给已学习 epoll 的同学）

```
⏭️  跳过 Demo 0（已在 learn_epoll 学过）
  ↓
Demo 4: 回调驱动的事件循环 ⭐️ —— 重点！现代网络库核心思想
  ↓
Demo 1: 阻塞 + 多线程 —— 对比：线程 vs 事件驱动
  ↓
Demo 2: 非阻塞忙轮询 —— 对比：忙轮询 vs 事件通知
  ↓
⏭️  跳过 Demo 3（已在 learn_epoll 学过 select）
  ↓
Demo 5: 回调 + 定时器（进阶）—— fd 事件 + 时间事件融合
```

**学习重点：**
- 🔥 **Demo 4（回调驱动）是最重要的新内容**：理解事件循环 + 回调的解耦思想
- Demo 1 和 Demo 2 帮助你理解"为什么需要事件驱动"
- Demo 5 是进阶内容，理解定时器如何融入事件循环

---

## 📚 完整学习路径（从零开始）

如果你是第一次学习 I/O 模型，或者想系统梳理一遍，按以下顺序学习：

```
Demo 0: 阻塞版（单连接）—— 彻底卡住主线程
  ↓
Demo 1: 阻塞 + 线程池/线程 per connection —— 用线程掩盖阻塞
  ↓
Demo 2: 非阻塞忙轮询 —— 感受 CPU 100% 的代价
  ↓
Demo 3: 非阻塞 + select —— 第一次使用"就绪通知"
  ↓
Demo 4: 回调驱动的事件循环（基于 select）⭐️ —— 逻辑解耦
  ↓
Demo 5: 回调 + 定时器（可选）—— 把时间维度也纳入事件循环
```

---

## 📖 详细 Demo 说明

### Demo 0：阻塞版（单连接）

> 💡 **快速路径提示：** 如果已学习 `learn_epoll/demo0`，可跳过此 demo

- **学习目标：** `accept/read/write` 全阻塞，亲身体会"一人卡死，全场等待"。
- **你需要做的：**
  1. 监听 8888 端口，只处理一个客户端；`read` 返回 0 时退出。
  2. 客户端不发数据时，`read` 卡住；新客户端连不上（因为主线程一直在 `read`）。
- **体验点：** `nc 127.0.0.1 8888` 打开后不输东西，另一个终端 `nc` 会卡在连接。

---

### Demo 1：阻塞 + 线程（掩盖阻塞）⭐️ 新内容

- **学习目标:** 通过线程把阻塞"分摊"出去，同时看到线程的开销。
- **为什么重要：** 理解"线程 vs 事件驱动"的权衡，这是理解 Node.js/Nginx 架构的前提。
- **你需要做的：**
  1. 仍然阻塞式 `accept`；每接到一个连接，`pthread_create`/`std::thread` 开一个线程去阻塞 `read/write`。
  2. 记录线程数并打印 `pthread_self()`，观察 1000 个连接时的内存/上下文切换。
- **体验点：** 功能正常，但线程爆炸、切换成本高，且仍然是"阻塞 I/O"。
- **对比思考：** 为什么 Apache (多线程) 不如 Nginx (事件驱动) 高效？

---

### Demo 2：非阻塞忙轮询 ⭐️ 新内容

- **学习目标：** 理解"非阻塞 + 自己反复 poll"会把 CPU 打满。
- **为什么重要：** 体会"非阻塞 ≠ 高效"，理解为什么需要 select/epoll 这类机制。
- **你需要做的：**
  1. `listen_fd` + 已连接 fd 全部 `O_NONBLOCK`。
  2. `while(1)` 里不断 `accept` 新连接，遍历所有连接 `read`/`write`；遇到 `EAGAIN/EWOULDBLOCK` 就跳过。
  3. 不要 `sleep`，感受 CPU 飙升；再加一个小 `usleep(1000)` 对比效果。
- **体验点：** 功能上可行，但效率糟糕；这是非阻塞的第一个坑。
- **对比思考：** 用 `top` 命令观察 CPU 占用，对比 Demo 3 的差异。

---

### Demo 3：非阻塞 + select

> 💡 **快速路径提示：** 如果已学习 `learn_epoll/demo1`，可跳过此 demo

- **学习目标：** 用"就绪事件"避免忙轮询，理解 `select` 的等待语义。
- **你需要做的：**
  1. 延续 Demo 2 的非阻塞 fd 设置。
  2. 用 `select` 监听 `listen_fd` + 所有连接，可读时才去 `accept/read`；保持写也是非阻塞。
  3. 体验 `fd_set` 每次都要重建的繁琐，但 CPU 下降了。
- **体验点：** 看到"非阻塞 + 等待就绪"才是正确打开方式。

---

### Demo 4：回调驱动的事件循环（基于 select）⭐️⭐️⭐️ 最重要

- **学习目标：** 把"事件处理"抽成回调，解耦 I/O 与业务。
- **为什么最重要：**
  - 这是 libevent/libuv/Node.js/muduo 的核心架构
  - 理解 TinyTCP 主项目的 EventLoop 设计的关键
  - 从"面向过程"到"事件驱动"的思维转变
- **你需要做的：**
  1. 写一个极简事件循环：`register_fd(fd, EVENT_READ, cb)`，内部用 `select` 驱动。
  2. `cb` 类型形如 `void (*)(int fd, void* arg)`；在回调里完成 `read/echo`，需要时注册写事件回调。
  3. 把 Demo 3 的逻辑迁移到回调上，主循环只负责分发事件。
- **体验点：** 体会"事件分发器 + 回调"的可组合性，为后续 libevent/libuv 奠基。
- **架构设计：**
  ```c
  // 事件循环核心结构
  typedef void (*event_callback)(int fd, void* arg);

  struct EventLoop {
      fd_set readfds;
      fd_set writefds;
      event_callback callbacks[MAX_FDS];
      // ...
  };

  // 用户代码只需注册回调
  register_fd(listen_fd, EVENT_READ, on_accept, NULL);
  register_fd(client_fd, EVENT_READ, on_read, client_data);
  ```

---

### Demo 5：回调 + 定时器（可选进阶）⭐️ 新内容

- **学习目标：** 在事件循环里引入"时间事件"，理解网络库的定时器实现思路。
- **为什么重要：** 理解"fd 事件 + 时间事件"的统一调度，这是完整事件循环的必备能力。
- **你需要做的：**
  1. 维护一个小根堆或按到期时间排序的队列，提供 `register_timer(ms, cb)`。
  2. `select` 的 timeout 使用下一个到期时间；到期后执行回调（如心跳、超时关闭）。
  3. 验证：设置 5 秒超时，长时间不发数据的连接被关闭。
- **体验点：** 看见"fd 事件 + timer 事件"是事件循环的基本组合。
- **应用场景：** 连接超时、心跳检测、限流、缓存过期等

---

## 建议的文件/命名

- `demo0_blocking_single.c`
- `demo1_blocking_threads.c`
- `demo2_nonblock_busyloop.c`
- `demo3_nonblock_select.c`
- `demo4_event_loop_select.c`
- `demo5_event_loop_timer.c`（可选）

保持与 `demo/learn_epoll` 一样的端口（默认 8888），方便对比。

---

## 编译与运行示例

```bash
# 在 demo/learn_blocking 目录
gcc -Wall -Wextra -O2 -o demo0_blocking_single demo0_blocking_single.c
./demo0_blocking_single

# 其他 demo 依次替换文件名即可
```

> macOS 也可直接跑 0~4；Demo 5 若用高精度定时器，注意使用 `clock_gettime` 的兼容性。

---

## 如何测试

- **手动：** `nc 127.0.0.1 8888`；开多个终端，观察不同 demo 的表现（连接是否被拒、CPU 占用、延迟）。
- **CPU 对比：** `top`/`htop` 看 Demo 2 与 Demo 3 的差异。
- **并发体验：** `seq 1 50 | xargs -P 50 -I {} sh -c 'echo hi | nc 127.0.0.1 8888'`，比较各 demo 的响应。
- **超时验证（Demo 5）：** 长时间不发数据，确认超时回调触发、连接被关闭。

---

## 学习检查清单

- [ ] Demo 0：阻塞 `read` 如何让新连接进不来？
- [ ] Demo 1：线程数量随连接线性增长的成本？
- [ ] Demo 2：`EAGAIN` 处理与 CPU 飙升的关系？
- [ ] Demo 3：`select` 等待是否显著降低 CPU？
- [ ] Demo 4：回调注册/分发是否解耦了业务与 I/O？
- [ ] Demo 5：能否同时处理 fd 事件和定时器事件？

---

## 延伸阅读 / 思考

- 为什么“非阻塞 + 忙轮询”依然是坏方案？（CPU 与功耗）
- 回调地狱 vs 状态机：如何用状态机或协程让代码更可读？
- 多线程 + 非阻塞：主线程只做事件分发，工作线程处理业务，这与纯回调相比的利弊？
