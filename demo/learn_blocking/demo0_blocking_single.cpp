/**
 * Demo 0: 阻塞版（单连接）TCP Echo (C++风格重构 + 继承)
 *
 * 体验点：只要一个客户端不发数据，主线程就被卡住，新连接进不来。
 */

#include "base_server.h"
#include <iostream>
#include <string>

class BlockingEchoServer : public BaseEchoServer {
public:
    void acceptAndHandle() override {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        std::cout << "Demo0: blocking echo" << std::endl;
        std::cout << "提示：保持第一个客户端不发数据，第二个客户端会卡在连接阶段。" << std::endl;

        while (true) {
            int conn_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (conn_fd < 0) {
                std::cerr << "accept failed" << std::endl;
                continue;
            }

            handleConnection(conn_fd, client_addr);
        }
    }
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
