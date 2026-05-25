#include <dirent.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024
#define MAX_FILES 16
#define MAX_FILENAME_LEN 256

const char *path_file = ".";

char **name_files;
uint8_t len_name_files;
pthread_mutex_t files_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_files() {
    name_files = malloc(MAX_FILES * sizeof(char *));
    for (int i = 0; i < MAX_FILES; i++) {
        name_files[i] = malloc(MAX_FILENAME_LEN);
    }
    len_name_files = 0;
}

void free_files() {
    for (int i = 0; i < MAX_FILES; i++) {
        free(name_files[i]);
    }
    free(name_files);
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

struct tm *get_current_time() {
    time_t now = time(NULL);
    static __thread struct tm result;  // Thread-local storage
    localtime_r(&now, &result);
    return &result;
}

bool is_exit(const char *str) {
    return strncasecmp(str, "exit", 4) == 0 && (str[4] == '\0' || str[4] == '\r' || str[4] == '\n');
}

char *first_msg() {
    char *msg = calloc(1, BUFFER_SIZE);
    if (!msg) return NULL;

    if (len_name_files == 0) {
        snprintf(msg, BUFFER_SIZE, "ERROR No files to download\n");
    } else {
        int offset = snprintf(msg, BUFFER_SIZE, "Ok %d\r\n", len_name_files);
        for (int i = 0; i < len_name_files; i++) {
            int written = snprintf(msg + offset, BUFFER_SIZE - offset, "%s\r\n", name_files[i]);
            if (written > 0) offset += written;
            if (offset >= BUFFER_SIZE - 3) break; // Leave room for final \r\n
        }
        snprintf(msg + offset, BUFFER_SIZE - offset, "\r\n");
    }

    return msg;
}

int find_file(const char *filename, char *out_filename, size_t out_size) {
    int result = -1;
    for (int i = 0; i < len_name_files; i++) {
        if (strcmp(name_files[i], filename) == 0) {
            strncpy(out_filename, name_files[i], out_size - 1);
            out_filename[out_size - 1] = '\0';
            result = i;
            break;
        }
    }
    return result;
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    char filename_buf[MAX_FILENAME_LEN];
    char *msg = first_msg();
    if (msg) {
        send(client_fd, msg, strlen(msg), 0);
        free(msg);
    }

    while (1) {
        send(client_fd, "Enter file name to download: ", 30, 0);
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) break;
        buffer[strcspn(buffer, "\r\n")] = '\0';
        if (is_exit(buffer)) {
            send(client_fd, "Goodbye!\n", 9, 0);
            break;
        }
        int index_file = find_file(buffer, filename_buf, sizeof(filename_buf));
        if (index_file == -1) {
            send(client_fd, "ERROR File not found\n", 21, 0);
            continue;
        }
        struct stat file_stat;
        if (stat(filename_buf, &file_stat) == -1) {
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
        FILE *file = fopen(filename_buf, "rb");
        if (file == NULL) {
            send(client_fd, "ERROR Cannot open file\n", 23, 0);
            continue;
        }

        size_t n;
        while ((n = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
            send(client_fd, buffer, n, 0);
        }
        fclose(file);
    }
    close(client_fd);
    return NULL;
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

    while (1) {
        int client_fd = accept(listener, NULL, NULL);
        if (client_fd >= 0) {
            int *p_client_fd = malloc(sizeof(int));
            if (p_client_fd) {
                *p_client_fd = client_fd;
                pthread_t thread;
                if (pthread_create(&thread, NULL, handle_client, p_client_fd) != 0) {
                    perror("pthread_create");
                    free(p_client_fd);
                    close(client_fd);
                } else {
                    pthread_detach(thread);
                }
            } else {
                close(client_fd);
            }
        }
    }

    free_files();
    close(listener);
    return 0;
}
