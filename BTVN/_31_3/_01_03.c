#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 2048

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Sử dụng: %s <PORT>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("socket() failed");
        return 1;
    }

    struct sockaddr_in sv_addr, cl_addr;
    memset(&sv_addr, 0, sizeof(sv_addr));

    sv_addr.sin_family = AF_INET;
    sv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    sv_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&sv_addr, sizeof(sv_addr)) < 0) {
        perror("bind() failed");
        return 1;
    }

    printf("Server is listening on port %d\n", port);

    char buffer[BUFFER_SIZE];
    socklen_t cl_addr_len = sizeof(cl_addr);

    while (1) {
        ssize_t n = recvfrom(sockfd, buffer, BUFFER_SIZE - 11, 0, (struct sockaddr*)&cl_addr, &cl_addr_len);
        if (n < 0) {
            perror("recvfrom() failed");
            continue;
        }
        buffer[n] = '\0';

        printf("Received from client %s:%d - %s\n", inet_ntoa(cl_addr.sin_addr), ntohs(cl_addr.sin_port), buffer);

        sendto(sockfd, buffer, n, 0, (const struct sockaddr *)&cl_addr, cl_addr_len);
    }

    close(sockfd);
    return 0;
}