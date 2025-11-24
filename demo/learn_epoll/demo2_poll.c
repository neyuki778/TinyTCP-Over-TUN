/**
 * Demo 2: 使用 poll 实现多路复用
 * 
 * 学习目标：理解"结构体数组"取代"位图"的好处
 * 优点：
 * 1. 不需要每次重置集合（events 和 revents 分离）
 * 2. 没有 1024 连接限制
 * 痛点：
 * 1. 依然需要全量遍历整个数组
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>

#define PORT 8888
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1024

int main() {
    int listen_fd;
    struct pollfd fds[MAX_CLIENTS];
    int nfds = 1;  // 当前监控的 fd 数量
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // TODO: 创建监听 socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    // 设置地址复用、绑定、监听
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);
    bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(listen_fd, 5);
    // TODO: 初始化 pollfd 数组
    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;  // 关心可读事件
    
    for (int i = 1; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;  // -1 表示空闲位置
    }

    printf("Poll Echo Server started on port %d\n", PORT);
    printf("📊 体验要点：不需要重置集合，但依然要全量遍历\n\n");

    while (1) {
        // TODO: 调用 poll（无超时，永久等待）
        int nready = poll(fds, nfds, -1);
        
        if (nready < 0) {
            perror("poll error");
            exit(1);
        }

        // TODO: 检查监听 socket是否有新连接
        if (fds[0].revents & POLLIN) {
            int conn_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
            
            // 添加到数组
            for (int i = 1; i < MAX_CLIENTS; i++) {
                if (fds[i].fd == -1) {
                    fds[i].fd = conn_fd;
                    fds[i].events = POLLIN;
                    if (i >= nfds) nfds = i + 1;
                    printf("New client [fd=%d] connected, total=%d\n", conn_fd, nfds - 1);
                    break;
                }
            }
        }

        // TODO: 遍历所有客户端（体验：哪怕只有 1 个活跃连接，也要遍历整个数组）
        for (int i = 1; i < nfds; i++) {
            if (fds[i].fd == -1) continue;
            
            if (fds[i].revents & POLLIN) {
                int fd = fds[i].fd;
                ssize_t n = read(fd, buffer, sizeof(buffer));
                if (n <= 0) {
                    printf("Client [fd=%d] disconnected\n", fd);
                    close(fd);
                    fds[i].fd = -1;
                } else {
                    write(fd, buffer, n);
                }
            }
        }
    }

    close(listen_fd);
    return 0;
}
