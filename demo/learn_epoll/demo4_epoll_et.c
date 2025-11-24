/**
 * Demo 4: 使用 epoll (ET 模式) + 非阻塞 I/O
 * 
 * 学习目标：理解"边缘触发"的严格要求
 * 核心挑战：
 * 1. 必须把 socket 设置为非阻塞
 * 2. 收到通知后必须循环读取，直到 EAGAIN
 * 3. 如果不读完，epoll 不会再通知（"边缘"触发）
 * 
 * 坑点：先故意写一个"只读一次"的错误版本，体验客户端卡死
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>

#define PORT 8888
#define BUFFER_SIZE 1024
#define MAX_EVENTS 64

// 工具函数：设置非阻塞
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

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
    // TODO: 把监听 socket 设置为非阻塞
    set_nonblocking(listen_fd);

    // TODO: 创建 epoll 句柄
    epoll_fd = epoll_create1(0);

    // TODO: 把监听 socket 加入 epoll（注意：EPOLLET = 边缘触发）
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    printf("Epoll (ET) Echo Server started on port %d\n", PORT);
    printf("⚡ 体验要点：必须循环读取直到 EAGAIN，否则客户端卡死！\n");
    printf("建议：先故意只读一次（不用 while），体验 bug 再修复。\n\n");

    while (1) {
        int nready = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        for (int i = 0; i < nready; i++) {
            int fd = events[i].data.fd;
        
            if (fd == listen_fd) {
                // TODO: ET 模式下，accept 也要循环（可能一次来多个连接）
                while (1) {
                    int conn_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
                    if (conn_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;  // 没有更多连接了
                        }
                        perror("accept error");
                        continue;
                    }
                    
                    printf("New client [fd=%d] connected\n", conn_fd);
                    set_nonblocking(conn_fd);
                    
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = conn_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev);
                }
            } else {
                // TODO: 关键！必须循环读取直到 EAGAIN
                // 错误示范（先试试这个，看客户端会卡死）：
                // ssize_t n = read(fd, buffer, sizeof(buffer));
                // if (n > 0) write(fd, buffer, n);
        
                // 正确示范：
                while (1) {
                    ssize_t n = read(fd, buffer, sizeof(buffer));
                    if (n > 0) {
                        write(fd, buffer, n);
                    } else if (n == 0) {
                        printf("Client [fd=%d] disconnected\n", fd);
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                        close(fd);
                        break;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;  // 数据读完了，正常
                        }
                        perror("read error");
                        break;
                    }
                }
            }
        }
    }

    close(listen_fd);
    close(epoll_fd);
    return 0;
}
