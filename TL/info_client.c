#include <arpa/inet.h>
#include <linux/limits.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024
#define PATH_FILE_SIZE 256

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: tcp_client <IP_ADDRESS> <PORT>\n");
        exit(1);
    }

    int client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == -1) {
        perror("Error");
        return 0;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in sv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };

    if(inet_pton(AF_INET, ip, &sv_addr.sin_addr) <= 0) {
        perror("Error");
        exit(1);
    }

    if(connect(client_socket, (struct sockaddr *)&sv_addr, sizeof(sv_addr)) < 0) {
        perror("Error");
        exit(1);
    }

    printf("Connected\n");

    char cwd[PATH_FILE_SIZE];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        send(client_socket, cwd, PATH_FILE_SIZE, 0);
    }

    DIR *dir = opendir(".");
    struct dirent *entry;
    struct stat file_stat;
    char buffer[BUFFER_SIZE];

    if (dir == NULL) {
        perror("Error");
        exit(1);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (stat(entry->d_name, &file_stat) == 0) {
            if (S_ISREG(file_stat.st_mode)) {
                send(client_socket, entry->d_name, PATH_FILE_SIZE, 0);
                uint32_t file_size = htonl(file_stat.st_size);
                printf("%u\n", htonl(file_size));
                send(client_socket, &file_size, sizeof(file_size), 0);
            }
        }
    }

    closedir(dir);
    close(client_socket);

    return 0;
}