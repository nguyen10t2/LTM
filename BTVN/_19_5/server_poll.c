#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_ID_NAME 32
#define MAX_CLIENTS 512
#define MAX_BUFFER 1024
#define MAX_MSG 128
#define MAX_TIME_STR 64

typedef struct {
    char name[MAX_ID_NAME];
    int fd;
    int registered;
} client_t;

client_t clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void get_time_str(char *buffer, int size) {
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    strftime(buffer, size, "%Y/%m/%d %H:%M:%S", &t);
}

void broadcast(int sender_fd, const char *msg) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1 && clients[i].fd != sender_fd && clients[i].registered) {
            send(clients[i].fd, msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    int fd = *(int *)arg;
    free(arg);

    int my_index = -1;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == -1) {
            clients[i].fd = fd;
            clients[i].registered = 0;
            my_index = i;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (my_index == -1) {
        send(fd, "Server full\n", 12, 0);
        close(fd);
        return NULL;
    }

    char *welcome = "Nhap dung format: client_id: client_name\n";
    send(fd, welcome, strlen(welcome), 0);

    struct pollfd fds[1];
    fds[0].fd = fd;
    fds[0].events = POLLIN;

    char buffer[MAX_BUFFER];
    while (1) {
        int ret = poll(fds, 1, -1);
        if (ret <= 0) break;

        if (fds[0].revents & POLLIN) {
            int n = recv(fd, buffer, sizeof(buffer) - 1, 0);
            if (n <= 0) break;
            buffer[n] = '\0';
            buffer[strcspn(buffer, "\r\n")] = '\0';

            if (!clients[my_index].registered) {
                char id[MAX_ID_NAME], name[MAX_ID_NAME];
                if (sscanf(buffer, "client_id: %s", name) == 1) {
                    pthread_mutex_lock(&clients_mutex);
                    strncpy(clients[my_index].name, name, MAX_ID_NAME - 1);
                    clients[my_index].registered = 1;
                    pthread_mutex_unlock(&clients_mutex);

                    char ok_msg[MAX_MSG];
                    snprintf(ok_msg, sizeof(ok_msg), "Dang ky thanh cong: %s\n", name);
                    send(fd, ok_msg, strlen(ok_msg), 0);
                    printf("Client %d registered as %s\n", fd, name);
                } else {
                    send(fd, "Sai format. Dung: client_id: client_name\n", 41, 0);
                }
            } else {
                char time_str[MAX_TIME_STR];
                char broadcast_msg[MAX_BUFFER + MAX_MSG];
                get_time_str(time_str, sizeof(time_str));
                
                printf("%s %s %s\n", time_str, clients[my_index].name, buffer);
                snprintf(broadcast_msg, sizeof(broadcast_msg), "%s %s: %s\n", time_str, clients[my_index].name, buffer);
                broadcast(fd, broadcast_msg);
            }
        }
    }

    printf("Client %d disconnected\n", fd);
    pthread_mutex_lock(&clients_mutex);
    clients[my_index].fd = -1;
    clients[my_index].registered = 0;
    pthread_mutex_unlock(&clients_mutex);
    close(fd);
    return NULL;
}

int main() {
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].fd = -1;

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(9000),
    };

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(listener, 10) == -1) {
        perror("listen");
        return 1;
    }

    printf("Broadcast Server (Threaded + Poll) listening on port 9000\n");

    while (1) {
        int client_fd = accept(listener, NULL, NULL);
        if (client_fd >= 0) {
            int *p_fd = malloc(sizeof(int));
            if (p_fd) {
                *p_fd = client_fd;
                pthread_t thread;
                if (pthread_create(&thread, NULL, handle_client, p_fd) == 0) {
                    pthread_detach(thread);
                } else {
                    perror("pthread_create");
                    free(p_fd);
                    close(client_fd);
                }
            } else {
                close(client_fd);
            }
        }
    }

    close(listener);
    return 0;
}
