#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 1024

typedef struct {
  char name[50];
  char mssv[20];
  char birthday[20];
  char cpa[10];
} student_info_t;

void read_info(student_info_t *student) {
  printf("Enter student information:\n");
  printf("MSSV: ");
  fgets(student->mssv, sizeof(student->mssv), stdin);
  student->mssv[strcspn(student->mssv, "\n")] = '\0';

  printf("Name: ");
  fgets(student->name, sizeof(student->name), stdin);
  student->name[strcspn(student->name, "\n")] = '\0';

  printf("Birthday (dd/mm/yyyy): ");
  fgets(student->birthday, sizeof(student->birthday), stdin);
  student->birthday[strcspn(student->birthday, "\n")] = '\0';

  printf("CPA: ");
  fgets(student->cpa, sizeof(student->cpa), stdin);
  student->cpa[strcspn(student->cpa, "\n")] = '\0';
}

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
  student_info_t student;
  memset(&student, 0, sizeof(student));

  while (1) {
    read_info(&student);

    printf("Send: %s %s %s %s\n", student.mssv, student.name,
           student.birthday, student.cpa);

    memset(buffer, 0, sizeof(buffer));

    snprintf(buffer, BUF_SIZE, "%s|%s|%s|%s\n", student.mssv, student.name,
             student.birthday, student.cpa);

    if (send(client, buffer, strlen(buffer), 0) == -1) {
      perror("send() failed");
      break;
    }
  }

  close(client);
  return 0;
}