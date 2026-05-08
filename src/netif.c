#include <bsp/board_api.h>
#include <lwip/etharp.h>
#include <lwip/init.h>
#include <lwip/ip_addr.h>
#include <lwip/netif.h>
#include <pico/unique_id.h>
#include <tusb.h>

#include "netif.h"

#define INIT_IP4(a, b, c, d) {PP_HTONL(LWIP_MAKEU32(a, b, c, d))}

static struct netif netif_data;

static ip4_addr_t address = INIT_IP4(0, 0, 0, 0);
static ip4_addr_t gateway = INIT_IP4(0, 0, 0, 0);
static ip4_addr_t netmask = INIT_IP4(255, 255, 255, 0);

static char hostname[9 + 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES];

static err_t output(struct netif *netif, struct pbuf *packet,
                    const ip4_addr_t *address) {
  return etharp_output(netif, packet, address);
}

static err_t linkoutput(struct netif *netif, struct pbuf *packet) {
  for (;;) {
    if (!tud_ready())
      return ERR_USE;

    if (tud_network_can_xmit(packet->tot_len)) {
      tud_network_xmit(packet, 0);
      return ERR_OK;
    }

    tud_task();
  }
}

static err_t initialize_netif(struct netif *netif) {
  netif->mtu = CFG_TUD_NET_MTU;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_UP |
                 NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_LINK_UP;
  netif->state = NULL;
  netif->name[0] = 'E';
  netif->name[1] = 'X';
  netif->output = output;
  netif->linkoutput = linkoutput;
  return ERR_OK;
}

static struct netif *add_netif(pico_unique_board_id_t *board_id,
                               ip_addr_t *static_address) {
  memcpy(tud_network_mac_address, &board_id->id[2], 6);
  tud_network_mac_address[0] &= (uint8_t)~0x1; // unicast
  tud_network_mac_address[0] |= 0x2;           // locally administered

  netif_data.hwaddr_len = sizeof tud_network_mac_address;
  memcpy(netif_data.hwaddr, tud_network_mac_address, netif_data.hwaddr_len);
  netif_data.hwaddr[5] ^= 0x01;
  netif_data.hostname = hostname;

  ip_addr_set(&address, static_address);
  return netif_add(&netif_data, &address, &netmask, &gateway, NULL,
                   initialize_netif, netif_input);
}

static void initialize_board() {
  board_init();

  tusb_rhport_init_t usb_device = {.role = TUSB_ROLE_DEVICE,
                                   .speed = TUSB_SPEED_AUTO};
  tusb_init(BOARD_TUD_RHPORT, &usb_device);

  if (board_init_after_tusb)
    board_init_after_tusb();
}

static void set_hostname(pico_unique_board_id_t *board_id) {
  static const char *prefix = "picoRIO-";
  size_t length = strlen(prefix);
  strcpy(hostname, prefix);

  char *s = hostname + length;
  for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; ++i) {
    *s++ = "0123456789abcdef"[(board_id->id[i] >> 0) & 0x0f];
    *s++ = "0123456789abcdef"[(board_id->id[i] >> 4) & 0x0f];
  }
  *s = '\0';
}

struct netif *netif_init_from_ip_addr(ip_addr_t *address) {
  initialize_board();
  lwip_init();

  pico_unique_board_id_t board_id;
  pico_get_unique_board_id(&board_id);
  set_hostname(&board_id);

  struct netif *netif = add_netif(&board_id, address);
  while (!netif_is_up(netif))
    ;
  return netif;
}
