#include <bits/types/struct_timeval.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
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

client_t clients[MAX_CLIENTS];
uint8_t num_clients = 0;

void init_secrets(secret_t *secrets, uint8_t *num_secrets) {
    memset(secrets, 0, sizeof(secret_t) * MAX_ALL_SECRET);
    *num_secrets = 0;
}

void init_clients(client_t *clients, uint8_t *num_clients) {
    memset(clients, 0, sizeof(client_t) * MAX_CLIENTS);
    *num_clients = 0;
}

uint8_t read_secret(FILE *fp, secret_t *secret) {
    int32_t ret = fscanf(fp, "%127s %127s", secret->username, secret->password);
    if (ret == EOF) {
        return 2;
    }
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
        if (ret == 2) {
            break;
        }
        if (ret != 0) {
            return 1;
        }
        number_secret++;
        if (number_secret >= MAX_ALL_SECRET) {
            perror("too many secrets");
            return 1;
        }
    } while (1);
    return 0;
}

int32_t find_free_slot(client_t *clients) {
    for (int32_t i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == 0)
            return i;
    }
    return -1;
}

void reset_client(client_t *client, int32_t client_fd) {
    memset(client, 0, sizeof(client_t));
    client->fd = client_fd;
}

void add_client(client_t *clients, uint8_t *num_clients, int32_t client_fd) {
    int32_t idx = find_free_slot(clients);
    if (idx == -1) {
        return;
    }
    reset_client(&clients[idx], client_fd);
    (*num_clients)++;
    const char *msg = "Nhap username: ";
    send(client_fd, msg, strlen(msg), 0);
}

void remove_client(client_t *clients, int32_t idx, fd_set *read_fd, uint8_t *num_clients) {
    close(clients[idx].fd);
    FD_CLR(clients[idx].fd, read_fd);
    reset_client(&clients[idx], -1);
    (*num_clients)--;
}

int32_t find_client(client_t *clients, int32_t client_fd) {
    for (int32_t i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == client_fd)
            return i;
    }
    return -1;
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

void handle_client_data(client_t *clients, int32_t idx, fd_set *read_fd, uint8_t *num_clients) {
    char buffer[BUFFER_SIZE];
    int32_t n = recv(clients[idx].fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        printf("client %d disconnected\n", idx);
        remove_client(clients, idx, read_fd, num_clients);
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

int32_t main() {
    init_secrets(all_secrets, &number_secret);
    init_clients(clients, &num_clients);
    FILE *fp = fopen("./secret.txt", "r");
    if (fp == NULL) {
        perror("file not found");
        exit(1);
    }
    if (read_all_secrets(fp) != 0) {
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
    fd_set read_fds, test_fds;
    struct timeval time_out;
    char buf[BUFFER_SIZE];
    FD_ZERO(&read_fds);
    FD_SET(listener, &read_fds);
    int32_t max_fd = listener;

    while (1) {
        test_fds = read_fds;
        time_out.tv_sec = 5;
        time_out.tv_usec = 0;
        int32_t ret = select(max_fd + 1, &test_fds, NULL, NULL, &time_out);
        if (ret == -1) {
            perror("select");
            return 1;
        }
        if (ret == 0) continue;
        for (int32_t i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &test_fds)) {
                if (i == listener) {
                    int32_t client_fd = accept(listener, NULL, NULL);
                    if (client_fd == -1) {
                        perror("accept");
                        continue;
                    }
                    FD_SET(client_fd, &read_fds);
                    if (client_fd > max_fd) {
                        max_fd = client_fd;
                    }
                    add_client(clients, &num_clients, client_fd);
                } else {
                    int32_t idx = find_client(clients, i);
                    if (idx == -1) {
                        FD_CLR(i, &read_fds);
                        continue;
                    }
                    handle_client_data(clients, idx, &read_fds, &num_clients);
                }
            }
        }
    }
    close(listener);
    return 0;
}
