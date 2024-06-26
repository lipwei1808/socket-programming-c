#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>  

#include "message.h"

int sd = 0;

void* get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    struct sockaddr_in* res = (struct sockaddr_in*) sa;
    return &res->sin_addr;
  }

  struct sockaddr_in6* res = (struct sockaddr_in6*) sa;
  return &res->sin6_addr;
}

int get_connection_socket(char* hostname, char* port) {
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int status;
  if ((status = getaddrinfo(hostname, port, &hints, &res)) != 0) {
    fprintf(stderr, "error getaddrinfo %s\n", gai_strerror(status));
    return -1;
  }

  int sockfd;
  struct addrinfo* p;
  for (p = res; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      fprintf(stderr, "error socket %d\n", errno);
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      fprintf(stderr, "error connecting %d\n", errno);
      continue;
    }
    break;
  }
  if (p == NULL) {
    return -1;
  }

  char ip[INET6_ADDRSTRLEN];
  inet_ntop(p->ai_family, get_in_addr(p->ai_addr), ip, sizeof(ip));
  printf("Connected to %s\n", ip);

  freeaddrinfo(res);
  return sockfd;
}

void run(int sockfd, char* name) {
  char buffer[1024];
  while (1) {
    printf("Enter: ");
    while (fgets(buffer, 1024, stdin) == NULL && !feof(stdin));
    buffer[strlen(buffer) - 1] = '\0';
    printf("You entered: %s\n", buffer);
    if (strncmp(buffer, "exit", 4) == 0 || strncmp(buffer, "quit", 4) == 0) {
      sd = 1;
    }

    char b[1024];
    Message* msg = message_create(strlen(name), strlen(buffer), name, buffer);
    message_marshal(msg, b);
    message_delete(msg);
    if (send(sockfd, b, 1024, 0) == -1) {
      fprintf(stderr, "error send %d\n", errno);
      continue;
    }
    if (sd == 1) break;
  }
}

void* runner(void* arg) {
  int* sockfd = (int*) arg;
  printf("Sockfd: %d\n", *sockfd);
  while (sd == 0) {
    char buffer[1024];
    memset(buffer, 0, 1024);
    int bytes;
    if ((bytes = recv(*sockfd, buffer, 1024, 0)) == -1) {
      fprintf(stderr, "recv error %d\n", errno);
      continue;
    }
    if (bytes == 0) {
      printf("Server stopped\n");
      break;
    }
    Message* msg = message_unmarshal(buffer);
    printf("[FROM %s]: %s\n", msg->name, msg->data);
    message_delete(msg);
  }
  printf("runner stopping...\n");
  write(STDIN_FILENO, "\n",0);
  return NULL;
}

int main(int argc, char* argv[]) {
  if (argc < 4) {
    fprintf(stderr, "usage: ./client [hostname] [port] [name]");
    return 1;
  }

  int sockfd = get_connection_socket(argv[1], argv[2]);
  if (sockfd == -1) {
      fprintf(stderr, "error failed to connect\n");
      return 1;
  }
  printf("Got sockfd: %d\n", sockfd);

  // listen to server for messages
  pthread_t t;
  pthread_create(&t, NULL, runner, (void*) &sockfd);
  
  // get user input
  run(sockfd, argv[3]);
  
  pthread_join(t, NULL);
  close(sockfd);
  return 0;
}