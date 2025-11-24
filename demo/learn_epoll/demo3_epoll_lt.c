/**
 * Demo 3: 使用 epoll (LT 模式) 实现多路复用
 * 
 * 学习目标：理解"事件驱动"和"O(1) 效率"
 * 核心优势：
 * 1. epoll_wait 只返回活跃的 fd（不需要全量遍历）
 * 2. 内核维护就绪队列，性能与连接数无关
 * 3. LT 模式（电平触发）：只要有数据就会一直通知，较宽容
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>

#define PORT 8888
#define BUFFER_SIZE 1024
#define MAX_EVENTS 64

int main() {
    int listen_fd, epoll_fd;
    struct epoll_event ev, events[MAX_EVENTS];
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
    // TODO: 创建 epoll 句柄
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1 error");
        exit(1);
    }

    // TODO: 把监听 socket 加入 epoll
    ev.events = EPOLLIN;  // 关心可读事件
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    printf("Epoll (LT) Echo Server started on port %d\n", PORT);
    printf("🚀 体验要点：只遍历活跃连接，性能飞跃！\n\n");

    while (1) {
        // TODO: 等待事件（返回活跃的 fd 数量）
        int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        if (nready < 0) {
            perror("epoll_wait error");
            exit(1);
        }

        // TODO: 只遍历活跃的 fd（这里是核心！）
        for (int i = 0; i < nready; i++) {
            int fd = events[i].data.fd;
        
            if (fd == listen_fd) {
                // 新连接
                int conn_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
                printf("New client [fd=%d] connected\n", conn_fd);
                
                // 添加到 epoll
                ev.events = EPOLLIN;
                ev.data.fd = conn_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev);
            } else {
                // 客户端数据
                ssize_t n = read(fd, buffer, sizeof(buffer));
                if (n <= 0) {
                    printf("Client [fd=%d] disconnected\n", fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                } else {
                    write(fd, buffer, n);
                }
            }
        }
    }

    close(listen_fd);
    close(epoll_fd);
    return 0;
}
