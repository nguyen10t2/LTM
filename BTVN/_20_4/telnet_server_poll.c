#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>

#define MAX_USERNAME_LEN 128
#define MAX_PASSWORD_LEN 128
#define MAX_ALL_SECRET 128
#define MAX_CLIENTS 128
#define BUFFER_SIZE 1024
#define MAX_CMD_LEN 128

typedef struct {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
} secret_t;

typedef enum {
    STATE_USERNAME,
    STATE_PASSWORD,
    STATE_AUTHENTICATED,
} state_t;

typedef struct {
    char buffer[BUFFER_SIZE];
    char input_username[MAX_USERNAME_LEN];
    char input_password[MAX_PASSWORD_LEN];
    int32_t fd;
    int32_t buf_len;
    state_t state;
} client_t;

secret_t all_secrets[MAX_ALL_SECRET];
uint8_t number_secret = 0;

// +1 để index 0 dành cho listener, 1..MAX_CLIENTS dành cho client
client_t clients[MAX_CLIENTS + 1];
uint8_t num_clients = 0;
struct pollfd poll_fds[MAX_CLIENTS + 1];

void init_secrets(secret_t *secrets, uint8_t *num_secrets) {
    memset(secrets, 0, sizeof(secret_t) * MAX_ALL_SECRET);
    *num_secrets = 0;
}

void init_clients(client_t *cls, uint8_t *num_clients) {
    memset(cls, 0, sizeof(client_t) * (MAX_CLIENTS + 1));
    for (int i = 0; i <= MAX_CLIENTS; i++) {
        cls[i].fd = -1; // -1 đánh dấu slot trống
    }
    *num_clients = 0;
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

uint8_t check_credentials(const char *username, const char *password, secret_t *secrets, uint8_t num_secrets) {
    for (uint8_t i = 0; i < num_secrets; i++) {
        if (strcmp(username, secrets[i].username) == 0 && strcmp(password, secrets[i].password) == 0) {
            return 1;
        }
    }
    return 0;
}

uint8_t read_all_secrets(FILE *fp) {
    do {
        uint8_t ret = read_secret(fp, &all_secrets[number_secret]);
        if (ret == 2) break;
        if (ret != 0) return 1;
        number_secret++;
        if (number_secret >= MAX_ALL_SECRET) {
            perror("too many secrets");
            return 1;
        }
    } while (1);
    return 0;
}

int32_t find_free_slot(void) {
    for (int32_t i = 1; i <= MAX_CLIENTS; i++) {
        if (clients[i].fd == -1) return i;
    }
    return -1;
}

void reset_client(client_t *client, int32_t client_fd) {
    memset(client, 0, sizeof(client_t));
    client->fd = client_fd;
}

void add_client(uint8_t *num_clients, int32_t client_fd) {
    int32_t idx = find_free_slot();
    if (idx == -1) return;

    reset_client(&clients[idx], client_fd);
    poll_fds[idx].fd = client_fd;
    poll_fds[idx].events = POLLIN;
    poll_fds[idx].revents = 0;
    (*num_clients)++;

    const char *msg = "Nhap username: ";
    send(client_fd, msg, strlen(msg), 0);
}

void remove_client(int32_t idx, uint8_t *num_clients) {
    close(clients[idx].fd);
    poll_fds[idx].fd = -1;
    poll_fds[idx].events = 0;
    poll_fds[idx].revents = 0;
    reset_client(&clients[idx], -1);
    (*num_clients)--;
}

void handle_client_command(client_t *client, const char *cmd) {
    char fullcmd[MAX_CMD_LEN];
    snprintf(fullcmd, MAX_CMD_LEN, "%s > ./out.txt", cmd);
    system(fullcmd);
    FILE *fp = fopen("./out.txt", "r");
    if (fp) {
        char line[BUFFER_SIZE];
        while (fgets(line, sizeof(line), fp)) {
            send(client->fd, line, strlen(line), 0);
        }
        fclose(fp);
    }
    const char *msg = "Nhap lenh: ";
    send(client->fd, msg, strlen(msg), 0);
}

void handle_client_data(int32_t idx, uint8_t *num_clients) {
    char buffer[BUFFER_SIZE];
    int32_t n = recv(clients[idx].fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        printf("client %d disconnected\n", idx);
        remove_client(idx, num_clients);
        return;
    }
    buffer[strcspn(buffer, "\r\n")] = '\0';
    buffer[n] = '\0';

    switch (clients[idx].state) {
        case STATE_USERNAME: {
            strncpy(clients[idx].input_username, buffer, MAX_USERNAME_LEN - 1);
            clients[idx].input_username[MAX_USERNAME_LEN - 1] = '\0';
            const char *msg_pass = "Nhap password: ";
            send(clients[idx].fd, msg_pass, strlen(msg_pass), 0);
            clients[idx].state = STATE_PASSWORD;
            break;
        }
        case STATE_PASSWORD: {
            strncpy(clients[idx].input_password, buffer, MAX_PASSWORD_LEN - 1);
            clients[idx].input_password[MAX_PASSWORD_LEN - 1] = '\0';
            if (check_credentials(clients[idx].input_username, clients[idx].input_password, all_secrets, number_secret)) {
                const char *msg = "Dang nhap thanh cong!\nNhap lenh: ";
                send(clients[idx].fd, msg, strlen(msg), 0);
                clients[idx].state = STATE_AUTHENTICATED;
            } else {
                const char *msg = "Sai username hoac password!\nNhap username: ";
                send(clients[idx].fd, msg, strlen(msg), 0);
                clients[idx].state = STATE_USERNAME;
            }
            break;
        }
        case STATE_AUTHENTICATED: {
            char cmd[MAX_CMD_LEN];
            strncpy(cmd, buffer, MAX_CMD_LEN - 1);
            cmd[MAX_CMD_LEN - 1] = '\0';
            handle_client_command(&clients[idx], cmd);
            break;
        }
    }
}

int32_t main(void) {
    init_secrets(all_secrets, &number_secret);
    init_clients(clients, &num_clients);

    FILE *fp = fopen("./secret.txt", "r");
    if (fp == NULL) {
        perror("file not found");
        exit(1);
    }
    if (read_all_secrets(fp) != 0) {
        fclose(fp);
        exit(1);
    }
    fclose(fp);

    int32_t listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket");
        exit(1);
    }
    int32_t opt = 1;
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
    if (listen(listener, MAX_CLIENTS >> 1) == -1) {
        perror("listen");
        exit(1);
    }
    printf("Telnet server listening on port 9000\n");

    // Khởi tạo poll_fds
    for (int i = 0; i <= MAX_CLIENTS; i++) {
        poll_fds[i].fd = -1;
        poll_fds[i].events = 0;
        poll_fds[i].revents = 0;
    }
    poll_fds[0].fd = listener;
    poll_fds[0].events = POLLIN;

    while (1) {
        // poll() trả về số lượng fd có sự kiện, timeout tính bằng ms
        int32_t ret = poll(poll_fds, MAX_CLIENTS + 1, 5000);
        if (ret == -1) {
            perror("poll");
            return 1;
        }
        if (ret == 0) continue; // Timeout

        for (int32_t i = 0; i <= MAX_CLIENTS; i++) {
            if (poll_fds[i].fd == -1) continue;

            // Xử lý lỗi hoặc ngắt kết nối
            if (poll_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                if (i == 0) continue;
                printf("client %d disconnected (error/hangup)\n", i);
                remove_client(i, &num_clients);
                continue;
            }

            // Xử lý dữ liệu đến
            if (poll_fds[i].revents & POLLIN) {
                if (i == 0) {
                    int32_t client_fd = accept(listener, NULL, NULL);
                    if (client_fd == -1) {
                        perror("accept");
                        continue;
                    }
                    add_client(&num_clients, client_fd);
                } else {
                    handle_client_data(i, &num_clients);
                }
            }
        }
    }

    close(listener);
    return 0;
}
