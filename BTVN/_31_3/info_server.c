#include <arpa/inet.h>
#include <linux/limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER 1024
#define PATH_FILE_SIZE 256

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "tcp_server <PORT> \n");
        exit(1);
    }

    int port = atoi(argv[1]);

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        exit(1);
    }

    struct sockaddr_in sv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(listener, (struct sockaddr *)&sv_addr, sizeof(sv_addr)) <
    0) {
        perror("bind() failed");
        exit(1);
    }

    if (listen(listener, 5) < 0) {
        perror("listen() failed");
        exit(1);
    }

    printf("Server is listening on port %d\n", port);

    while (1) {

        int client = accept(listener, NULL, NULL);
        if (client < 0) {
            perror("Error");
            exit(1);
        }

        char path_buffer[PATH_FILE_SIZE];

        int ret = recv(client, path_buffer, PATH_FILE_SIZE, 0);
        if (ret > 0) {
            printf("Thu muc hien tai cua client: %s\n", path_buffer);
        }

        while (1) {
            ret = recv(client, path_buffer, PATH_FILE_SIZE, MSG_WAITALL);
            if (ret <= 0) {
                printf("Client disconnected\n");
                break;
            }

            uint32_t file_size;
            ret = recv(client, &file_size, sizeof(file_size), MSG_WAITALL);
            if (ret <= 0) {
                printf("Client disconnected\n");
                break;
            }

            uint32_t file_size_host = ntohl(file_size);
            printf("File: %s, Size: %u bytes\n", path_buffer, file_size_host);
        }

        close(client);
    }

    close(listener);
    return 0;
}