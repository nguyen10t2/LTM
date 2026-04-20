/*******************************************************************************
 * @file    non_blocking_server.c
 * @brief   Mô tả ngắn gọn về chức năng của file
 * @date    2026-03-31 07:10
 *******************************************************************************/

#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_NAME 50
#define MAX_MSSV 10
#define MAX_CLIENTS 5
#define MAX_EMAIL_LENGTH 101

typedef struct {
  char name[MAX_NAME];
  char mssv[MAX_MSSV];
} student_info_t;

typedef enum {
  WAIT_NAME,
  WAIT_MSSV,
  DONE,
} state_t;

typedef struct {
  int fd;
  state_t state;
  student_info_t info;
} client_info_t;

void send_msg(int client, const char *msg) {
  send(client, msg, strlen(msg), 0);
}

char *generate_hust_email(student_info_t *info) {
  char *email_res = malloc(MAX_EMAIL_LENGTH * sizeof(char));
  if (!email_res)
    return NULL;

  char temp_name[MAX_NAME];
  strncpy(temp_name, info->name, MAX_NAME - 1);
  temp_name[MAX_NAME - 1] = '\0';

  char *words[10];
  int count = 0;

  char *token = strtok(temp_name, " ");
  while (token != NULL && count < 10) {
    words[count++] = token;
    token = strtok(NULL, " ");
  }

  if (count < 2) {
    free(email_res);
    return NULL;
  }

  char first_name[MAX_NAME];
  strcpy(first_name, words[count - 1]);

  char initials[10] = "";
  for (int i = 0; i < count - 1; i++) {
    strncat(initials, &words[i][0], 1);
  }

  char *short_mssv = (strlen(info->mssv) > 2) ? (info->mssv + 2) : info->mssv;

  sprintf(email_res, "%s.%s%s@sis.hust.edu.vn", first_name, initials,
          short_mssv);

  return email_res;
}

int main() {
  int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == -1) {
    perror("socket() failed");
    return 1;
  }

  // Chuyen socket listener sang non-blocking
  unsigned long ul = 1;
  ioctl(listener, FIONBIO, &ul);

  if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
    perror("setsockopt() failed");
    close(listener);
    return 1;
  }

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(8080);

  if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
    perror("bind() failed");
    close(listener);
    return 1;
  }

  if (listen(listener, 5)) {
    perror("listen() failed");
    close(listener);
    return 1;
  }

  // Server is now listening for incoming connections
  printf("Server is listening on port 8080...\n");

  client_info_t clients[MAX_CLIENTS];
  int nclients = 0;
  char buf[256];

  while (1) {
    int client_fd = accept(listener, NULL, NULL);
    if (client_fd != -1) {
      if (nclients < MAX_CLIENTS) {
        ul = 1;
        ioctl(client_fd, FIONBIO, &ul);

        clients[nclients].fd = client_fd;
        clients[nclients].state = WAIT_NAME;
        memset(&clients[nclients].info, 0, sizeof(student_info_t));

        nclients++;
        send_msg(client_fd, "Ho Ten: ");
      } else {
        printf("Full Slots");
        close(client_fd);
      }
    }

    for (int i = 0; i < nclients; ++i) {
      int len = recv(clients[i].fd, buf, sizeof(buf) - 1, 0);
      if (len == -1) {
        if (errno != EWOULDBLOCK) {
        }
        continue;
      }

      if (len == 0) {
        close(clients[i].fd);
        clients[i] = clients[nclients - 1];
        nclients--;
        i--;
        continue;
      }

      buf[len] = '\0';

      buf[strcspn(buf, "\r\n")] = '\0';

      switch (clients[i].state) {
      case WAIT_NAME:
        strncpy(clients[i].info.name, buf, MAX_NAME - 1);
        send_msg(clients[i].fd, "MSSV: ");
        clients[i].state = WAIT_MSSV;
        break;
      case WAIT_MSSV:
        strncpy(clients[i].info.mssv, buf, MAX_MSSV - 1);
        char *email = generate_hust_email(&clients[i].info);
        send_msg(clients[i].fd, email);
        free(email);
        clients[i].state = DONE;
        break;
      case DONE:
        break;
      }
    }
  }

  return 0;
}
