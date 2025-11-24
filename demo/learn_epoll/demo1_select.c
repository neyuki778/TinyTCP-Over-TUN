/**
 * Demo 1: 使用 select 实现多路复用
 * 
 * 学习目标：理解"位图"和"全量遍历"
 * 关键点：
 * 1. 每次调用 select 前必须重置 fd_set
 * 2. 需要维护 max_fd
 * 3. 返回后要遍历所有 fd 检查 FD_ISSET
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>

#define PORT 8888
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1024

int main() {
    int listen_fd;
    int client_fds[MAX_CLIENTS];  // 保存所有客户端 fd
    int max_fd;
    fd_set master_set, read_set;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // TODO: 创建监听 socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    // 设置地址复用
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // 绑定地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    // 开始监听
    listen(listen_fd, 5);

    // 初始化客户端数组
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_fds[i] = -1;
    }

    // TODO: 初始化 fd_set
    FD_ZERO(&master_set);
    FD_SET(listen_fd, &master_set);
    max_fd = listen_fd;

    printf("Select Echo Server started on port %d\n", PORT);
    printf("📊 体验要点：观察 max_fd 的维护和全量遍历\n\n");

    while (1) {
        // TODO: 每次循环都要重置 read_set（这是 select 的著名缺陷！）
        read_set = master_set;

        // TODO: 调用 select（无超时，永久等待）
        int nready = select(max_fd + 1, &read_set, NULL, NULL, NULL);
        
        if (nready < 0) {
            perror("select error");
            exit(1);
        }

        // TODO: 检查监听 socket 是否有新连接
        if (FD_ISSET(listen_fd, &read_set)) {
            int conn_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
            
            // 添加到 client_fds 数组
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_fds[i] == -1) {
                    client_fds[i] = conn_fd;
                    FD_SET(conn_fd, &master_set);
                    if (conn_fd > max_fd) max_fd = conn_fd;
                    printf("New client [fd=%d] connected\n", conn_fd);
                    break;
                }
            }
        }

        // TODO: 遍历所有客户端 fd（体验"全量遍历"的开销）
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = client_fds[i];
            if (fd == -1) continue;
        
            if (FD_ISSET(fd, &read_set)) {
                ssize_t n = read(fd, buffer, sizeof(buffer));
                if (n <= 0) {
                    // 客户端断开
                    printf("Client [fd=%d] disconnected\n", fd);
                    close(fd);
                    FD_CLR(fd, &master_set);
                    client_fds[i] = -1;
                } else {
                    write(fd, buffer, n);  // 回显
                }
            }
        }
    }

    close(listen_fd);
    return 0;
}
