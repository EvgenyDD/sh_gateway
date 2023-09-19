#ifndef __STM32F4x7_ETH_CONF_H
#define __STM32F4x7_ETH_CONF_H

#include "stm32f4xx.h"

/* Uncomment the line below when using time stamping and/or IPv4 checksum offload */
#define USE_ENHANCED_DMA_DESCRIPTORS

/* Uncomment the line below if you want to use user defined Delay function
   (for precise timing), otherwise default _eth_delay_ function defined within
   the Ethernet driver is used (less precise timing) */
// #define USE_Delay

#ifdef USE_Delay
#define _eth_delay_ Delay	  /* User can provide more timing precise _eth_delay_ function \
								in this example Systick is configured with an interrupt every 10 ms*/
#else
#define _eth_delay_ ETH_Delay /* Default _eth_delay_ function with less precise timing */
#endif

/*This define allow to customize configuration of the Ethernet driver buffers */
// #define CUSTOM_DRIVER_BUFFERS_CONFIG

#ifdef CUSTOM_DRIVER_BUFFERS_CONFIG
/* Redefinition of the Ethernet driver buffers size and count */
#define ETH_RX_BUF_SIZE ETH_MAX_PACKET_SIZE /* buffer size for receive */
#define ETH_TX_BUF_SIZE ETH_MAX_PACKET_SIZE /* buffer size for transmit */
#define ETH_RXBUFNB 4						/* 4 Rx buffers of size ETH_RX_BUF_SIZE */
#define ETH_TXBUFNB 4						/* 4 Tx buffers of size ETH_TX_BUF_SIZE */
#endif

/* PHY configuration section **************************************************/
#ifdef USE_Delay
/* PHY Reset delay */
#define PHY_RESET_DELAY ((uint32_t)0x000000FF)
/* PHY Configuration delay */
#define PHY_CONFIG_DELAY ((uint32_t)0x00000FFF)
/* Delay when writing to Ethernet registers*/
#define ETH_REG_WRITE_DELAY ((uint32_t)0x00000001)
#else
/* PHY Reset delay */
#define PHY_RESET_DELAY ((uint32_t)0x000FFFFF)
/* PHY Configuration delay */
#define PHY_CONFIG_DELAY ((uint32_t)0x00FFFFFF)
/* Delay when writing to Ethernet registers*/
#define ETH_REG_WRITE_DELAY ((uint32_t)0x0000FFFF)
#endif

/*******************  PHY Extended Registers section : ************************/

/* The LAN8720 PHY status register  */
#define PHY_SR ((uint16_t)0x1F)				 // PHY status register Offset
#define PHY_SPEED_STATUS ((uint16_t)0x0004)	 // PHY Speed mask
#define PHY_DUPLEX_STATUS ((uint16_t)0x0010) // PHY Duplex mask

//

#endif // __STM32F4x7_ETH_CONF_H