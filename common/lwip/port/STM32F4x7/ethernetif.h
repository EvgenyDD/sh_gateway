#ifndef __ETHERNETIF_H__
#define __ETHERNETIF_H__

#include "lwip/err.h"
#include "lwip/netif.h"
#include <stdint.h>

err_t ethernetif_init(struct netif *netif);
err_t ethernetif_input(struct netif *netif);
uint8_t *ethernetif_cfg_mac(void);

#endif // __ETHERNETIF_H__