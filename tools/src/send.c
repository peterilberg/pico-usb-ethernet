#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define DATA "The sea is calm tonight, the tide is full . . ."

int main(int argc, char *argv[]) {
  int sock;
  struct sockaddr_in name;
  struct hostent *hp;

  if (argc != 3) {
    fprintf(stderr, "Usage: client host port\n");
    exit(1);
  }

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket");
    exit(1);
  }

  hp = gethostbyname(argv[1]);
  if (hp == 0) {
    fprintf(stderr, "%s: unknown host\n", argv[1]);
    exit(1);
  }
  memcpy(&name.sin_addr, hp->h_addr, hp->h_length);
  name.sin_family = AF_INET;
  name.sin_port = htons(atoi(argv[2]));
  if (connect(sock, (struct sockaddr *)&name, sizeof name) < 0)
    perror("connect");

  if (send(sock, DATA, sizeof(DATA), 0) < 0)
    perror("sendto");

  char buffer[1024];
  if (recv(sock, buffer, sizeof buffer, 0) < 0)
    perror("receive");

  printf("received: %s", buffer);

  close(sock);
  exit(0);
}
