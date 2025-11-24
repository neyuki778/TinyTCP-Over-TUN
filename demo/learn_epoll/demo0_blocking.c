/**
 * Demo 0: 阻塞版 TCP Echo Server
 * 
 * 学习目标：体验单线程阻塞 I/O 的问题
 * 痛点：一次只能服务一个客户端，其他客户端必须等待
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8888
#define BUFFER_SIZE 1024

int main() {
    int listen_fd, conn_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // TODO: 创建 socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    // TODO: 设置地址复用（避免 "Address already in use" 错误）
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // TODO: 绑定地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // TODO: 监听
    listen(listen_fd, 5);

    printf("Blocking Echo Server started on port %d\n", PORT);
    printf("⚠️  注意：这是阻塞版本，同时只能服务一个客户端！\n");
    printf("尝试用两个终端连接，观察第二个终端的阻塞现象。\n\n");

    while (1) {
        // TODO: 接受新连接
        conn_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
        
        printf("Client connected from %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        // TODO: 回显数据（这里会阻塞！）
        while (1) {
            ssize_t n = read(conn_fd, buffer, sizeof(buffer));
            if (n <= 0) break;  // 客户端断开
            write(conn_fd, buffer, n);  // 回显
        }

        printf("Client disconnected\n");
        close(conn_fd);
    }

    close(listen_fd);
    return 0;
}
