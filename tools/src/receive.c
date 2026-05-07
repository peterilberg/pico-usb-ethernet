#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s port\n", argv[0]);
    exit(1);
  }

  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    perror("socket");
    exit(1);
  }

  struct sockaddr_in name;
  name.sin_family = AF_INET;
  name.sin_addr.s_addr = INADDR_ANY;
  name.sin_port = ntohs(atoi(argv[1]));

  if (bind(sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
    perror("bind");
    exit(1);
  }

  while (1) {
    char buffer[1024];
    memset(buffer, 0, sizeof buffer);

    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    if (recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from,
                 &from_len) < 0)
      perror("recvfrom");

    printf("from %s: %s\n", inet_ntoa(from.sin_addr), buffer);
  }

  close(sock);
  return 0;
}
