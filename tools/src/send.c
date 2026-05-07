#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MESSAGE                                                                \
  "When shall we three meet again In thunder, lightning, or in rain?"

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s host port\n", argv[0]);
    exit(1);
  }

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket");
    exit(1);
  }

  struct hostent *hp = gethostbyname(argv[1]);
  if (hp == NULL) {
    fprintf(stderr, "unknown host %s\n", argv[1]);
    exit(1);
  }

  struct sockaddr_in name;
  name.sin_family = AF_INET;
  name.sin_port = htons(atoi(argv[2]));
  size_t length = (hp->h_length < sizeof name.sin_addr) ? hp->h_length
                                                        : sizeof name.sin_addr;
  memcpy(&name.sin_addr, hp->h_addr, length);

  if (connect(sock, (struct sockaddr *)&name, sizeof name) < 0) {
    perror("connect");
    exit(1);
  }

  if (send(sock, MESSAGE, sizeof(MESSAGE), 0) < 0) {
    perror("sendto");
    exit(1);
  }

  char buffer[1024];
  if (recv(sock, buffer, sizeof buffer, 0) < 0) {
    perror("receive");
    exit(1);
  }

  printf("received: %s", buffer);

  close(sock);
  return 0;
}
