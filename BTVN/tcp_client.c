#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: tcp_client <IP_ADDRESS> <PORT>\n");
    exit(1);
  }

  const char *ip_des = argv[1];
  int port = atoi(argv[2]);

  int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (client == -1) {
    perror("socket() failed");
    exit(1);
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, ip_des, &server_addr.sin_addr) <= 0) {
    perror("Invalid address");
    exit(1);
  }

  if (connect(client, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("connect() failed");
    exit(1);
  }

  printf("Connected to server %s:%d\n", ip_des, port);

  char buffer[BUF_SIZE];

  while (1) {
    printf("Enter message: ");
    fflush(stdout);

    if (fgets(buffer, BUF_SIZE, stdin) == NULL)
      break;

    int sent = send(client, buffer, strlen(buffer), 0);
    if (sent <= 0) {
      perror("send() failed");
      break;
    }
  }

  close(client);
  return 0;
}