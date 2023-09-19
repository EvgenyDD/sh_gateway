#ifndef __NETCONF_H__
#define __NETCONF_H__

#include "stm32f4xx.h"
#include <stdbool.h>

uint8_t *LwIP_cfg_ip(void);
uint8_t *LwIP_cfg_gw(void);
uint8_t *LwIP_cfg_mask(void);
bool LwIP_check_eth_link(void);

void LwIP_init(void);
void LwIP_packet_handle(void);
void LwIP_periodic_handle(__IO uint32_t diff_ms);
bool LwIP_is_link_up(void);

#endif // __NETCONF_H__