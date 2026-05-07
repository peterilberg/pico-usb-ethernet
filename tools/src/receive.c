/*
 * Receive a UDP packet from client.c.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int sock;
  socklen_t fromlen;
  short port;
  struct sockaddr_in name, from;
  char buf[1024];

  if (argc != 2) {
    fprintf(stderr, "Usage: server port\n");
    exit(1);
  }
  port = atoi(argv[1]);

  sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    perror("socket");
    exit(1);
  }

  memset(&name, 0, sizeof name);
  name.sin_family = AF_INET;
  name.sin_addr.s_addr = INADDR_ANY;
  name.sin_port = ntohs(port);
  if (bind(sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
    perror("bind");
    exit(1);
  }

  fromlen = sizeof(from);
  while (1) {
    memset(buf, 0, sizeof buf);
    if (recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&from,
                 &fromlen) < 0)
      perror("recvfrom");
    printf("from %s: %s\n", inet_ntoa(from.sin_addr), buf);
  }

  close(sock);

  exit(0);
}
