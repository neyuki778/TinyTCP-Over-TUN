#pragma once

#include <arpa/inet.h>
#include <array>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8888
#define BUFFER_SIZE 1024
#define BACKLOG 16

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

class BaseEchoServer {
public:
    BaseEchoServer() : listen_fd_(-1), buffer_{} {}
    virtual ~BaseEchoServer() {
        if (listen_fd_ >= 0) {
            close(listen_fd_);
        }
    }

    void run() {
        setupSocket();
        bindAndListen();
        acceptAndHandle();
    }

protected:
    virtual void acceptAndHandle() = 0;  // 纯虚函数，由子类实现

    void setupSocket() {
        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            throw std::runtime_error("socket failed");
        }

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

        if (listen(listen_fd_, BACKLOG) < 0) {
            throw std::runtime_error("listen failed");
        }

        std::cout << "Echo server on " << PORT << std::endl;
    }

    void handleConnection(int conn_fd, const sockaddr_in& client_addr) {
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
    }

    int listen_fd_;
    std::array<char, BUFFER_SIZE> buffer_;
};