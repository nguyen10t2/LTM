#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#define MAX_BUFFER_SIZE         1024
#define MAX_FULL_PATH           127
#define MAX_HTML_SIZE           65536

const char *ROOT = "../../../LTM";

void send_directory(int client, const char *real_path, const char *url_path) {
    DIR *dir = opendir(real_path);
    if (!dir) return;

    char html[MAX_HTML_SIZE];
    strcpy(html, "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "</head>"
        "<body>"
        "<h1>Directory Listing</h1>");

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0) continue;

        char line[MAX_BUFFER_SIZE], href[MAX_BUFFER_SIZE];

        if(strcmp(url_path, "/") == 0) {
            snprintf(href, sizeof(href), "/%s", entry->d_name);
        }
        else {
            snprintf(href, sizeof(href), "%s/%s", url_path, entry->d_name);
        }

        char child[MAX_BUFFER_SIZE];
        snprintf(child, sizeof(child), "%s/%s", real_path, entry->d_name);

        struct stat st;
        stat(child, &st);

        if (S_ISDIR(st.st_mode)) {
            snprintf(line, sizeof(line), "<h2><b>📁 <a href=\"%s\">%s</a></b><br></h2>", href, entry->d_name);
        }
        else {
            snprintf(line, sizeof(line), "<h2><i>📄 <a href=\"%s\">%s</a></i><br></h2>", href, entry->d_name);
        }

        strcat(html, line);
    }

    strcat(html, "</body></html>");

    closedir(dir);

    char header[256];

    sprintf(header,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %zu\r\n\r\n",
        strlen(html));

    send(client, header, strlen(header), 0);
    send(client, html, strlen(html), 0);
}

const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');

    if (!ext)
        return "application/octet-stream";

    if (!strcmp(ext, ".txt"))
        return "text/plain";

    if (!strcmp(ext, ".html"))
        return "text/html";

    if (!strcmp(ext, ".jpg"))
        return "image/jpeg";

    if (!strcmp(ext, ".jpeg"))
        return "image/jpeg";

    if (!strcmp(ext, ".png"))
        return "image/png";

    if (!strcmp(ext, ".gif"))
        return "image/gif";

    if (!strcmp(ext, ".mp3"))
        return "audio/mpeg";

    if (!strcmp(ext, ".mp4"))
        return "video/mp4";

    if (!strcmp(ext, ".c"))
        return "text/plain";

    if (!strcmp(ext, ".cpp"))
        return "text/plain";

    if (!strcmp(ext, ".h"))
        return "text/plain";

    if (!strcmp(ext, ".py"))
        return "text/plain";

    if (!strcmp(ext, ".rs"))
        return "text/plain";

    if (!strcmp(ext, ".java"))
        return "text/plain";

    return "application/octet-stream";
}

void send_file(int client, const char *path) {
    FILE *fp = fopen(path, "rb");

    if (!fp) return;

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    const char *mime = get_mime_type(path);

    char header[512];

    sprintf(header,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n\r\n",
        mime,
        filesize);

    send(client, header, strlen(header), 0);

    char buffer[4096];

    size_t n;

    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        send(client, buffer, n, 0);
    }

    fclose(fp);
}

void handle_method_get(int client, const char *path) {
    char fullpath[MAX_FULL_PATH];

    snprintf(fullpath, sizeof(fullpath), "%s%s", ROOT, path);

    struct stat st;
    if (stat(fullpath, &st) == -1) {
        char response[] =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<h1>404 Not Found</h1>";

        send(client, response, strlen(response), 0);
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        send_directory(client, fullpath, path);
    }
    else if (S_ISREG(st.st_mode)) {
        send_file(client, fullpath);
    }
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

    char buff[MAX_BUFFER_SIZE];

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client == -1) {
            perror("accept");
            continue;
        }

        int n = recv(client, buff, MAX_BUFFER_SIZE - 1, 0);
        if (n == -1) {
            perror("recv");
            close(client);
            continue;
        }
        buff[n] = '\0';

        char method[10], path[64];
        sscanf(buff, "%9s %63s", method, path);
        if (strncmp(method, "GET", 3) == 0) {
            handle_method_get(client, path);
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
