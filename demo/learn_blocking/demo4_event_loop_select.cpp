/**
 * Demo 4: 回调驱动的事件循环（基于 select）
 *
 * 体验点：事件分发 + 回调解耦业务逻辑，主循环只做分发。
 *
 * 事件循环解释：
 * - 事件循环是一种编程模式，用于处理异步事件（如 I/O 操作）。
 * - 在这里，使用 select 系统调用来监听多个文件描述符（fd）的读/写事件。
 * - 当 select 返回时，表示某些 fd 准备好进行 I/O 操作。
 * - 主循环（run()）只负责等待事件和分发，不处理具体业务逻辑。
 *
 * 回调函数解释：
 * - 回调函数是当某个事件发生时被调用的函数。
 * - 在这个例子中，回调函数是 lambda 表达式，捕获 this 指针以访问成员变量。
 * - 注册回调：使用 add_read/add_write 将回调与 fd 关联。
 * - 调用回调：当事件发生时，事件循环调用相应的回调函数，传入 fd 参数。
 * - 这样，业务逻辑（如 accept 或读数据）被封装在回调中，主循环保持简洁。
 *
 * 关键概念：
 * - 非阻塞 I/O：socket 设置为非阻塞，避免操作阻塞整个程序。
 * - fd_set：位图结构，用于告诉 select 监听哪些 fd。
 * - 事件分发：select 返回后，检查哪些 fd 有事件，然后调用回调。
 */

#include "base_server.hpp"
#include <cerrno>
#include <csignal>
#include <functional>
#include <fcntl.h>
#include <iostream>
#include <vector>

namespace {

int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

class SelectEventLoop {
public:
    using Callback = std::function<void(int)>;  // 回调函数类型：接受一个 int 参数（文件描述符），返回 void

    // 构造函数：初始化事件循环
    // max_fd_: 当前监听的最大文件描述符，用于 select 的第一个参数
    // read_cbs_ 和 write_cbs_: 存储每个 fd 对应的读/写回调函数
    // read_set_ 和 write_set_: fd_set 用于 select，标记需要监听的 fd
    SelectEventLoop() : max_fd_(-1), read_cbs_(FD_SETSIZE), write_cbs_(FD_SETSIZE) {
        FD_ZERO(&read_set_);  // 清空读事件集合
        FD_ZERO(&write_set_);  // 清空写事件集合
    }

    // 注册读事件回调：当 fd 可读时，调用 cb(fd)
    // 这将 fd 添加到 read_set_ 中，并保存回调函数
    bool add_read(int fd, Callback cb) {
        if (!valid_fd(fd)) {
            return false;
        }
        FD_SET(fd, &read_set_);  // 将 fd 添加到读事件监听集合
        read_cbs_[fd] = std::move(cb);  // 保存回调函数（使用 move 避免拷贝）
        if (fd > max_fd_) {
            max_fd_ = fd;  // 更新最大 fd，用于 select 的范围
        }
        return true;
    }

    // 注册写事件回调：当 fd 可写时，调用 cb(fd)
    // 类似 add_read，但用于写事件
    // 这里的写事件注册在当前 demo 中未使用
    // bool add_write(int fd, Callback cb) {
    //     if (!valid_fd(fd)) {
    //         return false;
    //     }
    //     FD_SET(fd, &write_set_);  // 将 fd 添加到写事件监听集合
    //     write_cbs_[fd] = std::move(cb);  // 保存回调函数
    //     if (fd > max_fd_) {
    //         max_fd_ = fd;  // 更新最大 fd
    //     }
    //     return true;
    // }

    void remove_fd(int fd) {
        if (!valid_fd(fd)) {
            return;
        }
        FD_CLR(fd, &read_set_);
        FD_CLR(fd, &write_set_);
        read_cbs_[fd] = nullptr;
        write_cbs_[fd] = nullptr;
        if (fd == max_fd_) {
            for (int i = max_fd_ - 1; i >= 0; --i) {
                if (FD_ISSET(i, &read_set_) || FD_ISSET(i, &write_set_)) {
                    max_fd_ = i;
                    return;
                }
            }
            max_fd_ = -1;
        }
    }

    // 运行事件循环：这是事件驱动的核心
    // select 会阻塞等待，直到有 fd 准备好读或写
    // 当有事件发生时，检查哪些 fd 活跃，并调用对应的回调函数
    void run() {
        while (true) {
            fd_set rfds = read_set_;  // 复制读事件集合（select 会修改它）
            fd_set wfds = write_set_;  // 复制写事件集合

            // select 调用：阻塞等待事件
            // 参数：max_fd_+1 是 fd 范围，&rfds 和 &wfds 是监听的集合，nullptr 表示不监听异常和超时
            // 返回值 n 是活跃 fd 的数量
            int n = select(max_fd_ + 1, &rfds, &wfds, nullptr, nullptr);
            if (n < 0) {
                if (errno == EINTR) {  // 被信号中断，继续循环
                    continue;
                }
                perror("select");
                break;
            }

            // 遍历所有可能的 fd，检查哪些有事件
            for (int fd = 0; fd <= max_fd_ && n > 0; ++fd) {
                if (FD_ISSET(fd, &rfds)) {  // 如果 fd 在活跃读集合中
                    --n;  // 减少剩余事件计数
                    if (read_cbs_[fd]) {  // 如果有注册的读回调
                        read_cbs_[fd](fd);  // 调用回调函数，传入 fd
                    }
                }
                // 这里是写事件的处理示例，当前 demo 没有用到写回调
                // if (FD_ISSET(fd, &wfds)) {  // 如果 fd 在活跃写集合中
                //     --n;
                //     if (write_cbs_[fd]) {  // 如果有注册的写回调
                //         write_cbs_[fd](fd);  // 调用回调函数
                //     }
                // }
            }
        }
    }

private:
    bool valid_fd(int fd) const { return fd >= 0 && fd < FD_SETSIZE; }

    fd_set read_set_;
    fd_set write_set_;
    int max_fd_;
    std::vector<Callback> read_cbs_;
    std::vector<Callback> write_cbs_;
};

}  // namespace

class CallbackSelectServer : public BaseEchoServer {
public:
    CallbackSelectServer() = default;

    void acceptAndHandle() override {
        // 设置监听 socket 为非阻塞模式，避免 accept 阻塞
        if (set_nonblock(listen_fd_) == -1) {
            throw std::runtime_error("set_nonblock listen_fd failed");
        }
        std::signal(SIGPIPE, SIG_IGN);  // 忽略 SIGPIPE 信号（写已关闭的 socket 时产生）

        // 定义客户端读事件的回调函数：当客户端 socket 可读时，调用 handle_client_read
        auto on_client_read = [this](int fd) { handle_client_read(fd); };

        // 定义 accept 事件的回调函数：当监听 socket 可读时（有新连接），处理 accept
        auto on_accept = [this, on_client_read](int fd) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            while (true) {
                client_len = sizeof(client_addr);
                // accept 新连接（非阻塞模式）
                int conn_fd = accept(fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
                if (conn_fd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {  // 没有更多连接，退出循环
                        break;
                    }
                    std::cerr << "accept failed\n";
                    break;
                }
                if (conn_fd >= FD_SETSIZE) {  // fd 超过 select 限制，关闭
                    std::cerr << "fd exceed FD_SETSIZE, close\n";
                    close(conn_fd);
                    continue;
                }
                // 设置新连接为非阻塞，并注册读事件回调
                if (set_nonblock(conn_fd) == -1) {
                    std::cerr << "set_nonblock failed\n";
                    close(conn_fd);
                    continue;
                }
                // 为新连接注册读事件：当可读时，调用 on_client_read（即 handle_client_read）
                loop_.add_read(conn_fd, on_client_read);
            }
        };

        // 为监听 socket 注册读事件：当有新连接时，调用 on_accept
        loop_.add_read(listen_fd_, on_accept);

        std::cout << "Demo4: callback-driven event loop (select)" << std::endl;
        std::cout << "提示：主循环仅分发事件，读/写逻辑在回调中完成。" << std::endl;

        // 启动事件循环：select 阻塞等待事件，事件发生时调用回调
        loop_.run();
    }

private:
    // 处理客户端读事件的回调函数：当客户端 socket 可读时被调用
    // 读数据并回写，实现 echo 功能
    void handle_client_read(int fd) {
        // 从 fd 读数据到 buffer_
        ssize_t n = read(fd, buffer_.data(), buffer_.size());
        if (n > 0) {  // 读到数据
            ssize_t sent = 0;
            // 尝试写回所有数据
            while (sent < n) {
                ssize_t m = write(fd, buffer_.data() + sent, n - sent);
                if (m > 0) {
                    sent += m;  // 已发送字节数
                } else if (m == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    break;  // 缓冲区满，丢弃剩余数据以保持示例简单
                } else {
                    std::cerr << "write error on fd " << fd << std::endl;
                    cleanup_fd(fd);  // 出错时清理 fd
                    return;
                }
            }
        } else if (n == 0 || !(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
            // n == 0 表示连接关闭，其他错误也清理
            cleanup_fd(fd);
        }
    }

    void cleanup_fd(int fd) {
        loop_.remove_fd(fd);
        close(fd);
    }

    SelectEventLoop loop_;
};

int main() {
    try {
        CallbackSelectServer server;
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
