#include <bsp/board_api.h>
#include <lwip/def.h>
#include <lwip/pbuf.h>
#include <lwip/timeouts.h>
#include <netif/ethernet.h>

#include "traffic.h"

uint8_t tud_network_mac_address[6] = {0x02, 0x02, 0x84, 0x6A, 0x96, 0x00};

static struct pbuf *received_frame;

void service_traffic(void) {
  tud_task();

  if (received_frame == NULL) {
    sys_check_timeouts();
    return;
  }

  if (ethernet_input(received_frame, netif_default) != ERR_OK) {
    pbuf_free(received_frame);
  }
  received_frame = NULL;

  tud_network_recv_renew();
  sys_check_timeouts();
}

void tud_network_init_cb(void) {
  if (received_frame) {
    pbuf_free(received_frame);
    received_frame = NULL;
  }
}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
  if (received_frame)
    return false;

  if (size == 0)
    return true;

  struct pbuf *frame = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
  if (frame != NULL) {
    memcpy(frame->payload, src, size);
    received_frame = frame;
  }
  return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
  struct pbuf *frame = (struct pbuf *)ref;
  return pbuf_copy_partial(frame, dst, frame->tot_len, 0);
}

sys_prot_t sys_arch_protect(void) { return 0; }
void sys_arch_unprotect(sys_prot_t pval) { (void)pval; }

uint32_t sys_now(void) { return board_millis(); }
