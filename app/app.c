#include "CANopen.h"
#include "CO_driver_app.h"
#include "CO_driver_storage.h"
#include "emeter.h"
#include "OD.h"
#include "adc.h"
#include "can_driver.h"
#include "config_system.h"
#include "crc.h"
#include "eth/netconf.h"
#include "eth/tftp_server.h"
#include "eth_con/console_udp.h"
#include "ethernetif.h"
#include "fram.h"
#include "fw_header.h"
#include "load_switcher.h"
#include "lss_cb.h"
#include "pc.h"
#include "platform.h"
#include "prof.h"
#include "ret_mem.h"
#include "spi_common.h"

extern void pooool(void);

#define SYSTICK_IN_US (168000000 / 1000000)
#define SYSTICK_IN_MS (168000000 / 1000)

#define NMT_CONTROL                  \
	(CO_NMT_STARTUP_TO_OPERATIONAL | \
	 CO_NMT_ERR_ON_ERR_REG |         \
	 CO_NMT_ERR_ON_BUSOFF_HB |       \
	 CO_ERR_REG_GENERIC_ERR |        \
	 CO_ERR_REG_COMMUNICATION)

bool g_stay_in_boot = false;
uint8_t g_ip_address[4] = {192, 168, 0, 200};

CO_t *CO = NULL;
uint32_t g_uid[3];

static uint8_t active_can_node_id = 1;	/* Copied from CO_pending_can_node_id in the communication reset section */
static uint8_t pending_can_node_id = 1; /* read from dip switches or nonvolatile memory, configurable by LSS slave */
uint16_t pending_can_baud = 500;		/* read from dip switches or nonvolatile memory, configurable by LSS slave */

volatile uint64_t system_time = 0;

static int32_t prev_systick = 0;

static CO_NMT_reset_cmd_t reset;
static LSS_cb_obj_t lss_obj;

config_entry_t g_device_config[] = {
	{"can_id", sizeof(pending_can_node_id), 0, &pending_can_node_id},
	{"can_baud", sizeof(pending_can_baud), 0, &pending_can_baud},
	{"hb_prod_ms", sizeof(OD_PERSIST_COMM.x1017_producerHeartbeatTime), 0, &OD_PERSIST_COMM.x1017_producerHeartbeatTime},
	{"ip", sizeof(g_ip_address), 0, &g_ip_address},
};
const uint32_t g_device_config_count = sizeof(g_device_config) / sizeof(g_device_config[0]);

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

void delay_ms_w_can_poll(volatile uint32_t delay_ms)
{
	volatile uint32_t start = 0;
	int32_t mark_prev = 0;
	prof_mark(&mark_prev);
	const uint32_t time_limit = delay_ms * SYSTICK_IN_MS;
	for(;;)
	{
		start += (uint32_t)prof_mark(&mark_prev);
		CO_CANinterrupt(CO->CANmodule);
		if(start >= time_limit)
			return;
	}
}

static void end_loop(void)
{
	delay_ms(2000);
	platform_reset();
}

void main(void)
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
	ret_mem_set_load_src(LOAD_SRC_APP); // let preboot know it was booted from bootloader

	load_switcher_init();
	pc_init();
	adc_init();
	spi_common_init();
	fram_init();

	if(config_validate() == CONFIG_STS_OK) config_read_storage();

	uint32_t crc = crc32((const uint8_t *)g_uid, 12);
	ethernetif_cfg_mac()[3] = (crc >> 16) & 0xFF;
	ethernetif_cfg_mac()[4] = (crc >> 8) & 0xFF;
	ethernetif_cfg_mac()[5] = crc & 0xFF;
	LwIP_cfg_ip()[0] = g_ip_address[0];
	LwIP_cfg_ip()[1] = g_ip_address[1];
	LwIP_cfg_ip()[2] = g_ip_address[2];
	LwIP_cfg_ip()[3] = g_ip_address[3];

	LwIP_init();
	console_udp_init();
	tftpd_init();
	emeter_init();

	can_drv_init(CAN1);

	CO = CO_new(NULL, (uint32_t[]){0});
	CO_driver_storage_init(OD_ENTRY_H1010_storeParameters, OD_ENTRY_H1011_restoreDefaultParameters);
	co_od_init_headers();

	for(uint32_t i = 0; i < 127; i++)
	{
		OD_PERSIST_COMM.x1016_consumerHeartbeatTime[i] = ((i + 1) << 16) | 5000;
	}

	OD_PERSIST_COMM.x1018_identity.UID0 = g_uid[0];
	OD_PERSIST_COMM.x1018_identity.UID1 = g_uid[1];
	OD_PERSIST_COMM.x1018_identity.UID2 = g_uid[2];

	for(;;)
	{
		lss_obj.lss_br_set_delay_counter = 0;
		lss_obj.co = CO;
		CO->CANmodule->CANptr = CAN1;
		reset = CO_RESET_NOT;

		while(reset != CO_RESET_APP)
		{
			CO->CANmodule->CANnormal = false;

			CO_CANsetConfigurationMode(CO->CANmodule->CANptr);
			CO_CANmodule_disable(CO->CANmodule);
			if(CO_CANinit(CO, CO->CANmodule->CANptr, pending_can_baud) != CO_ERROR_NO) end_loop();

			CO_LSS_address_t lssAddress = {.identity = {.vendorID = OD_PERSIST_COMM.x1018_identity.serialNumber,
														.productCode = OD_PERSIST_COMM.x1018_identity.UID0,
														.revisionNumber = OD_PERSIST_COMM.x1018_identity.UID1,
														.serialNumber = OD_PERSIST_COMM.x1018_identity.UID2}};

			if(CO_LSSinit(CO, &lssAddress, &pending_can_node_id, &pending_can_baud) != CO_ERROR_NO) end_loop();
			lss_cb_init(&lss_obj);

			active_can_node_id = pending_can_node_id;
			uint32_t errInfo = 0;
			CO_ReturnError_t err = CO_CANopenInit(CO,		   /* CANopen object */
												  NULL,		   /* alternate NMT */
												  NULL,		   /* alternate em */
												  OD,		   /* Object dictionary */
												  NULL,		   /* Optional OD_statusBits */
												  NMT_CONTROL, /* CO_NMT_control_t */
												  500,		   /* firstHBTime_ms */
												  1000,		   /* SDOserverTimeoutTime_ms */
												  500,		   /* SDOclientTimeoutTime_ms */
												  false,	   /* SDOclientBlockTransfer */
												  active_can_node_id,
												  &errInfo);
			if(err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) end_loop();

			err = CO_CANopenInitPDO(CO, CO->em, OD, active_can_node_id, &errInfo);
			if(err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) end_loop();

			CO_EM_initCallbackRx(CO->em, co_emcy_rcv_cb);
			CO_CANsetNormalMode(CO->CANmodule);
			CO_driver_storage_error_report(CO->em);

			reset = CO_RESET_NOT;

			prof_mark(&prev_systick);

			while(reset == CO_RESET_NOT)
			{
				// time diff
				uint32_t time_diff_systick = (uint32_t)prof_mark(&prev_systick);

				static uint32_t remain_systick_us_prev = 0, remain_systick_ms_prev = 0;
				uint32_t diff_us = (time_diff_systick + remain_systick_us_prev) / (SYSTICK_IN_US);
				remain_systick_us_prev = (time_diff_systick + remain_systick_us_prev) % SYSTICK_IN_US;

				uint32_t diff_ms = (time_diff_systick + remain_systick_ms_prev) / (SYSTICK_IN_MS);
				remain_systick_ms_prev = (time_diff_systick + remain_systick_ms_prev) % SYSTICK_IN_MS;

				platform_watchdog_reset();

				CO_CANinterrupt(CO->CANmodule);
				reset = CO_process(CO, false, diff_us, NULL);
				lss_cb_poll(&lss_obj, diff_us);

				system_time += diff_ms;
				LwIP_periodic_handle(diff_ms);

				static uint32_t difff = 0;
				difff += diff_ms;
				if(difff > 1500) difff = 0;
				if(difff < (LwIP_is_link_up() ? 150 : 7))
				{
					GPIOE->ODR |= (1 << 4);
				}
				else
				{
					GPIOE->ODR &= ~(1 << 4);
				}
				adc_track();
				pc_poll(diff_ms);
				emeter_poll(diff_ms);
				if(load_switcher_poll(diff_ms))
				{
					_PRINTF("FAULT! Load Switcher!\n");
				}

				pooool();
			}
		}

		// PLATFORM_RESET:
		// 	CO_CANsetConfigurationMode(CO->CANmodule->CANptr);
		// 	CO_delete(CO);

		// 	platform_reset();
	}
}