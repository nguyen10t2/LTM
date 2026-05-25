#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int client_fds[MAX_CLIENTS + 1];
    uint8_t head;
    uint8_t tail;
} ClientQueue;

bool enqueue(ClientQueue *queue, int client_fd) {
    uint8_t next_tail = (queue->tail + 1) % (MAX_CLIENTS + 1);
    if (next_tail == queue->head) {
        return false;
    }
    queue->client_fds[queue->tail] = client_fd;
    queue->tail = next_tail;
    return true;
}

int dequeue(ClientQueue *queue) {
    if (queue->head == queue->tail) {
        return -1;
    }
    int client_fd = queue->client_fds[queue->head];
    queue->head = (queue->head + 1) % (MAX_CLIENTS + 1);
    return client_fd;
}

bool is_queue_empty(ClientQueue *queue) {
    return queue->head == queue->tail;
}

int count_queue(ClientQueue *queue) {
    if (queue->tail >= queue->head) {
        return queue->tail - queue->head;
    } else {
        return (MAX_CLIENTS + 1) - (queue->head - queue->tail);
    }
}

typedef struct {
    int c1;
    int c2;
} ChatPair;

bool is_exit(const char *msg) {
    return strncmp(msg, "exit", 4) == 0;
}

void *handle_chat(void *arg) {
    ChatPair *pair = (ChatPair *)arg;
    int c1 = pair->c1;
    int c2 = pair->c2;
    free(pair);

    char *msg = "Da tim thay ban chat. Bat dau tro chuyen!\n";
    send(c1, msg, strlen(msg), 0);
    send(c2, msg, strlen(msg), 0);

    struct pollfd fds[2];
    fds[0].fd = c1;
    fds[0].events = POLLIN;
    fds[1].fd = c2;
    fds[1].events = POLLIN;

    char buffer[BUFFER_SIZE];
    while (1) {
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            perror("poll");
            break;
        }

        if (fds[0].revents & POLLIN) {
            int n = recv(c1, buffer, sizeof(buffer), 0);
            if (n <= 0 || is_exit(buffer)) break;
            send(c2, buffer, n, 0);
        }

        if (fds[1].revents & POLLIN) {
            int n = recv(c2, buffer, sizeof(buffer), 0);
            if (n <= 0 || is_exit(buffer)) break;
            send(c1, buffer, n, 0);
        }

        if (fds[0].revents & (POLLERR | POLLHUP) || fds[1].revents & (POLLERR | POLLHUP)) {
            break;
        }
    }

    printf("Ket thuc cuoc tro chuyen giua %d va %d\n", c1, c2);
    close(c1);
    close(c2);
    return NULL;
}

int main() {
    ClientQueue queue = { .head = 0, .tail = 0 };

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(9000),
        .sin_addr = { .s_addr = INADDR_ANY },
    };

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(listener, 10) == -1) {
        perror("listen");
        return 1;
    }

    printf("Chat Server listening on port 9000\n");
    while (1) {
        int client_fd = accept(listener, NULL, NULL);
        if (client_fd >= 0) {
            pthread_mutex_lock(&queue_mutex);
            if (enqueue(&queue, client_fd)) {
                printf("Client connected: %d. Dang cho ban chat...\n", client_fd);
                send(client_fd, "Dang tim ban chat, vui long cho...\n", 35, 0);
            } else {
                printf("Queue full, rejecting client: %d\n", client_fd);
                send(client_fd, "Hang doi day, vui long quay lai sau.\n", 38, 0);
                close(client_fd);
            }

            while (count_queue(&queue) >= 2) {
                int c1 = dequeue(&queue);
                int c2 = dequeue(&queue);

                ChatPair *pair = malloc(sizeof(ChatPair));
                pair->c1 = c1;
                pair->c2 = c2;

                pthread_t thread;
                if (pthread_create(&thread, NULL, handle_chat, pair) != 0) {
                    perror("pthread_create");
                    close(c1);
                    close(c2);
                    free(pair);
                } else {
                    pthread_detach(thread);
                    printf("Da ghep doi: %d <-> %d\n", c1, c2);
                }
            }
            pthread_mutex_unlock(&queue_mutex);
        }
    }
    return 0;
}
