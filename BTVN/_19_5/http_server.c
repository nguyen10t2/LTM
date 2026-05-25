#include <netinet/in.h>
#include <pthread.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

const char *msg = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Hello World!!!</h1></body></html>\n";

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    struct pollfd fds[1];
    fds[0].fd = client_fd;
    fds[0].events = POLLIN;

    int ret = poll(fds, 1, 5000); // 5 seconds timeout
    if (ret > 0) {
        if (fds[0].revents & POLLIN) {
            char buffer[BUFFER_SIZE];
            ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                char *first_line = strtok(buffer, "\r\n");
                if (first_line) {
                    printf("Received request: %s\n", first_line);
                }
                send(client_fd, msg, strlen(msg), 0);
            }
        }
    } else if (ret == 0) {
        printf("Client %d timed out\n", client_fd);
    } else {
        perror("poll");
    }

    close(client_fd);
    return NULL;
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket");
        exit(1);
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
        exit(1);
    }

    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(1);
    }

    printf("HTTP Server (Threaded + Poll) listening on port 9000\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = accept(listener, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        int *p_client_fd = malloc(sizeof(int));
        if (p_client_fd) {
            *p_client_fd = client_fd;
            pthread_t thread;
            if (pthread_create(&thread, NULL, handle_client, p_client_fd) != 0) {
                perror("pthread_create");
                free(p_client_fd);
                close(client_fd);
            } else {
                pthread_detach(thread);
            }
        } else {
            close(client_fd);
        }
    }

    close(listener);
    return 0;
}
