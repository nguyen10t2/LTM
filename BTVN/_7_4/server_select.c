#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_ID_NAME 32
#define MAX_CLIENTS 512
#define MAX_BUFFER 1024
#define MAX_MSG 128
#define MAX_TIME_STR 64

typedef struct {
    char name[MAX_ID_NAME];
    int32_t fd;
    uint8_t registered;
} client_t;

void get_time_str(char *buffer, int32_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y/%m/%d %H:%M:%S", t);
}

void init_clients(client_t *clients, uint16_t *num_clients) {
    memset(clients, 0, sizeof(client_t) * MAX_CLIENTS);
    *num_clients = 0;
}

int32_t find_free_slot(client_t *clients) {
    for (int32_t i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == 0)
            return i;
    }
    return -1;
}

void add_client(client_t *clients, int32_t fd, uint16_t *num_clients) {
    int32_t idx = find_free_slot(clients);
    if (idx == -1) {
        close(fd);
        return;
    }

    memset(&clients[idx], 0, sizeof(client_t));
    clients[idx].fd = fd;
    (*num_clients)++;

    char *msg = "Nhap dung format: client_id: client_name\n";
    send(fd, msg, strlen(msg), 0);
}

void remove_client(client_t *clients, int32_t idx, fd_set *read_fd, uint16_t *num_clients) {
    close(clients[idx].fd);
    FD_CLR(clients[idx].fd, read_fd);

    memset(&clients[idx], 0, sizeof(client_t));
    (*num_clients)--;
}

int32_t parse_register(char *buffer, char *id_out, char *name_out) {
    char id[MAX_ID_NAME];
    char name[MAX_ID_NAME];

    if (sscanf(buffer, "%[^:]: %s", id, name) == 2) {
        if (strcmp(id, "client_id")) {
            return -1;
        }
        strncpy(name_out, name, MAX_ID_NAME - 1); // lưu client_name
        return 1;
    }
    return 0;
}

void broadcast(client_t *clients, uint16_t *num_clients, int32_t sender_id, const char *msg) {
    for (uint16_t i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].fd != 0 && i != sender_id) {
            send(clients[i].fd, msg, strlen(msg), 0);
        }
    }
}

void handle_client_data(client_t *clients, int32_t idx, fd_set *read_fd, uint16_t *num_clients) {
    char buffer[MAX_BUFFER];
    int32_t fd = clients[idx].fd;
    int32_t ret = recv(fd, buffer, MAX_BUFFER - 1, 0);
    if (ret <= 0) {
        printf("Client %d disconnected\n", clients[idx].fd);
        remove_client(clients, idx, read_fd, num_clients);
        return;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';
    buffer[ret] = '\0';

    if (!clients[idx].registered) {
        char id[MAX_ID_NAME];
        char name[MAX_ID_NAME];
        if (parse_register(buffer, id, name) == 1) {
            strncpy(clients[idx].name, name, MAX_ID_NAME - 1);
            clients[idx].registered = 1;

            char msg[MAX_MSG];
            sprintf(msg, "Dang ky thanh cong: %s\n", name);
            send(fd, msg, strlen(msg), 0);
            printf("Client id=%d dang ky ten: %s\n", clients[idx].fd, name);
        } else {
            char *err = "Sai format. Dung: client_id: client_name\n";
            send(fd, err, strlen(err), 0);
        }
    }
    else {
        char msg[MAX_BUFFER + MAX_MSG];
        char time_str[MAX_TIME_STR];

        get_time_str(time_str, sizeof(time_str));
        printf("%s %d %s\n", time_str, clients[idx].fd, buffer);
        snprintf(msg, sizeof(msg), "%s %d: %s\n", time_str, clients[idx].fd, buffer);

        broadcast(clients, num_clients, idx, msg);
    }
}

int32_t main() {
    int32_t listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(9000),
    };

    int32_t opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(listener, MAX_CLIENTS) == -1) {
        perror("listen");
        return 1;
    }

    fd_set read_fd, test_fd;
    struct timeval timeout;

    client_t clients[MAX_CLIENTS];
    uint16_t num_clients = 0;
    init_clients(clients, &num_clients);

    FD_ZERO(&read_fd);
    FD_SET(listener, &read_fd);

    int32_t max_fd = listener;

    while (1) {
        test_fd = read_fd;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        int32_t ret = select(max_fd + 1, &test_fd, NULL, NULL, &timeout);
        if (ret < 0) {
            perror("select");
            break;
        }
        if (ret == 0)
            continue;

        for (int32_t i = 0; i <= max_fd; i++) {
            if (!FD_ISSET(i, &test_fd))
                continue;

            if (i == listener) {
                int32_t client_fd = accept(listener, NULL, NULL);
                if (client_fd >= 0) {
                    FD_SET(client_fd, &read_fd);
                    add_client(clients, client_fd, &num_clients);
                    if (client_fd > max_fd)
                        max_fd = client_fd;
                }
            }
            else {
                for (int32_t j = 0; j < MAX_CLIENTS; j++) {
                    if (clients[j].fd == i) {
                        handle_client_data(clients, j, &read_fd, &num_clients);
                        break;
                    }
                }
            }
        }
    }

    close(listener);
    return 0;
}
