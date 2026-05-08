#include <lwip/ip_addr.h>

void dhcp_start_on_netif(struct netif *netif, ip_addr_t *address);
void dhcp_stop_on_netif(struct netif *netif);

void dhcp_refresh_gateway(struct netif *netif);
