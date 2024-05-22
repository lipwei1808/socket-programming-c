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
  if ((status = getaddrinfo("localhost", port, &hints, &res)) != 0) {
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

void remove_fd(struct pollfd** fds, size_t* fd_count, int idx) {
  (*fds)[idx] = (*fds)[--(*fd_count)];
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
  struct pollfd* fds = malloc(fd_size * sizeof(struct pollfd));
  fds[0].fd = sockfd;
  fds[0].events = POLLIN;

  printf("Listening for connections on port %s\n", argv[1]);

  while (1) {
    // printf("Polling...\n");
    int cnt = poll(fds, fd_count, -1);
    if (cnt == -1) {
      fprintf(stderr, "error poll %d\n", errno);
      return 1;
    }
    // printf("Got %d items to poll out of %zu sockets\n", cnt, fd_count);

    // loop through all the fd and check if they are ready
    for (int i = 0; i < fd_count; i++) {
      // check if fd is ready
      if (fds[i].revents & POLLIN) {
        printf("Found a pollable socket idx: %d, socket: %d out of %zu sockets\n", i, fds[i].fd, fd_count);
        // listener fd will accept connection
        if (fds[i].fd == sockfd) {
          struct sockaddr addr;
          int new_fd;
          socklen_t size = sizeof(addr);
          if ((new_fd = accept(sockfd, &addr, &size)) == -1) {
            fprintf(stderr, "error accepting new connection %d\n", errno);
            continue;
          }

          add_fd(&fds, &fd_count, &fd_size, new_fd);
        }
        // other connections 
        else {
          char buffer[CLIENT_BUFFER];
          memset(buffer, 0, CLIENT_BUFFER);
          int bytes_recv = recv(fds[i].fd, buffer, CLIENT_BUFFER, 0);
          switch (bytes_recv) {
            case 0: {
              printf("client closed (1)\n");
              close(fds[i].fd);
              remove_fd(&fds, &fd_count, i);
              break;
            }
            case -1: {
              fprintf(stderr, "error recv %d\n", errno);
              break;
            }
            default: {
              printf("Rceived: %s\n", buffer);
              if (strncmp(buffer, "exit", 4) == 0 || strncmp(buffer, "quit", 4) == 0) {
                if (send(fds[i].fd, "Goodbye!", 8, 0) == -1) {
                  fprintf(stderr, "send error: %d\n", errno);
                }
                remove_fd(&fds, &fd_count, i);
                close(fds[i].fd);
                printf("client closed (2)\n");
              } else {
                printf("Sending to all clients: %s\n", buffer);
                // send to all other peeps
                for (int j = 0; j < fd_count; j++) {
                  if (fds[j].fd != sockfd && fds[j].fd != fds[i].fd) {
                    if (send(fds[j].fd, buffer, strlen(buffer), 0) == -1) {
                      fprintf(stderr, "send error: %d\n", errno);
                    }
                  }
                }
              }
              break;
            }
          }
        }
      }
    }
  }

  close(sockfd);
  return 0;
}