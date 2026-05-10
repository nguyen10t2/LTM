#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>

#define MAX_USERNAME_LEN 128
#define MAX_PASSWORD_LEN 128
#define MAX_ALL_SECRET 128
#define BUFFER_SIZE 1024
#define MAX_CMD_LEN 128

typedef struct {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
} secret_t;

secret_t all_secrets[MAX_ALL_SECRET];
uint8_t number_secret = 0;

void sigchld_handler(int s) {
    (void)s;
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

uint8_t read_secret(FILE *fp, secret_t *secret) {
    int32_t ret = fscanf(fp, "%127s %127s", secret->username, secret->password);
    if (ret == EOF) return 2;
    if (ret != 2) {
        perror("invalid file format");
        return 1;
    }
    return 0;
}

uint8_t read_all_secrets() {
    FILE *fp = fopen("./secret.txt", "r");
    if (fp == NULL) {
        perror("file secret.txt not found");
        return 1;
    }
    while (number_secret < MAX_ALL_SECRET) {
        uint8_t ret = read_secret(fp, &all_secrets[number_secret]);
        if (ret == 2) break;
        if (ret != 0) {
            fclose(fp);
            return 1;
        }
        number_secret++;
    }
    fclose(fp);
    return 0;
}

uint8_t check_credentials(const char *username, const char *password) {
    for (uint8_t i = 0; i < number_secret; i++) {
        if (strcmp(username, all_secrets[i].username) == 0 && strcmp(password, all_secrets[i].password) == 0) {
            return 1;
        }
    }
    return 0;
}

bool is_exit(const char *str) {
    return !strncmp(str, "EXIT", 4) || !strncmp(str, "exit", 4);
}

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    int n;

    // Authentication phase
    while (1) {
        const char *msg_user = "Nhap username: ";
        send(client_fd, msg_user, strlen(msg_user), 0);
        n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) return;
        buffer[strcspn(buffer, "\r\n")] = '\0';
        strncpy(username, buffer, MAX_USERNAME_LEN - 1);
        username[MAX_USERNAME_LEN - 1] = '\0';

        const char *msg_pass = "Nhap password: ";
        send(client_fd, msg_pass, strlen(msg_pass), 0);
        n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) return;
        buffer[strcspn(buffer, "\r\n")] = '\0';
        strncpy(password, buffer, MAX_PASSWORD_LEN - 1);
        password[MAX_PASSWORD_LEN - 1] = '\0';

        if (check_credentials(username, password)) {
            const char *msg_ok = "Dang nhap thanh cong!\n";
            send(client_fd, msg_ok, strlen(msg_ok), 0);
            break;
        } else {
            const char *msg_fail = "Sai username hoac password!\n";
            send(client_fd, msg_fail, strlen(msg_fail), 0);
        }
    }

    // Command execution phase
    while (1) {
        const char *msg_cmd = "Nhap lenh: ";
        send(client_fd, msg_cmd, strlen(msg_cmd), 0);
        n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) break;
        buffer[strcspn(buffer, "\r\n")] = '\0';

        if (strlen(buffer) == 0) continue;
        if (is_exit(buffer)) {
            const char *msg_exit = "Thoat khoi he thong!\n";
            send(client_fd, msg_exit, strlen(msg_exit), 0);
            break;
        }

        char cmd[BUFFER_SIZE + 10];
        snprintf(cmd, sizeof(cmd), "%s 2>&1", buffer);
        FILE *fp = popen(cmd, "r");
        if (fp) {
            char line[BUFFER_SIZE];
            while (fgets(line, sizeof(line), fp)) {
                send(client_fd, line, strlen(line), 0);
            }
            pclose(fp);
        } else {
            const char *msg_err = "Loi thuc thi lenh!\n";
            send(client_fd, msg_err, strlen(msg_err), 0);
        }
    }
    close(client_fd);
}

int main(void) {
    if (read_all_secrets() != 0) {
        exit(1);
    }

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

    printf("Telnet server (fork) listening on port 9000\n");

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
        } else if (pid > 0) {
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
