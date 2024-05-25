#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "message.h"

#define CLIENT_BUFFER 1024

void* get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    struct sockaddr_in* res = (struct sockaddr_in*) sa;
    return &res->sin_addr;
  }

  struct sockaddr_in6* res = (struct sockaddr_in6*) sa;
  return &res->sin6_addr;
}

int get_listener_socket(char* port) {
  struct addrinfo hints, *res;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status;
  if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
    fprintf(stderr, "error getaddrinfo: %s\n", gai_strerror(status));
    return -1;
  }

  int sockfd;
  struct addrinfo* p;
  for (p = res; p != NULL; p = p->ai_next) {
    char res_string[INET6_ADDRSTRLEN];
    inet_ntop(p->ai_family, get_in_addr(p->ai_addr), res_string, sizeof res_string);
    printf("addr: %s\n", res_string);
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      fprintf(stderr, "error creating socket: %d\n", errno);
      continue;
    }

    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      fprintf(stderr, "error setsockopt %d\n", errno);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      fprintf(stderr, "error bind: %d\n", errno);
      continue;
    }
    break;
  }

  freeaddrinfo(res);
  if (p == NULL) {
    fprintf(stderr, "error failed to create socket\n");
    return -1;
  }

  if (listen(sockfd, 10) == -1) {
    fprintf(stderr, "error listen: %d\n", errno);
    close(sockfd);
    return -1;
  }

  return sockfd;
}

struct pollfd* init_fd(size_t fd_count, int sockfd) {
  struct pollfd* fds = malloc(fd_count * sizeof(struct pollfd));
  fds[0].fd = sockfd;
  fds[0].events = POLLIN;

  return fds;
}

void add_fd(struct pollfd** fds, size_t* fd_count, size_t* fd_size, int fd) {
  // need to resize 
  if (*fd_count == *fd_size) {
    *fd_size *= 2;
    *fds = realloc(*fds, *fd_size * sizeof(struct pollfd));
  }

  (*fds)[*fd_count].fd = fd;
  (*fds)[*fd_count].events = POLLIN;

  (*fd_count)++;
}

void remove_fd(struct pollfd* fds, size_t* fd_count, int idx) {
  fds[idx] = fds[--(*fd_count)];
}

void handle_listener(int sockfd, struct pollfd** fds, size_t* fd_count, size_t* fd_size) {
  struct sockaddr addr;
  int new_fd;
  socklen_t size = sizeof(addr);
  if ((new_fd = accept(sockfd, &addr, &size)) == -1) {
    fprintf(stderr, "error accepting new connection %d\n", errno);
    return;
  }

  add_fd(fds, fd_count, fd_size, new_fd);
}

void close_fd(struct pollfd* fds, size_t* fd_count, int i) {
  close(fds[i].fd);
  remove_fd(fds, fd_count, i);
}

int handle_connection(int sockfd, int i, struct pollfd* fds, size_t* fd_count) {
  char buffer[CLIENT_BUFFER];
  memset(buffer, 0, CLIENT_BUFFER);
  int bytes_recv = recv(fds[i].fd, buffer, CLIENT_BUFFER, 0);

  switch (bytes_recv) {
    // No bytes received, client has closed the connection
    case 0: {
      printf("client closed (1)\n");
      return 1;
    }
    // Error
    case -1: {
      fprintf(stderr, "error recv %d\n", errno);
      break;
    }
    // Data successfully sent by client
    default: {
      Message* msg = message_unmarshal(buffer);
      printf("Received from: [%s], data: [%s]\n", msg->name, msg->data);
      if (strncmp(msg->data, "exit", 4) == 0 || strncmp(msg->data, "quit", 4) == 0) {
        if (send(fds[i].fd, "Goodbye!", 8, 0) == -1) {
          fprintf(stderr, "send error: %d\n", errno);
        }
        printf("client closed (2)\n");
        return 1;
      } 

      printf("Sending to all clients: %s\n", buffer);
      // Message* forwarded_message = message_create(7, strlen)
      // send to all other peeps
      for (int j = 0; j < *fd_count; j++) {
        if (fds[j].fd != sockfd && fds[j].fd != fds[i].fd) {
          if (send(fds[j].fd, buffer, strlen(buffer), 0) == -1) {
            fprintf(stderr, "send error: %d\n", errno);
          }
        }
      }
      break;
    }
  }
  return 0;
}

void remove_fds(int* to_remove, int cnt, struct pollfd* fds, size_t* fd_count) {
  for (int i = 0; i < cnt; i++) {
    close_fd(fds, fd_count, to_remove[i]);
  }
}

int run(int sockfd, struct pollfd* fds, size_t fd_count, size_t fd_size) {
  while (1) {
    printf("Polling...\n");
    int cnt = poll(fds, fd_count, -1);
    if (cnt == -1) {
      fprintf(stderr, "error poll %d\n", errno);
      return -1;
    }
    printf("Polled: %d\n", cnt);
    int to_remove[fd_count];
    int idx = 0;

    int cur_size = fd_count;
    // loop through all the fd and check if they are ready
    for (int i = 0; i < cur_size; i++) {
      printf("Looping through sockets: %d, fd_count: %zu\n", i, fd_count);
      // check if fd is ready
      if (fds[i].revents & POLLIN) {
        printf("Found a pollable socket idx: %d, socket: %d out of %zu sockets\n", i, fds[i].fd, fd_count);
        if (fds[i].fd == sockfd) {
          handle_listener(sockfd, &fds, &fd_count, &fd_size);
        } else {
          int remove = handle_connection(sockfd, i, fds, &fd_count);
          if (remove == 1) {
            to_remove[idx++] = i;
          }
        }
      }
    }

    remove_fds(to_remove, idx, fds, &fd_count);
  }
  return 0;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: ./server [PORT]\n");
    return 1;
  }

  int sockfd = get_listener_socket(argv[1]);
  if (sockfd == -1) {
    fprintf(stderr, "error get_listener_socket\n");
    return 1;
  }

  // heap memory to allow for resizing 
  size_t fd_size = 5;
  size_t fd_count = 1;
  struct pollfd* fds = init_fd(fd_count, sockfd);

  printf("Listening for connections on port %s\n", argv[1]);

  if (run(sockfd, fds, fd_count, fd_size) == -1) {
    fprintf(stderr, "error with run\n");
  }

  close(sockfd);
  free(fds);
  return 0;
}