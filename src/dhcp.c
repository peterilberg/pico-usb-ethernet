#include <dhserver.h>
#include <lwip/dhcp.h>
#include <tusb.h>

#include "dhcp.h"

static struct dhcp dhcp;
static dhcp_entry_t entries[4];

#define INIT_IP4(a, b, c, d) {PP_HTONL(LWIP_MAKEU32(a, b, c, d))}

static dhcp_config_t dhcp_config = {
    .router = INIT_IP4(0, 0, 0, 0), /* router address (if any) */
    .port = 67,                     /* listen port */
    .dns = INIT_IP4(0, 0, 0, 0),    /* dns server (if any) */
    "usb",                          /* dns suffix */
    TU_ARRAY_SIZE(entries),         /* num entry */
    entries                         /* entries */
};

static bool is_vacant(dhcp_entry_t *entry) {
  return memcmp("\0\0\0\0\0", entry->mac, 6) == 0;
}

static ip4_addr_t *get_gateway() {
  for (int i = 0; i < dhcp_config.num_entry; ++i) {
    if (!is_vacant(&dhcp_config.entries[i])) {
      return &dhcp_config.entries[i].addr;
    }
  }
  return NULL;
}

void dhcp_start_on_netif(struct netif *netif, ip_addr_t *address) {
  ip_addr_set(&dhcp_config.dns, address);

  u8_t a = ip4_addr1(address);
  u8_t b = ip4_addr2(address);
  u8_t c = ip4_addr3(address);
  u8_t d = ip4_addr4(address);

  for (int i = 0; i < TU_ARRAY_SIZE(entries); i++) {
    dhcp_entry_t *entry = &entries[i];
    IP4_ADDR(&entry->addr, a, b, c, d + i + 1);
    entry->lease = 24 * 60 * 60;
    memset(&entry->mac, 0, sizeof entry->mac);
  }

  dhcp_set_struct(netif, &dhcp);
  while (dhserv_init(&dhcp_config) != ERR_OK)
    ;
}

void dhcp_stop_on_netif(struct netif *netif) {
  dhcp_stop(netif);
  dhcp_remove_struct(netif);
}

void dhcp_refresh_gateway(struct netif *netif) {
  netif_set_gw(netif, get_gateway());
}
