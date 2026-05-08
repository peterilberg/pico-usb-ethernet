#include <lwip/arch.h>
#include <lwip/dhcp.h>
#include <lwip/ip_addr.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <lwip/udp.h>
#include <pico/stdlib.h>
#include <pico/time.h>
#include <tusb.h>

#include "dhcp.h"
#include "netif.h"
#include "traffic.h"

#define PICO_ADDRESS "192.168.7.1"
#define PICO_PORTNUM 12345

#define HOST_ADDRESS "192.168.64.47"
#define HOST_PORTNUM 12345

struct host_info {
  struct udp_pcb *pcb;
  ip_addr_t *address;
  u16_t port;
};

struct health_info {
  struct host_info host;
  ip_addr_t *address;
  const char *name;
};

static void echo(void *arg, struct udp_pcb *pcb, struct pbuf *packet,
                 const ip_addr_t *client, u16_t port) {
  if (packet == NULL)
    return;

  udp_sendto(pcb, packet, client, port);
  pbuf_free(packet);
}

static void send_message(struct udp_pcb *pcb, ip_addr_t *address, u16_t port,
                         char *message) {
  size_t size = strlen(message) + 1;
  struct pbuf *packet = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RAM);
  if (packet == NULL)
    return;

  pbuf_take(packet, message, size);
  udp_sendto(pcb, packet, address, port);
  pbuf_free(packet);
}

static bool health_update(repeating_timer_t *timer) {
  struct health_info *info = (struct health_info *)timer->user_data;

  char packet[64];
  sprintf(packet, "%s at %d.%d.%d.%d", info->name, ip4_addr1(info->address),
          ip4_addr2(info->address), ip4_addr3(info->address),
          ip4_addr4(info->address));
  send_message(info->host.pcb, info->host.address, info->host.port, packet);
  return true;
}

static bool gateway_update(repeating_timer_t *timer) {
  dhcp_refresh_gateway(netif_default);
  return true;
}

int main(void) {
  ip_addr_t self, host;
  ipaddr_aton(PICO_ADDRESS, &self);
  ipaddr_aton(HOST_ADDRESS, &host);

  netif_set_default(netif_init_from_ip_addr(&self));
  dhcp_start_on_netif(netif_default, &self);

  struct udp_pcb *pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  if (pcb != NULL && udp_bind(pcb, IP_ANY_TYPE, PICO_PORTNUM) == ERR_OK)
    udp_recv(pcb, echo, NULL);

  struct health_info health_info = {
      .host.pcb = pcb,
      .host.address = &host,
      .host.port = HOST_PORTNUM,
      .address = &self,
      .name = (netif_default->hostname == NULL) ? "<unknown>"
                                                : netif_default->hostname,
  };

  repeating_timer_t gateway, timer;
  add_repeating_timer_ms(1000, gateway_update, NULL, &gateway);
  add_repeating_timer_ms(1000, health_update, &health_info, &timer);

  while (1)
    service_traffic();

  cancel_repeating_timer(&gateway);
  cancel_repeating_timer(&timer);

  udp_remove(pcb);
  dhcp_stop_on_netif(netif_default);
  return 0;
}
