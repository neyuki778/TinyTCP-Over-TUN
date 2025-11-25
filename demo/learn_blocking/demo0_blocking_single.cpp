/**
 * Demo 0: 阻塞版（单连接）TCP Echo (C++风格重构)
 *
 * 体验点：只要一个客户端不发数据，主线程就被卡住，新连接进不来。
 */

#include <arpa/inet.h>
#include <array>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8888
#define BUFFER_SIZE 1024

class Socket {
public:
    explicit Socket(int fd) : fd_(fd) {}
    ~Socket() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }
    int get() const { return fd_; }
private:
    int fd_;
};

class BlockingEchoServer {
public:
    BlockingEchoServer() : listen_fd_(-1), buffer_{} {}
    ~BlockingEchoServer() {
        if (listen_fd_ >= 0) {
            close(listen_fd_);
        }
    }

    void run() {
        setupSocket();
        bindAndListen();
        acceptAndEcho();
    }

private:
    void setupSocket() {
        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            throw std::runtime_error("socket failed");
        }

        // IO多路复用知识点：默认情况下，socket是阻塞的。
        int opt = 1;
        if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            throw std::runtime_error("setsockopt failed");
        }
    }

    void bindAndListen() {
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(PORT);

        if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
            throw std::runtime_error("bind failed");
        }

        if (listen(listen_fd_, 5) < 0) {
            throw std::runtime_error("listen failed");
        }

        std::cout << "Demo0: blocking echo on " << PORT << std::endl;
        std::cout << "提示：保持第一个客户端不发数据，第二个客户端会卡在连接阶段。" << std::endl;
    }

    void acceptAndEcho() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        while (true) {
            int conn_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (conn_fd < 0) {
                std::cerr << "accept failed" << std::endl;
                continue;
            }

            // 用Socket类自动管理conn_fd，离开作用域时自动close
            Socket conn_socket(conn_fd);

            std::cout << "Client " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << " connected" << std::endl;

            // IO多路复用知识点：read() 是阻塞的。
            while (true) {
                ssize_t n = read(conn_socket.get(), buffer_.data(), buffer_.size());
                if (n <= 0) {
                    break;
                }
                write(conn_socket.get(), buffer_.data(), n);
            }

            std::cout << "Client disconnected" << std::endl;
            // 无需手动close，conn_socket析构时自动处理
        }
    }

    int listen_fd_;
    std::array<char, BUFFER_SIZE> buffer_;
};

int main() {
    try {
        BlockingEchoServer server;
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
