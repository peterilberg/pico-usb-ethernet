#include "bsp/board_api.h"
#include "tusb.h"

#include "dhserver.h"
#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/udp.h"
#include <lwip/def.h>
#include <lwip/dhcp.h>
#include <lwip/dns.h>
#include <lwip/ip4_addr.h>
#include <lwip/ip_addr.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <lwip/prot/udp.h>
#include <netif/ethernet.h>
#include <pico/time.h>
#include <pico/unique_id.h>

#ifdef INCLUDE_IPERF
#include "lwip/apps/lwiperf.h"
#endif

#define INIT_IP4(a, b, c, d) {PP_HTONL(LWIP_MAKEU32(a, b, c, d))}

/* lwip context */
static struct netif netif_data;

/* shared between tud_network_recv_cb() and service_traffic() */
static struct pbuf *received_frame;

/* this is used by this code, ./class/net/net_driver.c, and usb_descriptors.c */
/* ideally speaking, this should be generated from the hardware's unique ID (if
 * available) */
/* it is suggested that the first byte is 0x02 to indicate a link-local address
 */
uint8_t tud_network_mac_address[6] = {0x02, 0x02, 0x84, 0x6A, 0x96, 0x00};

/* network parameters of this MCU */
static const ip4_addr_t ipaddr = INIT_IP4(192, 168, 7, 1);
static const ip4_addr_t netmask = INIT_IP4(255, 255, 255, 0);
static const ip4_addr_t gateway = INIT_IP4(0, 0, 0, 0);

/* database IP addresses that can be offered to the host; this must be in RAM to
 * store assigned MAC addresses */
static dhcp_entry_t entries[] = {
    /* mac ip address               lease time */
    {{0}, INIT_IP4(192, 168, 7, 2), 24 * 60 * 60},
    {{0}, INIT_IP4(192, 168, 7, 3), 24 * 60 * 60},
    {{0}, INIT_IP4(192, 168, 7, 4), 24 * 60 * 60},
};

static bool is_vacant(dhcp_entry_t *entry) {
  return memcmp("\0\0\0\0\0", entry->mac, 6) == 0;
}

static const dhcp_config_t dhcp_config = {
    .router = INIT_IP4(0, 0, 0, 0),  /* router address (if any) */
    .port = 67,                      /* listen port */
    .dns = INIT_IP4(192, 168, 7, 1), /* dns server (if any) */
    "usb",                           /* dns suffix */
    TU_ARRAY_SIZE(entries),          /* num entry */
    entries                          /* entries */
};

static ip4_addr_t *find_gateway() {
  for (int i = 0; i < dhcp_config.num_entry; ++i) {
    if (!is_vacant(&dhcp_config.entries[i])) {
      return &dhcp_config.entries[i].addr;
    }
  }
  return NULL;
}

static err_t linkoutput_fn(struct netif *netif, struct pbuf *p) {
  (void)netif;

  for (;;) {
    /* if TinyUSB isn't ready, we must signal back to lwip that there is nothing
     * we can do */
    if (!tud_ready())
      return ERR_USE;

    /* if the network driver can accept another packet, we make it happen */
    if (tud_network_can_xmit(p->tot_len)) {
      tud_network_xmit(p, 0 /* unused for this example */);
      return ERR_OK;
    }

    /* transfer execution to TinyUSB in the hopes that it will finish
     * transmitting the prior packet */
    tud_task();
  }
}

static err_t ip4_output_fn(struct netif *netif, struct pbuf *p,
                           const ip4_addr_t *addr) {
  return etharp_output(netif, p, addr);
}

#if LWIP_IPV6
static err_t ip6_output_fn(struct netif *netif, struct pbuf *p,
                           const ip6_addr_t *addr) {
  return ethip6_output(netif, p, addr);
}
#endif

static err_t netif_init_cb(struct netif *netif) {
  LWIP_ASSERT("netif != NULL", (netif != NULL));
  netif->mtu = CFG_TUD_NET_MTU;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP |
                 NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_UP |
                 NETIF_FLAG_LINK_UP;
  netif->state = NULL;
  netif->name[0] = 'E';
  netif->name[1] = 'X';
  netif->linkoutput = linkoutput_fn;
#if LWIP_IPV4
  netif->output = ip4_output_fn;
#endif
#if LWIP_IPV6
  netif->output_ip6 = ip6_output_fn;
#endif
#if LWIP_IGMP
  // netif_igmp_mac_filter() is not implemented
  // no mac filter options on tinyusb network driver(s), perhaps not needed if
  // promiscuous (given connection type)?

  // netif_set_igmp_mac_filter(netif, netif_igmp_mac_filter);
#endif
  return ERR_OK;
}

static struct dhcp netif_dhcp;
static char hostname[9 + 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES];

static void init_lwip(void) {
  struct netif *netif = &netif_data;

  lwip_init();

  pico_unique_board_id_t board_id;
  pico_get_unique_board_id(&board_id);
  memcpy(tud_network_mac_address, &board_id.id[2], 6);

  tud_network_mac_address[0] &= (uint8_t)~0x1; // unicast
  tud_network_mac_address[0] |= 0x2;           // locally administered
  /* the lwip virtual MAC address must be different from the host's; to ensure
   * this, we toggle the LSbit */
  netif->hwaddr_len = sizeof(tud_network_mac_address);
  memcpy(netif->hwaddr, tud_network_mac_address,
         sizeof(tud_network_mac_address));
  netif->hwaddr[5] ^= 0x01;
  netif->hostname = "hello";

  netif = netif_add(netif, &ipaddr, &netmask, &gateway, NULL, netif_init_cb,
                    netif_input /*ethernet_input ip_input*/);
#if LWIP_IPV6
  netif_create_ip6_linklocal_address(netif, 1);
#endif
  netif_set_default(netif);
}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
  /* this shouldn't happen, but if we get another packet before
  parsing the previous, we must signal our inability to accept it */
  if (received_frame)
    return false;

  if (size) {
    struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);

    if (p) {
      /* pbuf_alloc() has already initialized struct; all we need to do is copy
       * the data */
      memcpy(p->payload, src, size);

      /* store away the pointer for service_traffic() to later handle */
      received_frame = p;
    }
  }

  return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
  struct pbuf *p = (struct pbuf *)ref;

  (void)arg; /* unused for this example */

  return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

static void service_traffic(void) {
  /* handle any packet received by tud_network_recv_cb() */
  if (received_frame) {
    // Surrender ownership of our pbuf unless there was an error
    // Only call pbuf_free if not Ok else it will panic with "pbuf_free: p->ref
    // > 0" or steal it from whatever took ownership of it with undefined
    // consequences. See: https://savannah.nongnu.org/patch/index.php?10121
    if (ethernet_input(received_frame, &netif_data) != ERR_OK) {
      pbuf_free(received_frame);
    }
    received_frame = NULL;
    tud_network_recv_renew();
  }

  sys_check_timeouts();
}

void tud_network_init_cb(void) {
  /* if the network is re-initializing and we have a leftover packet, we must do
   * a cleanup */
  if (received_frame) {
    pbuf_free(received_frame);
    received_frame = NULL;
  }
}

static struct udp_pcb *udpecho_raw_pcb;

static void udpecho_raw_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                             const ip_addr_t *addr, u16_t port) {
  LWIP_UNUSED_ARG(arg);
  if (p != NULL) {
    /* send received packet back to sender */
    // udp_sendto(upcb, p, addr, port);
    /* free the pbuf */
    pbuf_free(p);

    struct pbuf *out =
        pbuf_alloc(PBUF_TRANSPORT + UDP_HLEN + 100, 32, PBUF_RAM);
    pbuf_take(out, "received", strlen("received") + 1);
    udp_sendto(upcb, out, addr, port);
    pbuf_free(out);
  }
}

#include "hardware/gpio.h"

#define LED_DELAY_MS 1

#ifndef PICO_DEFAULT_LED_PIN
#warning blink_simple example requires a board with a regular LED
#endif

// Initialize the GPIO for the LED
void pico_led_init(void) {
#ifdef PICO_DEFAULT_LED_PIN
  // A device like Pico that uses a GPIO for the LED will define
  // PICO_DEFAULT_LED_PIN so we can use normal GPIO functionality to turn the
  // led on and off
  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
}

// Turn the LED on or off
void pico_set_led(bool led_on) {
#if defined(PICO_DEFAULT_LED_PIN)
  // Just set the GPIO on or off
  gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#endif
}

int main(void) {
  pico_led_init();

  /* initialize TinyUSB */
  board_init();

  // init device stack on configured roothub port
  tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE,
                                 .speed = TUSB_SPEED_AUTO};
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  pico_unique_board_id_t board_id;
  pico_get_unique_board_id(&board_id);
#define PREFIX "picoRIO-"
  size_t len = strlen(PREFIX);
  strcpy(hostname, PREFIX);
  char *s = hostname + len;
  for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; ++i) {
    *s++ = "0123456789abcdef"[(board_id.id[i] >> 0) & 0x0f];
    *s++ = "0123456789abcdef"[(board_id.id[i] >> 4) & 0x0f];
  }
  *s = '\0';

  /* initialize lwip, dhcp-server, dns-server, and http */
  init_lwip();
  dhcp_set_struct(netif_default, &netif_dhcp);

  while (!netif_is_up(netif_default))
    ;
  while (dhserv_init(&dhcp_config) != ERR_OK)
    ;

  // dns_init();

  udpecho_raw_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  if (udpecho_raw_pcb != NULL) {
    err_t err;

    err = udp_bind(udpecho_raw_pcb, IP_ANY_TYPE, 12345);
    if (err == ERR_OK) {
      udp_recv(udpecho_raw_pcb, udpecho_raw_recv, NULL);
    } else {
      /* abort? output diagnostic? */
    }
  } else {
    /* abort? output diagnostic? */
  }

  // udpecho_raw_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  ip_addr_t host_addr;
  ipaddr_aton("192.168.64.47", &host_addr);

#ifdef INCLUDE_IPERF
  // test with: iperf -c 192.168.7.1 -e -i 1 -M 5000 -l 8192 -r
  lwiperf_start_tcp_server_default(NULL, NULL);
#endif
  int i = 0;
  while (1) {
    tud_task();
    service_traffic();

    if (++i == 1000000) {
      i = 0;
      const char *msg =
          "hello                                                          ";
      struct pbuf *m1 = pbuf_alloc(PBUF_TRANSPORT, strlen(msg) + 1, PBUF_RAM);
      pbuf_take(m1, msg, strlen(msg) + 1);
      char buffer[16];
      sprintf(buffer, "%d.%d.%d.%d", ((netif_default->gw.addr >> 0) & 0xff),
              ((netif_default->gw.addr >> 8) & 0xff),
              ((netif_default->gw.addr >> 16) & 0xff),
              ((netif_default->gw.addr >> 24) & 0xff));
      pbuf_take_at(m1, buffer, strlen(buffer), 35);
      udp_sendto(udpecho_raw_pcb, m1, &host_addr, 12345);
      pbuf_free(m1);

      ip4_addr_t *gw = find_gateway();
      if (gw == NULL) {
        pico_set_led(true);
      } else {
        pico_set_led(false);
      }
      netif_set_gw(netif_default, gw);
    }
  }

  udp_remove(udpecho_raw_pcb);
  dhcp_stop(netif_default);
  dhcp_remove_struct(netif_default);
  return 0;
}

/* lwip has provision for using a mutex, when applicable */
sys_prot_t sys_arch_protect(void) { return 0; }
void sys_arch_unprotect(sys_prot_t pval) { (void)pval; }

/* lwip needs a millisecond time source, and the TinyUSB board support code has
 * one available */
uint32_t sys_now(void) { return board_millis(); }
