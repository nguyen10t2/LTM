#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#define BUF_SIZE 1024

typedef struct {
  char mssv[20];
  char name[50];
  char birthday[20];
  float cpa;
} student_info_t;

void get_current_time(char *buffer, size_t size) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "tcp_server <PORT> <PATH_LOG>  \n");
    exit(1);
  }

  int port = atoi(argv[1]);
  const char *path_log = argv[2];

  FILE *log_file = fopen(path_log, "a");
  if (log_file == NULL) {
    perror("Failed to open log file");
    exit(1);
  }

  int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == -1) {
    perror("socket() failed");
    exit(1);
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port);

  if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("bind() failed");
    exit(1);
  }

  if (listen(listener, 5) < 0) {
    perror("listen() failed");
    exit(1);
  }

  printf("Server is listening on port %d\n", port);

  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  char client_ip[INET_ADDRSTRLEN];
  char current_time[20];

  while (1) {
    int client = accept(listener, (struct sockaddr *)&client_addr, &addr_len);
    if (client < 0) {
      perror("accept() failed");
      continue;
    }

    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

    char buffer[BUF_SIZE];
    student_info_t student;

    memset(buffer, 0, sizeof(buffer));
    memset(&student, 0, sizeof(student));

    int received = recv(client, buffer, sizeof(buffer) - 1, 0);

    if (received > 0) {
      buffer[received] = '\0';

      printf("Received from client: %s\n", buffer);

      sscanf(buffer, "%[^|]|%[^|]|%[^|]|%f", student.mssv, student.name,
             student.birthday, &student.cpa);
      
      get_current_time(current_time, sizeof(current_time));

      fprintf(log_file, "%s %s %s %s %s %.2f\n", current_time, client_ip, student.mssv, student.name,
              student.birthday, student.cpa);

      fflush(log_file);
    }
  }

  fclose(log_file);
  close(listener);
  return 0;
}