#include <netinet/in.h>
#include <pthread.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024
const char *CMD = "GET_TIME";
const int LEN_CMD = 8;
const char *INVALID_CMD = "INVALID_COMMAND\r\n";

const char FORMAT[4][11] = {
    "dd/mm/yyyy",
    "dd/mm/yy",
    "mm/dd/yyyy",
    "mm/dd/yy",
};

struct tm *get_current_time_safe() {
    time_t now = time(NULL);
    static __thread struct tm result;
    localtime_r(&now, &result);
    return &result;
}

int find_index_format(const char *format) {
    for (int i = 0; i < 4; i++) {
        if (strcmp(format, FORMAT[i]) == 0) {
            return i;
        }
    }
    return -1;
}

bool is_exit(const char *str) {
    return !strncasecmp(str, "EXIT", 4);
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    struct pollfd fds[1];
    fds[0].fd = client_fd;
    fds[0].events = POLLIN;

    while (1) {
        send(client_fd, "Enter command: ", 15, 0);
        
        int ret = poll(fds, 1, -1); // Wait indefinitely
        if (ret <= 0) break;

        if (fds[0].revents & POLLIN) {
            ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_read <= 0) break;
            buffer[strcspn(buffer, "\r\n")] = '\0';
            
            if (is_exit(buffer)) {
                send(client_fd, "Goodbye!\n", 9, 0);
                break;
            }
            if (strncmp(buffer, CMD, LEN_CMD) != 0) {
                send(client_fd, INVALID_CMD, strlen(INVALID_CMD), 0);
                continue;
            }
            char *format = buffer + LEN_CMD + 1;
            int index_format = find_index_format(format);
            if (index_format == -1) {
                send(client_fd, INVALID_CMD, strlen(INVALID_CMD), 0);
                continue;
            }
            struct tm *current_time = get_current_time_safe();
            char time_str[BUFFER_SIZE];
            switch (index_format) {
                case 0: strftime(time_str, sizeof(time_str), "%d/%m/%Y\n", current_time); break;
                case 1: strftime(time_str, sizeof(time_str), "%d/%m/%y\n", current_time); break;
                case 2: strftime(time_str, sizeof(time_str), "%m/%d/%Y\n", current_time); break;
                case 3: strftime(time_str, sizeof(time_str), "%m/%d/%y\n", current_time); break;
            }
            send(client_fd, time_str, strlen(time_str), 0);
        }
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

    printf("Time Server (Threaded + Poll) listening on port 9000\n");

    while (1) {
        int client_fd = accept(listener, NULL, NULL);
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
