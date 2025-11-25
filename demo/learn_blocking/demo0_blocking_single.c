/**
 * Demo 0: 阻塞版（单连接）TCP Echo
 *
 * 体验点：只要一个客户端不发数据，主线程就被卡住，新连接进不来。
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8888
#define BUFFER_SIZE 1024

int main() {
    int listen_fd, conn_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    // socket: 创建一个socket文件描述符。AF_INET表示IPv4，SOCK_STREAM表示TCP协议。
    // 返回的文件描述符用于后续的网络操作。
    if (listen_fd < 0) {
        perror("socket");
        // perror: 打印最近一次系统调用的错误信息到stderr。
        return 1;
    }

    // IO多路复用知识点：默认情况下，socket是阻塞的，即操作会等待直到完成。
    // 这意味着如果没有数据或连接，程序会停在这里，无法处理其他任务。

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // setsockopt: 设置socket选项。这里设置SO_REUSEADDR，允许服务器重用本地地址和端口，
    // 避免TIME_WAIT状态导致的绑定失败，常用于开发和测试环境。

    memset(&server_addr, 0, sizeof(server_addr));
    // memset: 将结构体server_addr的内存清零，确保所有字段初始化为0，
    // 避免未初始化的垃圾值导致的意外行为。
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    // htons: 将主机字节序的端口号转换为网络字节序。网络通信使用大端字节序。

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        // bind: 将socket绑定到指定的IP地址和端口。INADDR_ANY表示监听所有本地IP地址。
        // 必须在listen之前调用。
        perror("bind");
        return 1;
    }

    if (listen(listen_fd, 5) < 0) {
        // listen: 将socket设置为监听模式，第二个参数是等待连接队列的最大长度。
        // 服务器开始监听客户端连接请求。
        perror("listen");
        return 1;
    }

    printf("Demo0: blocking echo on %d\n", PORT);
    printf("提示：保持第一个客户端不发数据，第二个客户端会卡在连接阶段。\n");

    // IO多路复用知识点：这个外层循环每次只能处理一个连接。
    // 当处理当前连接时，新连接无法被接受，这就是阻塞IO的串行处理问题。
    // IO多路复用允许同时监控多个文件描述符，提高并发处理能力。
    while (1) {
        conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        // accept: 接受客户端连接，返回新的socket文件描述符用于与客户端通信。
        // client_addr存储客户端地址信息。
        if (conn_fd < 0) {
            perror("accept");
            continue;
        }

        printf("Client %s:%d connected\n", inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));
        // inet_ntoa: 将网络字节序的IP地址转换为点分十进制字符串。
        // ntohs: 将网络字节序的端口号转换为主机字节序的整数。

        // IO多路复用知识点：read() 也是阻塞的，如果客户端不发送数据，程序会停在这里。
        // 此时，即使有新连接请求，也无法处理，这就是阻塞IO的局限性。
        // 解决方案是使用select()、poll() 或 epoll() 等IO多路复用技术，同时监听多个文件描述符。
        while (1) {
            ssize_t n = read(conn_fd, buffer, sizeof(buffer));
            // read: 从socket读取数据到buffer中。返回读取的字节数，0表示连接关闭，-1表示错误。
            if (n <= 0) {
                break;  // 客户端断开或错误
            }
            write(conn_fd, buffer, n);  // 回显
            // write: 将buffer中的数据写入socket发送给客户端。返回写入的字节数。
        }

        printf("Client disconnected\n");
        close(conn_fd);
        // close: 关闭socket文件描述符，释放资源。
    }

    close(listen_fd);
    // close: 关闭监听socket，结束服务器。
    return 0;
}
