#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 1024

int main() {
    int client = socket(AF_INET, SOCK_STREAM, 0);
    struct addrinfo *res;
    int ret = getaddrinfo("lebavui.io.vn", "80", NULL, &res);
    if (ret != 0) {
        perror("getaddrinfo");
        return 1;
    }

    int connected = connect(client, res->ai_addr, res->ai_addrlen);
    if (connected != 0) {
        perror("connect");
        return 1;
    }

    const char *request = "GET "
        "/samples/video.mp4"
        " HTTP/1.1\r\n"
        "Host: lebavui.io.vn\r\n"
        "Connection: close\r\n\r\n";

    int sent = send(client, request, strlen(request), 0);
    if (sent < 0) {
        perror("send");
        return 1;
    }

    char buffer[MAX_BUFFER_SIZE];
    int header_parsed = 0;
    long content_length = 0;
    long downloaded = 0;

    FILE *fp = fopen("video.mp4", "wb");

    while (1) {
        int rev = recv(client, buffer, MAX_BUFFER_SIZE - 1, 0);
        if (rev <= 0) {
            break;
        }

        if (!header_parsed) {
            char *header_end = strstr(buffer, "\r\n\r\n");
            if (header_end) {
                header_parsed = 1;
                int header_len = header_end - buffer + 4;

                char *cl = strstr(buffer, "Content-Length: ");
                if (cl) {
                    sscanf(cl, "Content-Length: %ld", &content_length);
                }

                int body_size = rev - header_len;

                if (body_size > 0) {
                    fwrite(buffer + header_len, 1, body_size, fp);
                }

                downloaded += body_size;
            }
        }
        else {
            fwrite(buffer, 1, rev, fp);
            downloaded += rev;
        }

        if (content_length > 0) {
            float percent = (downloaded * 100.0f) / content_length;
            printf("\rDownloading: %.2f%%", percent);
            fflush(stdout);
        }
    }

    printf("\n");

    fclose(fp);

    close(client);
    freeaddrinfo(res);

    return 0;
}
