/**
 * Demo 3: 非阻塞 + select
 *
 * 体验点：用就绪事件避免忙轮询，但仍需要维护 fd_set 与 max_fd。
 */

#include "base_server.hpp"
#include <cerrno>
#include <csignal>
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

int compute_max_fd(int listen_fd, const std::vector<Socket>& clients) {
    int max_fd = listen_fd;
    for (const auto& sock : clients) {
        if (sock.get() > max_fd) {
            max_fd = sock.get();
        }
    }
    return max_fd;
}

}  // namespace

class NonBlockingSelectServer : public BaseEchoServer {
public:
    NonBlockingSelectServer() = default;

    void acceptAndHandle() override {
        if (set_nonblock(listen_fd_) == -1) {
            throw std::runtime_error("set_nonblock listen_fd failed");
        }
        // Ignore SIGPIPE to prevent crashes on write to closed sockets
        std::signal(SIGPIPE, SIG_IGN);

        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        std::cout << "Demo3: non-blocking + select echo" << std::endl;
        std::cout << "提示：关注 fd_set 重置与 max_fd 维护。" << std::endl;

        while (true) {
            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(listen_fd_, &read_set);
            for (const auto& sock : clients_) {
                FD_SET(sock.get(), &read_set);
            }
            int max_fd = compute_max_fd(listen_fd_, clients_);

            int nready = select(max_fd + 1, &read_set, nullptr, nullptr, nullptr);
            if (nready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                perror("select");
                break;
            }

            if (FD_ISSET(listen_fd_, &read_set)) {
                while (true) {
                    client_len = sizeof(client_addr);
                    int conn_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
                    if (conn_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        std::cerr << "accept failed\n";
                        break;
                    }
                    if (set_nonblock(conn_fd) == -1) {
                        std::cerr << "set_nonblock failed\n";
                        close(conn_fd);
                        continue;
                    }
                    clients_.push_back(Socket(conn_fd));
                }
            }

            for (auto it = clients_.begin(); it != clients_.end();) {
                int fd = it->get();
                if (!FD_ISSET(fd, &read_set)) {
                    ++it;
                    continue;
                }

                ssize_t n = read(fd, buffer_.data(), buffer_.size());
                if (n > 0) {
                    ssize_t sent = 0;
                    while (sent < n) {
                        ssize_t m = write(fd, buffer_.data() + sent, n - sent);
                        if (m > 0) {
                            sent += m;
                        } else if (m == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                            break;  // 缓冲区满，剩余数据丢弃
                        } else {
                            std::cerr << "write error on fd " << fd << std::endl;
                            close(fd);
                            it = clients_.erase(it);
                            goto next_client;
                        }
                    }
                    ++it;
                } else if (n == 0) {
                    close(fd);
                    it = clients_.erase(it);
                } else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                    ++it;
                } else {
                    std::cerr << "read error on fd " << fd << std::endl;
                    close(fd);
                    it = clients_.erase(it);
                }
            next_client:
                continue;
            }
        }
    }

private:
    std::vector<Socket> clients_;
};

int main() {
    try {
        NonBlockingSelectServer server;
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
