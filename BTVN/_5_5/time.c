#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
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

void sigchld_handler(int s) {
    (void)s;
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

struct tm *get_current_time() {
    time_t now = time(NULL);
    return localtime(&now);
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
    return !strncmp(str, "EXIT", 4) || !strncmp(str, "exit", 4);
}

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    while (1) {
        send(client_fd, "Enter command: ", 15, 0);
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
        struct tm *current_time = get_current_time();
        char time_str[BUFFER_SIZE];
        switch (index_format) {
            case 0:
                strftime(time_str, sizeof(time_str), "%d/%m/%Y\n", current_time);             break;
            case 1:
                strftime(time_str, sizeof(time_str), "%d/%m/%y\n", current_time);             break;
            case 2:
                strftime(time_str, sizeof(time_str), "%m/%d/%Y\n", current_time);             break;
            case 3:
                strftime(time_str, sizeof(time_str), "%m/%d/%y\n", current_time);             break;
        }
        send(client_fd, time_str, strlen(time_str), 0);
    }
    close(client_fd);
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

    printf("Server listening on port 9000\n");

    signal(SIGCHLD, sigchld_handler);

    while (1) {
        int client_fd = accept(listener, NULL, NULL);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(listener);
            handle_client(client_fd);
            exit(0);
        }
        else if (pid > 0) {
            // Parent process
            close(client_fd);
        } else {
            perror("fork");
            close(client_fd);
        }
    }

    close(listener);
    return 0;
}
