/**
 * Demo 1: 阻塞 + 线程 per connection (C++风格重构)
 *
 * 体验点：功能正常，但线程数随连接线性增长，切换和内存成本高。
 */

#include <arpa/inet.h>
#include <array>
#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
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

class ThreadPerConnectionServer {
public:
    ThreadPerConnectionServer() : listen_fd_(-1), buffer_{}, active_threads_(0) {}
    ~ThreadPerConnectionServer() {
        if (listen_fd_ >= 0) {
            close(listen_fd_);
        }
    }

    void run() {
        setupSocket();
        bindAndListen();
        acceptAndCreateThreads();
    }

private:
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

        std::cout << "Demo1: blocking + thread-per-connection echo on " << PORT << std::endl;
        std::cout << "提示：大量连接会快速增加线程数量，观察内存与上下文切换。" << std::endl;
    }

    void acceptAndCreateThreads() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        while (true) {
            int conn_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (conn_fd < 0) {
                std::cerr << "accept failed" << std::endl;
                continue;
            }

            // 用unique_ptr传递fd给线程
            auto arg_fd = std::make_unique<int>(conn_fd);

            std::thread t(&ThreadPerConnectionServer::handleClient, this, std::move(arg_fd));
            t.detach();  // 分离线程
        }
    }

    void handleClient(std::unique_ptr<int> arg_fd) {
        int fd = *arg_fd;

        // 用Socket类自动管理fd
        Socket conn_socket(fd);

        {
            std::lock_guard<std::mutex> lock(count_mutex_);
            active_threads_++;
            std::cout << "[thread " << std::this_thread::get_id() << "] start, active=" << active_threads_ << std::endl;
        }

        while (true) {
            ssize_t n = read(conn_socket.get(), buffer_.data(), buffer_.size());
            if (n <= 0) {
                break;
            }
            write(conn_socket.get(), buffer_.data(), n);
        }

        {
            std::lock_guard<std::mutex> lock(count_mutex_);
            active_threads_--;
            std::cout << "[thread " << std::this_thread::get_id() << "] exit, active=" << active_threads_ << std::endl;
        }
    }

    int listen_fd_;
    std::array<char, BUFFER_SIZE> buffer_;
    std::mutex count_mutex_;
    std::atomic<int> active_threads_;
};

int main() {
    try {
        ThreadPerConnectionServer server;
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
