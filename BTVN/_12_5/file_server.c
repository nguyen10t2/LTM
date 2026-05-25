#include <ctype.h>
#include <dirent.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024
#define MAX_FILES 16
#define MAX_FILENAME_LEN 256

const char *path_file = ".";

char **name_files;
uint8_t len_name_files;

void init_files() {
    name_files = malloc(MAX_FILES * sizeof(char *));
    for (int i = 0; i < MAX_FILES; i++) {
        name_files[i] = malloc(MAX_FILENAME_LEN);
    }
    len_name_files = 0;
}

void read_files() {
    len_name_files = 0;
    DIR *dir = opendir(path_file);
    struct dirent *entry;
    if (dir == NULL) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (len_name_files < MAX_FILES) {
            strncpy(name_files[len_name_files], entry->d_name, MAX_FILENAME_LEN - 1);
            name_files[len_name_files][MAX_FILENAME_LEN - 1] = '\0';
            len_name_files++;
        }
    }
    closedir(dir);
}

void sigchld_handler(int s) {
    (void)s;
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

struct tm *get_current_time() {
    time_t now = time(NULL);
    return localtime(&now);
}

bool is_exit(char *str) {
    for (size_t i = 0; i < strlen(str); i++) {
        str[i] = tolower(str[i]);
    }
    return strcmp(str, "exit") == 0;
}

char *first_msg() {
    char *msg = calloc(1, BUFFER_SIZE);
    if (len_name_files == 0) {
        snprintf(msg, BUFFER_SIZE, "ERROR No files to download\n");
    } else {
        snprintf(msg, BUFFER_SIZE, "Ok %d\r\n", len_name_files);
        for (int i = 0; i < len_name_files; i++) {
            strncat(msg, name_files[i], BUFFER_SIZE - strlen(msg) - 1);
            strncat(msg, "\r\n", BUFFER_SIZE - strlen(msg) - 1);
        }
        strncat(msg, "\r\n", BUFFER_SIZE - strlen(msg) - 1);
    }
    return msg;
}

int find_file(const char *filename) {
    for (int i = 0; i < len_name_files; i++) {
        if (strcmp(name_files[i], filename) == 0) {
            return i;
        }
    }
    return -1;
}

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    const char *msg = first_msg();
    send(client_fd, msg, strlen(msg), 0);
    while (1) {
        send(client_fd, "Enter file name to download: ", 30, 0);
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) break;
        buffer[strcspn(buffer, "\r\n")] = '\0';
        if (is_exit(buffer)) {
            send(client_fd, "Goodbye!\n", 9, 0);
            break;
        }
        int index_file = find_file(buffer);
        if (index_file == -1) {
            send(client_fd, "ERROR File not found\n", 21, 0);
            continue;
        }
        struct stat file_stat;
        if (stat(name_files[index_file], &file_stat) == -1) {
            send(client_fd, "ERROR Cannot access file\n", 25, 0);
            continue;
        }
        if (!S_ISREG(file_stat.st_mode)) {
            send(client_fd, "ERROR Not a regular file\n", 25, 0);
            continue;
        }
        char cmd_file[BUFFER_SIZE];
        snprintf(cmd_file, sizeof(cmd_file), "OK %ld\r\n", file_stat.st_size);
        send(client_fd, cmd_file, strlen(cmd_file), 0);
        FILE *file = fopen(name_files[index_file], "rb");
        if (file == NULL) {
            send(client_fd, "ERROR Cannot open file\n", 23, 0);
            continue;
        }
        while (fgets(buffer, BUFFER_SIZE - 1, file) != NULL) {
            send(client_fd, buffer, strlen(buffer), 0);
        }
        fclose(file);
    }
    close(client_fd);
}

int main() {
    init_files();
    read_files();

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(9000),
        .sin_addr = { .s_addr = INADDR_ANY },
    };

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(1);
    }

    printf("Server listening on port 9000\n");

    signal(SIGCHLD, sigchld_handler);

    while (1) {
        int client_fd = accept(listener, NULL, NULL);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(listener);
            handle_client(client_fd);
            exit(0);
        }
        else if (pid > 0) {
            // Parent process
            close(client_fd);
        } else {
            perror("fork");
            close(client_fd);
        }
    }

    close(listener);
    return 0;
}
