#include "lib.h"
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 9000
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

static void trim_newline(char *s) {
  size_t len = strlen(s);
  while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
    s[--len] = '\0';
  }
}

static bool start_with(const char *str, const char *prefix) {
  return strncmp(str, prefix, strlen(prefix)) == 0;
}

static void handle_sub(HashMapString *map, int fd, char *buffer) {
  char *topic = buffer + 4;
  while (*topic == ' ') {
    topic++;
  }
  trim_newline(topic);
  size_t len = strlen(topic);
  if (len == 0) {
    return;
  }
  add_subscriber(map, fd, topic, len);
  printf("[SUB] fd=%d topic=%s\n", fd, topic);
}

static void handle_unsub(HashMapString *map, int fd, char *buffer) {
  char *topic = buffer + 6;
  while (*topic == ' ') {
    topic++;
  }
  trim_newline(topic);
  size_t len = strlen(topic);
  if (len == 0) {
    return;
  }
  remove_subscriber(map, fd, topic, len);
  printf("[UNSUB] fd=%d topic=%s\n", fd, topic);
}

static void handle_pub(HashMapString *map, struct pollfd *poll_fds, int fd,
                       char *buffer) {
  trim_newline(buffer);
  char *ptr = buffer;
  if (strncmp(ptr, "PUB ", 4) != 0)
    return;
  ptr += 4;
  while (*ptr == ' ') {
    ptr++;
  }
  char *topic = ptr;
  while (*ptr != '\0' && *ptr != ' ') {
    ptr++;
  }
  if (*ptr == '\0')
    return;
  *ptr = '\0';
  ptr++;
  while (*ptr == ' ') {
    ptr++;
  }
  char *msg = ptr;
  if (*msg == '\0')
    return;
  TopicNode *node = find_topic(map, topic, strlen(topic));
  if (node == NULL) {
    return;
  }
  char send_buf[BUFFER_SIZE];
  snprintf(send_buf, BUFFER_SIZE, "[%s] %s\n", topic, msg);
  for (Subscriber *sub = node->subs; sub != NULL; sub = sub->next) {
    if (send(sub->fd, send_buf, strlen(send_buf), 0) <= 0) {
      int dead_fd = sub->fd;
      close(dead_fd);
      remove_client_from_all_topics(map, dead_fd);
      for (int i = 1; i <= MAX_CLIENTS; i++) {
        if (poll_fds[i].fd == dead_fd) {
          poll_fds[i].fd = -1;
          break;
        }
      }
      continue;
    }
  }
  printf("PUB fd=%d topic=%s msg=%s\n", fd, topic, msg);
}

int main() {
  signal(SIGPIPE, SIG_IGN);
  HashMapString map;
  init_hashmap(&map);
  int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == -1) {
    perror("socket");
    return 1;
  }

  int opt = 1;
  if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    perror("setsockopt");
    return 1;
  }
  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_port = htons(PORT),
                             .sin_addr.s_addr = INADDR_ANY};

  if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("bind");
    return 1;
  }
  if (listen(listener, 10) == -1) {
    perror("listen");
    return 1;
  }
  printf("Server is listening on port %d\n", PORT);
  struct pollfd poll_fds[MAX_CLIENTS + 1];
  poll_fds[0].fd = listener;
  poll_fds[0].events = POLLIN;
  poll_fds[0].revents = 0;
  for (int32_t i = 1; i <= MAX_CLIENTS; i++) {
    poll_fds[i].fd = -1;
    poll_fds[i].events = POLLIN;
    poll_fds[i].revents = 0;
  }
  while (1) {
    int32_t n = poll(poll_fds, MAX_CLIENTS + 1, -1);
    if (n == -1) {
      perror("poll");
      return 1;
    }

    if (poll_fds[0].revents & POLLIN) {
      int fd = accept(listener, NULL, NULL);
      if (fd >= 0) {
        int added = 0;
        for (int i = 1; i <= MAX_CLIENTS; i++) {
          if (poll_fds[i].fd == -1) {
            poll_fds[i].fd = fd;
            poll_fds[i].events = POLLIN;
            poll_fds[i].revents = 0;
            printf("New client connected fd=%d\n", fd);
            added = 1;
            break;
          }
        }
        if (!added) {
          const char *msg = "Server full\n";
          send(fd, msg, strlen(msg), 0);
          close(fd);
        }
      }
    }
    for (int i = 1; i <= MAX_CLIENTS; ++i) {
      if (poll_fds[i].fd == -1) {
        continue;
      }
      if (!(poll_fds[i].revents & POLLIN)) {
        continue;
      }

      char buffer[BUFFER_SIZE];
      ssize_t bytes_read = recv(poll_fds[i].fd, buffer, BUFFER_SIZE - 1, 0);
      if (bytes_read <= 0) {
        printf("Client disconnected fd=%d\n", poll_fds[i].fd);
        remove_client_from_all_topics(&map, poll_fds[i].fd);
        close(poll_fds[i].fd);
        poll_fds[i].fd = -1;
        continue;
      }
      buffer[bytes_read] = '\0';
      if (start_with(buffer, "SUB")) {
        handle_sub(&map, poll_fds[i].fd, buffer);
      } else if (start_with(buffer, "UNSUB")) {
        handle_unsub(&map, poll_fds[i].fd, buffer);
      } else if (start_with(buffer, "PUB")) {
        handle_pub(&map, poll_fds, poll_fds[i].fd, buffer);
      } else {
        const char *msg = "Unknown command\n";
        send(poll_fds[i].fd, msg, strlen(msg), 0);
      }
    }
  }
  close(listener);
  return 0;
}
