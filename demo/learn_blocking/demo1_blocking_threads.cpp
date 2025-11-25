/**
 * Demo 1: 阻塞 + 线程 per connection (C++风格重构 + 继承)
 *
 * 体验点：功能正常，但线程数随连接线性增长，切换和内存成本高。
 */

#include "base_server.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#define BACKLOG 16

class ThreadPerConnectionServer : public BaseEchoServer {
public:
    ThreadPerConnectionServer() : active_threads_(0) {}

    void acceptAndHandle() override {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        std::cout << "Demo1: blocking + thread-per-connection echo" << std::endl;
        std::cout << "提示：大量连接会快速增加线程数量，观察内存与上下文切换。" << std::endl;

        while (true) {
            int conn_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (conn_fd < 0) {
                std::cerr << "accept failed" << std::endl;
                continue;
            }

            // 用unique_ptr传递fd给线程
            auto arg_fd = std::make_unique<int>(conn_fd);

            std::thread t(&ThreadPerConnectionServer::handleClient, this, std::move(arg_fd), client_addr);
            t.detach();  // 分离线程
        }
    }

private:
    void handleClient(std::unique_ptr<int> arg_fd, sockaddr_in client_addr) {
        int fd = *arg_fd;

        // 用Socket类自动管理fd
        Socket conn_socket(fd);

        {
            std::lock_guard<std::mutex> lock(count_mutex_);
            active_threads_++;
            std::cout << "[thread " << std::this_thread::get_id() << "] start, active=" << active_threads_ << std::endl;
        }

        handleConnection(fd, client_addr);

        {
            std::lock_guard<std::mutex> lock(count_mutex_);
            active_threads_--;
            std::cout << "[thread " << std::this_thread::get_id() << "] exit, active=" << active_threads_ << std::endl;
        }
    }

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
