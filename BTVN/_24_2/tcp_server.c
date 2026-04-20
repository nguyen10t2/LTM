#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
  if (argc < 4) {
    fprintf(stderr, "tcp_server <PORT> <PATH_HELLO> <PATH_LOG>  \n");
    exit(1);
  }

  int port = atoi(argv[1]);
  const char *path_hello = argv[2];
  const char *path_log = argv[3];

  FILE *hello_file = fopen(path_hello, "r");
  if (hello_file == NULL) {
    perror("Failed to open hello file");
    exit(1);
  }

  char hello_message[BUF_SIZE];
  if (fgets(hello_message, BUF_SIZE, hello_file) == NULL) {
    perror("Failed to read hello message");
    fclose(hello_file);
    exit(1);
  }
  hello_message[strlen(hello_message)] = '\0';

  FILE *log_file = fopen(path_log, "a");
  if (log_file == NULL) {
    perror("Failed to open log file");
    fclose(hello_file);
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

  int client = accept(listener, NULL, NULL);
  if (client < 0) {
    perror("accept() failed");
    exit(1);
  }

  if (send(client, hello_message, strlen(hello_message), 0) <= 0) {
    perror("send() failed");
    close(client);
    exit(1);
  }

  char buffer[BUF_SIZE];
  while (1) {
    int received = recv(client, buffer, BUF_SIZE - 1, 0);
    if (received <= 0) {
      if (received == 0) {
        printf("Client disconnected\n");
      } else {
        perror("recv() failed");
      }
      break;
    }

    buffer[received] = '\0';
    fprintf(log_file, "%s", buffer);
    fflush(log_file);
  }

  fclose(hello_file);
  fclose(log_file);
  close(client);
  close(listener);
  return 0;
}