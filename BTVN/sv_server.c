#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 1024

typedef struct {
  char mssv[20];
  char name[50];
  char birthday[20];
  float cpa;
} student_info_t;

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

  while (1) {
    int client = accept(listener, (struct sockaddr *)&client_addr, &addr_len);
    if (client < 0) {
      perror("accept() failed");
      continue;
    }

    student_info_t student;
    int total = 0;
    int len = sizeof(student_info_t);

    while (total < len) {
      int received = recv(client, ((char *)&student) + total, len - total, 0);
      if (received <= 0)
        break;
      total += received;
    }

    printf("Received: %s | %s | %s | %.2f\n",
       student.mssv,
       student.name,
       student.birthday,
       student.cpa);

    if (total == len) {
      fprintf(log_file, "%s %d %s %s %s %.2f\n",
              inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
              student.mssv, student.name, student.birthday, student.cpa);
      fflush(log_file);
    }
  }

  fclose(log_file);
  close(listener);
  return 0;
}