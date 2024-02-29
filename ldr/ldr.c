#include "adc.h"
#include "config_system.h"
#include "crc.h"
#include "eth/netconf.h"
#include "eth/tftp_server.h"
#include "eth_con/console_udp.h"
#include "ethernetif.h"
#include "fram.h"
#include "fw_header.h"
#include "platform.h"
#include "prof.h"
#include "ret_mem.h"
#include "spi_common.h"

#define BOOT_DELAY 5000

#define SYSTICK_IN_US (168000000 / 1000000)
#define SYSTICK_IN_MS (168000000 / 1000)

bool g_stay_in_boot = false;

volatile uint64_t system_time = 0;

static volatile uint32_t boot_delay = BOOT_DELAY;
static int32_t prev_systick = 0;

static uint8_t ip_addr[4] = {192, 168, 0, 200};

config_entry_t g_device_config[] = {
	{"ip", sizeof(ip_addr), 0, &ip_addr},
};
const uint32_t g_device_config_count = sizeof(g_device_config) / sizeof(g_device_config[0]);

uint32_t g_uid[3];

void delay_ms(volatile uint32_t delay_ms)
{
	volatile uint32_t start = 0;
	int32_t mark_prev = 0;
	prof_mark(&mark_prev);
	const uint32_t time_limit = delay_ms * SYSTICK_IN_MS;
	for(;;)
	{
		start += (uint32_t)prof_mark(&mark_prev);
		if(start >= time_limit)
			return;
	}
}

__attribute__((noreturn)) void main(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;

	platform_get_uid(g_uid);

	prof_init();
	platform_watchdog_init();

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA |
							   RCC_AHB1Periph_GPIOB |
							   RCC_AHB1Periph_GPIOC |
							   RCC_AHB1Periph_GPIOD |
							   RCC_AHB1Periph_GPIOE,
						   ENABLE);

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;

	GPIO_Init(GPIOE, &GPIO_InitStruct);

	fw_header_check_all();

	ret_mem_init();
	ret_mem_set_load_src(LOAD_SRC_BOOTLOADER); // let preboot know it was booted from bootloader

	adc_init();
	spi_common_init();
	fram_init();

	if(config_validate() == CONFIG_STS_OK) config_read_storage();

	uint32_t crc = crc32((const uint8_t *)g_uid, 12);
	ethernetif_cfg_mac()[2] = (crc >> 16) & 0xFF;
	ethernetif_cfg_mac()[3] = (crc >> 8) & 0xFF;
	ethernetif_cfg_mac()[4] = crc & 0xFF;
	LwIP_cfg_ip()[0] = ip_addr[0];
	LwIP_cfg_ip()[1] = ip_addr[1];
	LwIP_cfg_ip()[2] = ip_addr[2];
	LwIP_cfg_ip()[3] = ip_addr[3];

	LwIP_init();
	console_udp_init();
	tftpd_init();

	for(;;)
	{
		uint32_t time_diff_systick = (uint32_t)prof_mark(&prev_systick);

		static uint32_t remain_systick_us_prev = 0, remain_systick_ms_prev = 0;
		uint32_t diff_us = (time_diff_systick + remain_systick_us_prev) / (SYSTICK_IN_US);
		remain_systick_us_prev = (time_diff_systick + remain_systick_us_prev) % SYSTICK_IN_US;

		uint32_t diff_ms = (time_diff_systick + remain_systick_ms_prev) / (SYSTICK_IN_MS);
		remain_systick_ms_prev = (time_diff_systick + remain_systick_ms_prev) % SYSTICK_IN_MS;

		platform_watchdog_reset();

		if(!boot_delay &&
		   !g_stay_in_boot &&
		   g_fw_info[FW_APP].locked == false)
		{
			platform_reset();
		}

		system_time += diff_ms;

		boot_delay = boot_delay >= diff_ms ? boot_delay - diff_ms : 0;

		LwIP_periodic_handle(diff_ms);

		static uint32_t difff = 0;
		difff += diff_ms;
		if(difff > 100) difff = 0;
		if(difff < (LwIP_is_link_up() ? 60 : 7))
		{
			GPIOE->ODR |= (1 << 4);
		}
		else
		{
			GPIOE->ODR &= (uint32_t) ~(1 << 4);
		}

		adc_track();
	}
}

void lwip_assert(const char *s)
{
	// _PRINTF("LWIP ASS %s\n", s);
}