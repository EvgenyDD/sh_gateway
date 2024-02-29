#include "netconf.h"
#include "ethernetif.h"
#include "lwip/dhcp.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/tcp.h"
#include "lwip/tcp_impl.h"
#include "lwip/udp.h"
#include "lwipopts.h"
#include "netconf.h"
#include "netif/etharp.h"
#include "stm32f4x7_eth.h"
#include "stm32f4x7_eth_conf.h"

#define MAX_DHCP_TRIES 4
#define LINK_CHECK_INTERVAL 200 // ms

#define ETH_INIT_FLAG ETH_SUCCESS
#define ETH_LINK_FLAG 0x10

#define LAN8720_PHY_ADDRESS 0x00

enum
{
	DHCP_START = 1,
	DHCP_WAIT_ADDRESS,
	DHCP_ADDRESS_ASSIGNED,
	DHCP_TIMEOUT,
	DHCP_LINK_DOWN,
};

#define IP_ADDR0 192
#define IP_ADDR1 168
#define IP_ADDR2 0
#define IP_ADDR3 10

#define NETMASK_ADDR0 255
#define NETMASK_ADDR1 255
#define NETMASK_ADDR2 255
#define NETMASK_ADDR3 0

#define GW_ADDR0 192
#define GW_ADDR1 168
#define GW_ADDR2 0
#define GW_ADDR3 1

static struct
{
	uint8_t ip[4];
	uint8_t mask[4];
	uint8_t gw[4];
} addr = {{IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3},
		  {NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3},
		  {GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3}};

static struct netif gnetif;
static __IO uint32_t g_eth_status = 0;
static ETH_InitTypeDef ETH_InitStructure;

static uint32_t tmr_tcp = 0;
static uint32_t tmr_arp = 0;
static uint32_t IPaddress = 0;
static uint32_t tmr_link_check = 0;

#if LWIP_DHCP
static uint32_t tmr_dhcp_fine = 0;
static uint32_t tmr_dhcp_coarse = 0;
static __IO uint8_t DHCP_state;
#endif

#if LWIP_DHCP
static void LwIP_DHCP_Process_Handle(void)
{
	struct ip_addr ipaddr;
	struct ip_addr netmask;
	struct ip_addr gw;
	uint8_t iptab[4];
	uint8_t iptxt[20];

	switch(DHCP_state)
	{
	case DHCP_START:
	{
		DHCP_state = DHCP_WAIT_ADDRESS;
		dhcp_start(&gnetif);
		IPaddress = 0; // IP address should be set to 0 every time we want to assign a new DHCP address
	}
	break;

	case DHCP_WAIT_ADDRESS:
	{
		IPaddress = gnetif.ip_addr.addr; // Read the new IP address

		if(IPaddress != 0)
		{
			DHCP_state = DHCP_ADDRESS_ASSIGNED;
			dhcp_stop(&gnetif);
		}
		else
		{
			if(gnetif.dhcp->tries > MAX_DHCP_TRIES) // DHCP timeout
			{
				DHCP_state = DHCP_TIMEOUT;
				dhcp_stop(&gnetif);

				/* Static address used */
				IP4_ADDR(&ipaddr, addr.ip[0], addr.ip[1], addr.ip[2], addr.ip[3]);
				IP4_ADDR(&netmask, addr.mask[0], addr.mask[1], addr.mask[2], addr.mask[3]);
				IP4_ADDR(&gw, addr.gw[0], addr.gw[1], addr.gw[2], addr.gw[3]);
				netif_set_addr(&gnetif, &ipaddr, &netmask, &gw);
			}
		}
	}
	break;
	default: break;
	}
}
#endif

static void ETH_Delay(__IO uint32_t nCount)
{
	for(__IO uint32_t index = nCount; index != 0; index--)
		;
}

static void eth_board_config(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOE, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_RMII);

	/*	 ETH_MDIO -------------------------> PA2
		 ETH_MDC --------------------------> PC1
		 ETH_MII_RX_CLK/ETH_RMII_REF_CLK---> PA1
		 ETH_MII_RX_DV/ETH_RMII_CRS_DV ----> PA7
		 ETH_MII_RXD0/ETH_RMII_RXD0 -------> PC4
		 ETH_MII_RXD1/ETH_RMII_RXD1 -------> PC5
		 ETH_MII_TX_EN/ETH_RMII_TX_EN -----> PB11
		 ETH_MII_TXD0/ETH_RMII_TXD0 -------> PB12
		 ETH_MII_TXD1/ETH_RMII_TXD1 -------> PB13
		 ETH_RST                    -------> PE13 */

	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_ETH);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource11, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource12, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_ETH);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource1, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource4, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource5, GPIO_AF_ETH);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	// PHY hardware reset
	GPIO_ResetBits(GPIOE, GPIO_Pin_13);
	for(volatile uint32_t i = 0; i < 20000; i++)
		;
	GPIO_SetBits(GPIOE, GPIO_Pin_13);
	for(volatile uint32_t i = 0; i < 20000; i++)
		;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_ETH_MAC | RCC_AHB1Periph_ETH_MAC_Tx | RCC_AHB1Periph_ETH_MAC_Rx, ENABLE);

	ETH_DeInit(); // Reset ETHERNET on AHB Bus

	ETH_SoftwareReset();

	while(ETH_GetSoftwareResetStatus() == SET)
		;

	ETH_StructInit(&ETH_InitStructure);

	// MAC
	ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Enable;
	//  ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Disable;
	//  ETH_InitStructure.ETH_Speed = ETH_Speed_10M;
	//  ETH_InitStructure.ETH_Mode = ETH_Mode_FullDuplex;

	ETH_InitStructure.ETH_LoopbackMode = ETH_LoopbackMode_Disable;
	ETH_InitStructure.ETH_RetryTransmission = ETH_RetryTransmission_Disable;
	ETH_InitStructure.ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable;
	ETH_InitStructure.ETH_ReceiveAll = ETH_ReceiveAll_Disable;
	ETH_InitStructure.ETH_BroadcastFramesReception = ETH_BroadcastFramesReception_Enable;
	ETH_InitStructure.ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;
	ETH_InitStructure.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;
	ETH_InitStructure.ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;
#ifdef CHECKSUM_BY_HARDWARE
	ETH_InitStructure.ETH_ChecksumOffload = ETH_ChecksumOffload_Enable;
#endif

	// DMA
	/* When we use the Checksum offload feature, we need to enable the Store and Forward mode:
	the store and forward guarantee that a whole frame is stored in the FIFO, so the MAC can insert/verify the checksum,
	if the checksum is OK the DMA can handle the frame otherwise the frame is dropped */
	ETH_InitStructure.ETH_DropTCPIPChecksumErrorFrame = ETH_DropTCPIPChecksumErrorFrame_Enable;
	ETH_InitStructure.ETH_ReceiveStoreForward = ETH_ReceiveStoreForward_Enable;
	ETH_InitStructure.ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;

	ETH_InitStructure.ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Disable;
	ETH_InitStructure.ETH_ForwardUndersizedGoodFrames = ETH_ForwardUndersizedGoodFrames_Disable;
	ETH_InitStructure.ETH_SecondFrameOperate = ETH_SecondFrameOperate_Enable;
	ETH_InitStructure.ETH_AddressAlignedBeats = ETH_AddressAlignedBeats_Enable;
	ETH_InitStructure.ETH_FixedBurst = ETH_FixedBurst_Enable;
	ETH_InitStructure.ETH_RxDMABurstLength = ETH_RxDMABurstLength_32Beat;
	ETH_InitStructure.ETH_TxDMABurstLength = ETH_TxDMABurstLength_32Beat;
	ETH_InitStructure.ETH_DMAArbitration = ETH_DMAArbitration_RoundRobin_RxTx_2_1;

	g_eth_status = ETH_Init(&ETH_InitStructure, LAN8720_PHY_ADDRESS);

	if(LwIP_check_eth_link())
	{
		g_eth_status |= ETH_LINK_FLAG;
	}
}

static void cb_eth_link(struct netif *netif)
{
#ifndef LWIP_DHCP
	fuck
		uint8_t iptab[4] = {0};
	uint8_t iptxt[20];
#endif

	if(netif_is_link_up(netif))
	{
		if(ETH_InitStructure.ETH_AutoNegotiation != ETH_AutoNegotiation_Disable)
		{
			__IO uint32_t timeout = 0;
			ETH_WritePHYRegister(LAN8720_PHY_ADDRESS, PHY_BCR, PHY_AutoNegotiation);

			do
			{
				timeout++;
			} while(!(ETH_ReadPHYRegister(LAN8720_PHY_ADDRESS, PHY_BSR) & PHY_AutoNego_Complete) && (timeout < (uint32_t)PHY_READ_TO));

			timeout = 0;
			uint32_t reg_val = ETH_ReadPHYRegister(LAN8720_PHY_ADDRESS, PHY_SR);

			if((reg_val & PHY_DUPLEX_STATUS) != (uint32_t)RESET) // Configure the MAC with the Duplex Mode fixed by the auto-negotiation process
			{
				/* Set Ethernet duplex mode to Full-duplex following the auto-negotiation */
				ETH_InitStructure.ETH_Mode = ETH_Mode_FullDuplex;
			}
			else
			{
				/* Set Ethernet duplex mode to Half-duplex following the auto-negotiation */
				ETH_InitStructure.ETH_Mode = ETH_Mode_HalfDuplex;
			}

			if(reg_val & PHY_SPEED_STATUS) // Configure the MAC with the speed fixed by the auto-negotiation process
			{
				ETH_InitStructure.ETH_Speed = ETH_Speed_10M;
			}
			else
			{
				ETH_InitStructure.ETH_Speed = ETH_Speed_100M;
			}

			/*------------------------ ETHERNET MACCR Re-Configuration --------------------*/
			uint32_t tmpreg = ETH->MACCR;

			/* Set the FES bit according to ETH_Speed value */
			/* Set the DM bit according to ETH_Mode value */
			tmpreg |= (uint32_t)(ETH_InitStructure.ETH_Speed | ETH_InitStructure.ETH_Mode);

			ETH->MACCR = (uint32_t)tmpreg;

			_eth_delay_(ETH_REG_WRITE_DELAY);
			tmpreg = ETH->MACCR;
			ETH->MACCR = tmpreg;
		}

		ETH_Start(); // Restart MAC interface

		struct ip_addr ipaddr;
		struct ip_addr netmask;
		struct ip_addr gw;

#if LWIP_DHCP
		ipaddr.addr = 0;
		netmask.addr = 0;
		gw.addr = 0;
		DHCP_state = DHCP_START;
#else
		IP4_ADDR(&ipaddr, addr.ip[0], addr.ip[1], addr.ip[2], addr.ip[3]);
		IP4_ADDR(&netmask, addr.mask[0], addr.mask[1], addr.mask[2], addr.mask[3]);
		IP4_ADDR(&gw, addr.gw[0], addr.gw[1], addr.gw[2], addr.gw[3]);
#endif

		netif_set_addr(&gnetif, &ipaddr, &netmask, &gw);

		netif_set_up(&gnetif);
	}
	else
	{
		ETH_Stop();
#if LWIP_DHCP
		DHCP_state = DHCP_LINK_DOWN;
		dhcp_stop(netif);
#endif
		netif_set_down(&gnetif);
	}
}

uint8_t *LwIP_cfg_ip(void) { return addr.ip; }
uint8_t *LwIP_cfg_gw(void) { return addr.gw; }
uint8_t *LwIP_cfg_mask(void) { return addr.mask; }

void LwIP_init(void)
{
	eth_board_config();

	struct ip_addr ipaddr;
	struct ip_addr netmask;
	struct ip_addr gw;
#ifndef LWIP_DHCP
	uint8_t iptab[4] = {0};
	uint8_t iptxt[20];
#endif

	mem_init();	 // Initializes the dynamic memory heap defined by MEM_SIZE
	memp_init(); // Initializes the memory pools defined by MEMP_NUM_x

#if LWIP_DHCP
	ipaddr.addr = 0;
	netmask.addr = 0;
	gw.addr = 0;
#else
	IP4_ADDR(&ipaddr, addr.ip[0], addr.ip[1], addr.ip[2], addr.ip[3]);
	IP4_ADDR(&netmask, addr.mask[0], addr.mask[1], addr.mask[2], addr.mask[3]);
	IP4_ADDR(&gw, addr.gw[0], addr.gw[1], addr.gw[2], addr.gw[3]);
#endif

	netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);
	netif_set_default(&gnetif);

	if(g_eth_status == (ETH_INIT_FLAG | ETH_LINK_FLAG))
	{
		gnetif.flags |= NETIF_FLAG_LINK_UP;
		netif_set_up(&gnetif);
#if LWIP_DHCP
		DHCP_state = DHCP_START;
#endif
	}
	else
	{
		netif_set_down(&gnetif);
#if LWIP_DHCP
		DHCP_state = DHCP_LINK_DOWN;
#endif
	}

	netif_set_link_callback(&gnetif, cb_eth_link);
}

void LwIP_packet_handle(void) { ethernetif_input(&gnetif); }

bool LwIP_check_eth_link(void)
{
	return ETH_ReadPHYRegister(LAN8720_PHY_ADDRESS, PHY_BSR) & PHY_Linked_Status;
}

void LwIP_periodic_handle(__IO uint32_t diff_ms)
{
	if(ETH_CheckFrameReceived()) LwIP_packet_handle();

	tmr_link_check += diff_ms;
	tmr_tcp += diff_ms;
	tmr_arp += diff_ms;

	if(tmr_link_check >= LINK_CHECK_INTERVAL)
	{
		tmr_link_check = 0;
		if(LwIP_check_eth_link())
		{
			g_eth_status |= ETH_LINK_FLAG;
			netif_set_link_up(&gnetif);
		}
		else
		{
			g_eth_status &= (uint32_t) ~(ETH_LINK_FLAG);
			netif_set_link_down(&gnetif);
		}
	}

	if(tmr_tcp >= TCP_TMR_INTERVAL) // TCP periodic process every 250 ms
	{
		tmr_tcp = 0;
		tcp_tmr();
	}

	if(tmr_arp >= ARP_TMR_INTERVAL) // ARP periodic process every 5s
	{
		tmr_arp = 0;
		etharp_tmr();
	}

#if LWIP_DHCP
	if(tmr_dhcp_fine >= DHCP_FINE_TIMER_MSECS) // Fine DHCP periodic process every 500ms
	{
		tmr_dhcp_fine = 0;
		dhcp_fine_tmr();
		if((DHCP_state != DHCP_ADDRESS_ASSIGNED) &&
		   (DHCP_state != DHCP_TIMEOUT) &&
		   (DHCP_state != DHCP_LINK_DOWN))
		{
			LwIP_DHCP_Process_Handle();
		}
	}

	if(tmr_dhcp_coarse >= DHCP_COARSE_TIMER_MSECS) // DHCP Coarse periodic process every 60s
	{
		tmr_dhcp_coarse = 0;
		dhcp_coarse_tmr();
	}
#endif
}

bool LwIP_is_link_up(void) { return g_eth_status & ETH_LINK_FLAG; }