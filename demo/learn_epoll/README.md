### 核心任务：编写一个单线程 TCP Echo Server

**功能描述：** 服务器监听端口（如 8888），支持多个客户端同时连接（使用 `nc` 或 `telnet` 测试）。客户端发什么，服务器就回什么。

---

## 学习路径

```
Demo 0: 阻塞版 (体验单线程阻塞 I/O 的痛苦)
  ↓
Demo 1: select (理解位图 + 全量遍历)
  ↓
Demo 2: poll (理解结构体数组 + 全量遍历)
  ↓  
Demo 3: epoll LT (理解事件驱动 + O(1) 效率)
  ↓
Demo 4: epoll ET (掌握边缘触发 + 非阻塞 I/O)
```

---

#### Demo 0: 基础 —— 阻塞版本（单客户端）

**学习目标：** 体验"单线程阻塞 I/O"的问题 —— 一次只能服务一个客户端。

**你需要做的事：**
1. 创建监听 socket，绑定 8888 端口。
2. 在 `while(1)` 循环里 `accept` 新连接。
3. `read` 数据，然后 `write` 回显。

> **体验痛点：** 当一个客户端连接后，`read` 会阻塞住，其他客户端的 `accept` 根本进不来。这就是为什么需要 I/O 多路复用。

---

#### Demo 1: 入门 —— 使用 `select` 实现

**学习目标：** 理解“位图（Bitmap）”和“轮询”的概念。 **你需要做的事：**

1.  创建一个 `fd_set` 集合（`rfds`）。
    
2.  把监听 socket (`listen_fd`) 放进去。
    
3.  在一个 `while(1)` 循环里调用 `select()`。
    
4.  **关键点：**
    
    -   每次调用 `select` 前，都需要**重置** `fd_set`（因为 select 会修改它，这是个著名的坑）。
        
    -   `select` 返回后，你需要写一个 `for` 循环，从 0 遍历到 `max_fd`，用 `FD_ISSET` 挨个检查是谁有数据。
        
    -   如果是 `listen_fd` 动了，说明有新连接 (`accept`)；如果是其他 fd 动了，说明有数据发来 (`read` & `write`)。
        

> **体验痛点：** 你会发现代码里必须维护一个 `max_fd` 变量，而且每次都要遍历所有 fd，哪怕只有一个 fd 活跃。

#### Demo 2: 过渡 —— 使用 `poll` 实现

**学习目标：** 理解“结构体数组”如何取代“位图”。 **你需要做的事：**

1.  把 Demo 1 里的 `fd_set` 换成一个 `struct pollfd` 数组（例如 `struct pollfd fds[1024]`）。
    
2.  把 `select()` 换成 `poll()`。
    
3.  **关键点：**
    
    -   不需要像 select 那样每次重置集合了（因为 `poll` 把“关心的事件”和“发生的事件”分开了）。
        
    -   但是！`poll` 返回后，你依然需要写一个 `for` 循环遍历整个 `fds` 数组，检查 `revents` 字段。
        

> **体验痛点：** 虽然没有了 1024 个连接的限制，但你发现“遍历整个数组”这个动作依然没变。

#### Demo 3: 现代 —— 使用 `epoll` (LT模式) 实现

**学习目标：** 理解“事件驱动”和“O(1) 效率”。 **你需要做的事：**

1.  使用 `epoll_create` 创建一个句柄。
    
2.  使用 `epoll_ctl` 把 `listen_fd` 添加进去 (`EPOLL_CTL_ADD`)。
    
3.  在 `while` 循环里调用 `epoll_wait`。
    
4.  **关键点（最爽的地方）：**
    
    -   `epoll_wait` 会返回一个整数 `n`，表示有 `n` 个事件发生了。
        
    -   你只需要遍历 `events` 数组的**前 n 个元素**。这里面每一个全是“干货”（活跃的连接），不需要像 select/poll 那样遍历无效的空连接。
        

___

### 进阶任务 (可选，但推荐)

#### Demo 4: 质变 —— `epoll` (ET模式) + 非阻塞 I/O

**场景：** 模拟数据包截断（比如缓冲区只有 1 字节，强迫你多次读取）。 **你需要做的事：**

1.  在 Demo 3 的基础上，把事件注册改为 `EPOLLET` (Edge Triggered)。
    
2.  把 socket 设置为非阻塞 (`O_NONBLOCK`)。
    
3.  **关键点：**
    
    -   当 `epoll_wait` 通知你可读时，你必须用一个 `while` 循环一直 `read`，直到 `read` 返回 -1 并且 `errno == EAGAIN`。
        
    -   如果不读完，下次 `epoll_wait` 就会“装死”，不再通知你，导致客户端卡死。
        

---

### 如何测试你的 Demo？

#### 方法 1: 手动测试
1. **启动服务器：** `./demo0_blocking` (或 demo1_select, demo2_poll 等)
2. **开启多个终端，模拟多个用户：**
   ```bash
   # 终端 1
   nc 127.0.0.1 8888
   
   # 终端 2
   nc 127.0.0.1 8888
   ```
3. 在 nc 中输入文字，观察服务器回显行为。

#### 方法 2: 自动化测试脚本
```bash
#!/bin/bash
# test_concurrent.sh
for i in {1..10}; do
    echo "Client $i: Hello" | nc 127.0.0.1 8888 &
done
wait
```

#### 方法 3: 性能压测（对比不同实现）
```bash
# 安装 apache bench
# macOS: brew install httpd (包含 ab 工具，但需要自己写简单的 HTTP 响应)
# 或者用 wrk/vegeta 等工具

# 简单并发测试
seq 1 100 | xargs -P 100 -I {} sh -c 'echo "test" | nc 127.0.0.1 8888'
```

---

### 编译和运行

```bash
# 编译所有 Demo
make

# 或者单独编译
gcc -o demo0_blocking demo0_blocking.c
gcc -o demo1_select demo1_select.c
gcc -o demo2_poll demo2_poll.c
gcc -o demo3_epoll_lt demo3_epoll_lt.c
gcc -o demo4_epoll_et demo4_epoll_et.c

# 运行
./demo1_select
```

---

### 学习检查清单

- [ ] Demo 0: 能否体会到阻塞 I/O 的"卡顿"？
- [ ] Demo 1: 是否理解 `FD_SET`/`FD_ISSET` 的位图操作？
- [ ] Demo 1: 是否记得每次 `select` 前要重置 `fd_set`？
- [ ] Demo 2: 是否感受到不需要维护 `max_fd` 的便利？
- [ ] Demo 3: 是否惊叹于 `epoll_wait` 只返回活跃连接的高效？
- [ ] Demo 4: 是否踩过"ET 模式不循环读导致卡死"的坑？

---

### 核心知识点对比

| 特性 | select | poll | epoll |
|------|--------|------|-------|
| 数据结构 | 位图 (fd_set) | 数组 (pollfd) | 红黑树 + 就绪链表 |
| 最大连接数 | 1024 (FD_SETSIZE) | 无限制 | 无限制 |
| 时间复杂度 | O(n) | O(n) | O(1) |
| 内核拷贝 | 每次都拷贝 | 每次都拷贝 | 只在 add/del 时 |
| 触发模式 | LT | LT | LT/ET |
| 跨平台 | ✅ | ✅ | ❌ (Linux only) |