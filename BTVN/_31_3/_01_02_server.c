#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BLOCK_SIZE 1024
const char *KEY = "0123456789";

uint8_t count_occurrences(const char *block, const char *key) {
    uint8_t count = 0;
    const char *ptr = block;
    while ((ptr = strstr(ptr, key)) != NULL) {
        count++;
        ptr += strlen(key);
    }
    return count;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <PORT>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);

    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket() failed");
        exit(1);
    }

    struct sockaddr_in sv_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port)
    };

    if (bind(listener, (struct sockaddr *)&sv_addr, sizeof(sv_addr)) < 0) {
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
            perror("accept() failed");
            exit(1);
        }

        char block[BLOCK_SIZE];
        int key_len = strlen(KEY);

        char leftover[16] = "";
        long total_occurrences = 0;

        while (1) {
            int ret = recv(client, block, BLOCK_SIZE - 1, 0);
            if (ret <= 0) break;
            block[ret] = '\0';

            int leftover_len = strlen(leftover);

            char *temp_process = malloc(leftover_len + ret + 1);
            if (!temp_process) {
                perror("malloc() failed");
                break;
            }
            memcpy(temp_process, leftover, leftover_len);
            memcpy(temp_process + leftover_len, block, ret);
            temp_process[leftover_len + ret] = '\0';

            total_occurrences += count_occurrences(temp_process, KEY);

            int temp_len = leftover_len + ret;
            int tail = key_len - 1;
            if (temp_len >= tail) {
                memcpy(leftover, temp_process + temp_len - tail, tail);
                leftover[tail] = '\0';
            } else {
                memcpy(leftover, temp_process, temp_len);
                leftover[temp_len] = '\0';
            }

            printf("Tổng số lần tìm thấy cho đến nay: %ld\n", total_occurrences);

            free(temp_process);
        }

        close(client);
        printf("Client kết thúc. Tổng cộng tìm thấy: %ld\n", total_occurrences);
    }

    close(listener);
    return 0;
}