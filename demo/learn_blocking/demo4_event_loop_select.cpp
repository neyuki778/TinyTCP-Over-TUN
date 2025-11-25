/**
 * Demo 4: 回调驱动的事件循环（基于 select）
 *
 * 体验点：事件分发 + 回调解耦业务逻辑，主循环只做分发。
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
    using Callback = std::function<void(int)>;

    SelectEventLoop() : max_fd_(-1), read_cbs_(FD_SETSIZE), write_cbs_(FD_SETSIZE) {
        FD_ZERO(&read_set_);
        FD_ZERO(&write_set_);
    }

    bool add_read(int fd, Callback cb) {
        if (!valid_fd(fd)) {
            return false;
        }
        FD_SET(fd, &read_set_);
        read_cbs_[fd] = std::move(cb);
        if (fd > max_fd_) {
            max_fd_ = fd;
        }
        return true;
    }

    bool add_write(int fd, Callback cb) {
        if (!valid_fd(fd)) {
            return false;
        }
        FD_SET(fd, &write_set_);
        write_cbs_[fd] = std::move(cb);
        if (fd > max_fd_) {
            max_fd_ = fd;
        }
        return true;
    }

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

    void run() {
        while (true) {
            fd_set rfds = read_set_;
            fd_set wfds = write_set_;

            int n = select(max_fd_ + 1, &rfds, &wfds, nullptr, nullptr);
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                perror("select");
                break;
            }

            for (int fd = 0; fd <= max_fd_ && n > 0; ++fd) {
                if (FD_ISSET(fd, &rfds)) {
                    --n;
                    if (read_cbs_[fd]) {
                        read_cbs_[fd](fd);
                    }
                }
                if (FD_ISSET(fd, &wfds)) {
                    --n;
                    if (write_cbs_[fd]) {
                        write_cbs_[fd](fd);
                    }
                }
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
        if (set_nonblock(listen_fd_) == -1) {
            throw std::runtime_error("set_nonblock listen_fd failed");
        }
        std::signal(SIGPIPE, SIG_IGN);

        auto on_client_read = [this](int fd) { handle_client_read(fd); };

        auto on_accept = [this, on_client_read](int fd) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            while (true) {
                client_len = sizeof(client_addr);
                int conn_fd = accept(fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
                if (conn_fd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    }
                    std::cerr << "accept failed\n";
                    break;
                }
                if (conn_fd >= FD_SETSIZE) {
                    std::cerr << "fd exceed FD_SETSIZE, close\n";
                    close(conn_fd);
                    continue;
                }
                if (set_nonblock(conn_fd) == -1) {
                    std::cerr << "set_nonblock failed\n";
                    close(conn_fd);
                    continue;
                }
                loop_.add_read(conn_fd, on_client_read);
            }
        };

        loop_.add_read(listen_fd_, on_accept);

        std::cout << "Demo4: callback-driven event loop (select)" << std::endl;
        std::cout << "提示：主循环仅分发事件，读/写逻辑在回调中完成。" << std::endl;

        loop_.run();
    }

private:
    void handle_client_read(int fd) {
        ssize_t n = read(fd, buffer_.data(), buffer_.size());
        if (n > 0) {
            ssize_t sent = 0;
            while (sent < n) {
                ssize_t m = write(fd, buffer_.data() + sent, n - sent);
                if (m > 0) {
                    sent += m;
                } else if (m == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    break;  // 缓冲区满，丢弃剩余数据以保持示例简单
                } else {
                    std::cerr << "write error on fd " << fd << std::endl;
                    cleanup_fd(fd);
                    return;
                }
            }
        } else if (n == 0 || !(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
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
