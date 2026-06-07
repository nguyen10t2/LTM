#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_BUFF_SIZE 1024

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(9000)
    };

    int32_t opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return -1;
    }

    if (listen(listener, 10) == -1) {
        perror("listen");
        return -1;
    }

    printf("Server listening on port 9000\n");

    char buff[MAX_BUFF_SIZE];
    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client == -1) {
            perror("accept");
            continue;
        }

        int n = recv(client, buff, MAX_BUFF_SIZE - 1, 0);
        if (n == -1) {
            perror("recv");
            close(client);
            continue;
        }

        char method[10], path[100];
        sscanf(buff, "%s %s", method, path);

        if (strncmp(path, "/me", 3) == 0) {
            char response[] =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n\r\n"

            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<title>Profile</title>"

            "<style>"
            "body {"
            "    margin: 0;"
            "    height: 100vh;"
            "    display: flex;"
            "    justify-content: center;"
            "    align-items: center;"
            "    background: linear-gradient(135deg, #ff0080, #7928ca, #00c6ff);"
            "    font-family: Arial, sans-serif;"
            "    overflow: hidden;"
            "}"

            ".card {"
            "    background: rgba(255,255,255,0.15);"
            "    backdrop-filter: blur(12px);"
            "    padding: 50px;"
            "    border-radius: 25px;"
            "    text-align: center;"
            "    box-shadow: 0 0 40px rgba(0,0,0,0.4);"
            "    animation: float 3s ease-in-out infinite;"
            "}"

            "h1 {"
            "    font-size: 55px;"
            "    margin: 0;"
            "    color: #fff;"
            "    text-shadow: 0 0 15px #fff;"
            "}"

            "h2 {"
            "    font-size: 35px;"
            "    color: #ffe600;"
            "    margin-top: 20px;"
            "}"

            "p {"
            "    color: #fff;"
            "    font-size: 20px;"
            "}"

            ".stars {"
            "    position: absolute;"
            "    width: 100%;"
            "    height: 100%;"
            "    background-image:"
            "        radial-gradient(white 1px, transparent 1px),"
            "        radial-gradient(white 1px, transparent 1px);"
            "    background-size: 50px 50px;"
            "    background-position: 0 0, 25px 25px;"
            "    opacity: 0.25;"
            "}"

            "@keyframes float {"
            "    0% { transform: translateY(0px); }"
            "    50% { transform: translateY(-20px); }"
            "    100% { transform: translateY(0px); }"
            "}"
            "</style>"

            "</head>"

            "<body>"

            "<div class='stars'></div>"

            "<div class='card'>"
            "    <h1>Đào Trọng Nguyên</h1>"
            "    <h2>20235390</h2>"
            "</div>"

            "</body>"
            "</html>";
            send(client, response, strlen(response), 0);
        }
        else if (strncmp(path, "/friend", 7) == 0) {
            char response[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n<html><body><h1>Trần Ngọc Toàn</h1>\n<h2>20235439</h2></body></html>";
            send(client, response, strlen(response), 0);
        }
        else {
            char response[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Not Found</h1></body></html>";
            send(client, response, strlen(response), 0);
        }

        close(client);
    }

    close(listener);

    return 0;
}
