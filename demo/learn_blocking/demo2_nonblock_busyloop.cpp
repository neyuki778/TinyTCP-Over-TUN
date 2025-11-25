/**
 * Demo 2: 非阻塞 + 忙轮询
 *
 * 体验点：非阻塞并不等于高效，忙轮询会让 CPU 打满。
 */

#include "base_server.h"
#include <fcntl.h>
#include <iostream>
#include <vector>

class NonBlockingBusyLoopServer : public BaseEchoServer {
public:
    NonBlockingBusyLoopServer() = default;

    void acceptAndHandle() override {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        std::cout << "Demo2: non-blocking + busy-loop echo" << std::endl;
        std::cout << "提示：循环里没有 select/poll，观察 CPU 占用。" << std::endl;

        // Busy-loop accept and handle
        while (true) {
            int conn_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            Socket conn_socket(std::move(conn_fd));
            if (conn_socket.get() < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 没有新连接，继续循环
                    continue;
                }
                std::cerr << "accept failed" << std::endl;
                continue;
            }

            if (setNonBlocking(conn_socket.get()) == -1) {
                std::cerr << "setNonBlocking failed" << std::endl;
                close(conn_socket.get());
                continue;
            }

            client_fds_.emplace_back(conn_fd);

        }

        // Handle all client connections in a busy loop
        for (auto it : client_fds_) {
            while (true) {
                ssize_t n = read(it, buffer_.data(), sizeof(buffer_));
                if (n < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // 没有数据可读，继续循环
                        break;
                    }
                    std::cerr << "read error on fd " << it << std::endl;
                    close(it);
                    break;
                } else if (n == 0) {
                    // 客户端关闭连接
                    close(it);
                    break;
                } else {
                    // 回显数据
                    write(it, buffer_.data(), n);
                }
            }
        }
    }

private:
    static int setNonBlocking(int fd){
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
            return -1;
        }
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            return -1;
        }
        return 0;
    }

    std::vector<int> client_fds_;
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