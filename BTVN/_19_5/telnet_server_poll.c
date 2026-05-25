#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_USERNAME_LEN 128
#define MAX_PASSWORD_LEN 128
#define MAX_ALL_SECRET 128
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

secret_t all_secrets[MAX_ALL_SECRET];
uint8_t number_secret = 0;

void init_secrets() {
    memset(all_secrets, 0, sizeof(all_secrets));
    number_secret = 0;
}

uint8_t read_all_secrets(FILE *fp) {
    while (number_secret < MAX_ALL_SECRET) {
        if (fscanf(fp, "%127s %127s", all_secrets[number_secret].username, all_secrets[number_secret].password) != 2) {
            break;
        }
        number_secret++;
    }
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

void handle_client_command(int client_fd, const char *cmd) {
    char out_file[64];
    snprintf(out_file, sizeof(out_file), "./out_%d.txt", client_fd);
    
    char fullcmd[MAX_CMD_LEN + 64];
    snprintf(fullcmd, sizeof(fullcmd), "%s > %s 2>&1", cmd, out_file);
    system(fullcmd);
    
    FILE *fp = fopen(out_file, "r");
    if (fp) {
        char line[BUFFER_SIZE];
        while (fgets(line, sizeof(line), fp)) {
            send(client_fd, line, strlen(line), 0);
        }
        fclose(fp);
        remove(out_file);
    }
    const char *msg = "Nhap lenh: ";
    send(client_fd, msg, strlen(msg), 0);
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    char input_username[MAX_USERNAME_LEN] = {0};
    char input_password[MAX_PASSWORD_LEN] = {0};
    state_t state = STATE_USERNAME;

    struct pollfd fds[1];
    fds[0].fd = client_fd;
    fds[0].events = POLLIN;

    send(client_fd, "Nhap username: ", 15, 0);

    while (1) {
        int ret = poll(fds, 1, -1);
        if (ret <= 0) break;

        if (fds[0].revents & POLLIN) {
            int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (n <= 0) break;
            
            buffer[n] = '\0';
            buffer[strcspn(buffer, "\r\n")] = '\0';

            switch (state) {
                case STATE_USERNAME:
                    strncpy(input_username, buffer, MAX_USERNAME_LEN - 1);
                    send(client_fd, "Nhap password: ", 15, 0);
                    state = STATE_PASSWORD;
                    break;
                case STATE_PASSWORD:
                    strncpy(input_password, buffer, MAX_PASSWORD_LEN - 1);
                    if (check_credentials(input_username, input_password)) {
                        send(client_fd, "Dang nhap thanh cong!\nNhap lenh: ", 33, 0);
                        state = STATE_AUTHENTICATED;
                    } else {
                        send(client_fd, "Sai username hoac password!\nNhap username: ", 43, 0);
                        state = STATE_USERNAME;
                    }
                    break;
                case STATE_AUTHENTICATED:
                    handle_client_command(client_fd, buffer);
                    break;
            }
        }
    }

    close(client_fd);
    return NULL;
}

int main() {
    init_secrets();
    FILE *fp = fopen("./secret.txt", "r");
    if (fp == NULL) {
        perror("secret.txt not found");
        return 1;
    }
    read_all_secrets(fp);
    fclose(fp);

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
    printf("Telnet Server (Threaded + Poll) listening on port 9000\n");

    while (1) {
        int client_fd = accept(listener, NULL, NULL);
        if (client_fd >= 0) {
            int *p_client_fd = malloc(sizeof(int));
            if (p_client_fd) {
                *p_client_fd = client_fd;
                pthread_t thread;
                if (pthread_create(&thread, NULL, handle_client, p_client_fd) == 0) {
                    pthread_detach(thread);
                } else {
                    perror("pthread_create");
                    free(p_client_fd);
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
