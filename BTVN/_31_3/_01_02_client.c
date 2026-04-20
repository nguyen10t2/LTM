#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

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
    char buffer[BUFFER_SIZE];

    while (1) {
        printf("Enter message: ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }

        buffer[strcspn(buffer, "\n")] = '\0';

        if (strcmp(buffer, "exit") == 0) {
            break;
        }
        
        if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
            perror("Error");
            break;
        }
    }  
    
    close(client_socket);

    return 0;
}