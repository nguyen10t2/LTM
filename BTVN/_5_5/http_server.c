#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

const char *msg = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Hello World!!!</h1></body></html>\n";

void sigchld_handler(int s) {
    (void)s;
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0) {
        buffer[strcspn(buffer, "\r\n")] = '\0';
        printf("Received request:\n%s\n", buffer);
        send(client_fd, msg, strlen(msg), 0);
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
