/**
 * Demo 1: 阻塞 + 线程 per connection
 *
 * 体验点：功能正常，但线程数随连接线性增长，切换和内存成本高。
 */

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8888
#define BUFFER_SIZE 1024

static pthread_mutex_t count_lock = PTHREAD_MUTEX_INITIALIZER;
// PTHREAD_MUTEX_INITIALIZER: 初始化互斥锁，用于保护共享变量active_threads。
static int active_threads = 0;

static void *handle_client(void *arg) {
    int fd = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];

    pthread_mutex_lock(&count_lock);
    // pthread_mutex_lock: 加锁互斥锁，确保只有一个线程能访问临界区。
    active_threads++;
    printf("[thread %lu] start, active=%d\n", (unsigned long)pthread_self(), active_threads);
    // pthread_self: 获取当前线程的ID，用于日志输出。
    pthread_mutex_unlock(&count_lock);
    // pthread_mutex_unlock: 解锁互斥锁，允许其他线程进入临界区。

    while (1) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n <= 0) {
            break;
        }
        write(fd, buffer, n);
    }

    close(fd);

    pthread_mutex_lock(&count_lock);
    // pthread_mutex_lock: 加锁互斥锁。
    active_threads--;
    printf("[thread %lu] exit, active=%d\n", (unsigned long)pthread_self(), active_threads);
    pthread_mutex_unlock(&count_lock);
    // pthread_mutex_unlock: 解锁互斥锁。

    return NULL;
}

int main() {
    int listen_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(listen_fd, 16) < 0) {
        perror("listen");
        return 1;
    }

    printf("Demo1: blocking + thread-per-connection echo on %d\n", PORT);
    printf("提示：大量连接会快速增加线程数量，观察内存与上下文切换。\n");

    while (1) {
        int conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (conn_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        int *arg_fd = malloc(sizeof(int));
        if (!arg_fd) {
            perror("malloc");
            close(conn_fd);
            continue;
        }
        *arg_fd = conn_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, arg_fd) != 0) {
            // pthread_create: 创建新线程，执行handle_client函数，传递arg_fd作为参数。
            // tid存储线程ID。
            perror("pthread_create");
            close(conn_fd);
            free(arg_fd);
            continue;
        }
        pthread_detach(tid);  // 不关心返回值，直接分离
        // pthread_detach: 分离线程，线程结束时自动释放资源，无需join。
    }

    close(listen_fd);
    return 0;
}
