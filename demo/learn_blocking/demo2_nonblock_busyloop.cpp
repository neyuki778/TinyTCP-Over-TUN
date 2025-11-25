/**
 * Demo 2: 非阻塞 + 忙轮询
 *
 * 体验点：非阻塞并不等于高效，忙轮询会让 CPU 打满。
 */

#include "base_server.h"
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <utility>
#include <vector>

class NonBlockingBusyLoopServer : public BaseEchoServer {
public:
    NonBlockingBusyLoopServer() = default;

    void acceptAndHandle() override {
        if (setNonBlocking(listen_fd_) == -1) {
            throw std::runtime_error("setNonBlocking listen_fd failed");
        }
        std::signal(SIGPIPE, SIG_IGN);

        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        std::cout << "Demo2: non-blocking + busy-loop echo" << std::endl;
        std::cout << "提示：循环里没有 select/poll，观察 CPU 占用。" << std::endl;

        // Busy-loop: 尝试接受连接，同时轮询已存在连接
        while (true) {
            client_len = sizeof(client_addr);
            int conn_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (conn_fd >= 0) {
                if (setNonBlocking(conn_fd) == -1) {
                    std::cerr << "setNonBlocking failed\n";
                    close(conn_fd);
                } else {
                    client_socks_.push_back(Socket(conn_fd));
                }
            } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                std::cerr << "accept failed\n";
            }

            for (auto it = client_socks_.begin(); it != client_socks_.end();) {
                int fd = it->get();
                ssize_t n = read(fd, buffer_.data(), buffer_.size());
                if (n > 0) {
                    ssize_t sent = 0;
                    while (sent < n) {
                        ssize_t m = write(fd, buffer_.data() + sent, n - sent);
                        if (m > 0) {
                            sent += m;
                        } else if (m == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                            break;  // 发送缓冲区满，放弃剩余部分
                        } else {
                            std::cerr << "write error on fd " << fd << std::endl;
                            close(fd);
                            it = client_socks_.erase(it);
                            goto next_client;
                        }
                    }
                    ++it;
                } else if (n == 0) {
                    close(fd);
                    it = client_socks_.erase(it);
                } else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    ++it;
                } else {
                    std::cerr << "read error on fd " << fd << std::endl;
                    close(fd);
                    it = client_socks_.erase(it);
                }
            next_client:
                continue;
            }
        }
    }

private:
    static int setNonBlocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
            return -1;
        }
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            return -1;
        }
        return 0;
    }

    std::vector<Socket> client_socks_;
};

int main() {
    try {
        NonBlockingBusyLoopServer server;
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
