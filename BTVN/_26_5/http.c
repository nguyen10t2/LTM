#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_BUFF_SIZE 1024

double add(double a, double b) {
    return a + b;
}

double sub(double a, double b) {
    return a - b;
}

double mul(double a, double b) {
    return a * b;
}

double divide(double a, double b) {
    if (b == 0) {
        fprintf(stderr, "Error: Division by zero\n");
        return 0; // Return 0 or handle as needed
    }
    return a / b;
}

typedef double (*func)(double, double);

func get_operation(const char op) {
    switch (op) {
        case '+': return add;
        case '-': return sub;
        case '*': return mul;
        case '/': return divide;
        default: return NULL;
    }
}

void handle_method_get(int client, const char *path) {
    char op;
    double a, b;
    if (sscanf(path, "/calc?op=%c&a=%lf&b=%lf", &op, &a, &b) != 3) {
        char response[] = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Bad Request</h1></body></html>";
        send(client, response, strlen(response), 0);
        return;
    }

    func operation = get_operation(op);
    if (operation == NULL) {
        char response[] = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Unknown Operator</h1></body></html>";
        send(client, response, strlen(response), 0);
        return;
    }

    double result = operation(a, b);
    char response[256];
    snprintf(response, sizeof(response), "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Result: %.2lf %c %.2lf =  %.2lf</h1></body></html>", a, op, b, result);
    send(client, response, strlen(response), 0);
}

void handle_method_post(int client, const char *path, const char *body) {
    if (strncmp(path, "/calc", 5) != 0) {
        char response[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Not Found</h1></body></html>";
        send(client, response, strlen(response), 0);
        return;
    }

    if (body == NULL) {
        char response[] = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Bad Request</h1></body></html>";
        send(client, response, strlen(response), 0);
        return;
    }

    char op;
    double a, b;
    if (sscanf(body, "{\"op\": \"%c\", \"a\": %lf, \"b\": %lf}", &op, &a, &b) != 3) {
        char response[] = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Bad Request</h1></body></html>";
        send(client, response, strlen(response), 0);
        return;
    }

    func operation = get_operation(op);
    if (operation == NULL) {
        char response[] = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Unknown Operator</h1></body></html>";
        send(client, response, strlen(response), 0);
        return;
    }

    double result = operation(a, b);
    char response[256];
    snprintf(response, sizeof(response), "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Result: %.2lf %c %.2lf =  %.2lf</h1></body></html>", a, op, b, result);
    send(client, response, strlen(response), 0);
}

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
        buff[n] = '\0';

        // METHOD PATH HTTP/1.1
        // GET: path: /calc?op=+&a=5&b=3
        // POST: path: /calc, body json: {"op": "+", "a": 5, "b": 3}
        char method[10], path[100];
        sscanf(buff, "%9s %99s", method, path);
        if (strncmp(method, "GET", 3) == 0) {
            handle_method_get(client, path);
            close(client);
        }
        else if (strncmp(method, "POST", 4) == 0) {
            char *body = strstr(buff, "\r\n\r\n");
            if (body) {
                body += 4;
            }
            handle_method_post(client, path, body);
            close(client);
        }
        else {
            char response[] = "HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Method Not Allowed</h1></body></html>";
            send(client, response, strlen(response), 0);
            close(client);
        }
    }
    return 0;
}
