#include "CANopen.h"
#include "CO_driver_app.h"
#include "adc.h"
#include "can_driver.h"
#include "config_system.h"
#include "emeter.h"
#include "eth/netconf.h"
#include "eth_con/console_udp.h"
#include "ethernetif.h"
#include "fram.h"
#include "fw_header.h"
#include "load_switcher.h"
#include "lss_helper.h"
#include "lwip/stats.h"
#include "pc.h"
#include "platform.h"
#include "rtc.h"
#include "sdo.h"
#include "sock_cli.h"
#include <ctype.h>
#include <string.h>

#undef BYTE_ORDER
#undef LITTLE_ENDIAN
#undef BIG_ENDIAN
#include <stdio.h>

extern int gsts;

extern uint32_t lwip_assert_counter;

extern CO_t *CO;
#include "console_can.h"

static void fw_cb(const char *arg, int l, int *ret)
{
	_PRINTF("Reset from: %s\n", paltform_reset_cause_get());
	fw_header_check_all();
	const char *p_build_ts = fw_fields_find_by_key_helper(&g_fw_info[FW_TYPE], "build_ts");
	_PRINTF("%s %s : %d.%d.%d : %s\n",
			g_fw_info[FW_APP].field_product_ptr,
			g_fw_info[FW_APP].field_product_name_ptr,
			g_fw_info[FW_APP].ver_major,
			g_fw_info[FW_APP].ver_minor,
			g_fw_info[FW_APP].ver_patch,
			p_build_ts ? p_build_ts : "");
	_PRINTF("PRELDR: %d\n", g_fw_info[FW_PRELDR].locked);
	_PRINTF("LDR   : %d\n", g_fw_info[FW_LDR].locked);
	_PRINTF("APP   : %d\n", g_fw_info[FW_APP].locked);
	_PRINTF("UID   : x%08x.x%08x.x%08x\n", g_uid[0], g_uid[1], g_uid[2]);
	_PRINTF("IP    : %d.%d.%d.%d\n", LwIP_cfg_ip()[0], LwIP_cfg_ip()[1], LwIP_cfg_ip()[2], LwIP_cfg_ip()[3]);
	_PRINTF("MAC   : x%x:x%x:x%x:x%x:x%x:x%x\n", ethernetif_cfg_mac()[0], ethernetif_cfg_mac()[1], ethernetif_cfg_mac()[2], ethernetif_cfg_mac()[3], ethernetif_cfg_mac()[4], ethernetif_cfg_mac()[5]);
}

static void reset_cb(const char *arg, int l, int *ret)
{
	platform_reset();
}

static void ip_cb(const char *arg, int l, int *ret)
{
	unsigned int a = 0, b = 0, c = 0, d = 0;
	int n = sscanf(arg, "%u.%u.%u.%u", &a, &b, &c, &d);

	while(isspace((int)*arg) && (*arg != 0))
	{
		arg++;
	}

	if(!*arg)
	{
		_PRINTF("Current IP: %d.%d.%d.%d\n", LwIP_cfg_ip()[0], LwIP_cfg_ip()[1], LwIP_cfg_ip()[2], LwIP_cfg_ip()[3]);
		return;
	}

	if((n != 4) || (a > 255) || (b > 255) || (c > 255) || (d > 255))
	{
		_PRINTF("Bad address\n");
		*ret = CON_CB_ERR_BAD_PARAM;
		return;
	}

	g_ip_address[0] = a;
	g_ip_address[1] = b;
	g_ip_address[2] = c;
	g_ip_address[3] = d;

	_PRINTF("OK, New IP: %d.%d.%d.%d\n", g_ip_address[0], g_ip_address[1], g_ip_address[2], g_ip_address[3]);
}

static void sock_mst_ip_cb(const char *arg, int l, int *ret)
{
	unsigned int a = 0, b = 0, c = 0, d = 0;
	int n = sscanf(arg, "%u.%u.%u.%u", &a, &b, &c, &d);

	while(isspace((int)*arg) && (*arg != 0))
	{
		arg++;
	}

	if(!*arg)
	{
		_PRINTF("Current MASTER slv IP: %d.%d.%d.%d\n", g_ip_addr_master[0], g_ip_addr_master[1], g_ip_addr_master[2], g_ip_addr_master[3]);
		return;
	}

	if((n != 4) || (a > 255) || (b > 255) || (c > 255) || (d > 255))
	{
		_PRINTF("Bad address\n");
		*ret = CON_CB_ERR_BAD_PARAM;
		return;
	}

	g_ip_addr_master[0] = a;
	g_ip_addr_master[1] = b;
	g_ip_addr_master[2] = c;
	g_ip_addr_master[3] = d;

	_PRINTF("OK, New IP: %d.%d.%d.%d\n", g_ip_addr_master[0], g_ip_addr_master[1], g_ip_addr_master[2], g_ip_addr_master[3]);
}

static void sock_gw_ip_cb(const char *arg, int l, int *ret)
{
	unsigned int ptr = 0, a = 0, b = 0, c = 0, d = 0;
	int n = sscanf(arg, "%u:%u.%u.%u.%u", &ptr, &a, &b, &c, &d);

	while(isspace((int)*arg) && (*arg != 0))
	{
		arg++;
	}

	if(!*arg)
	{
		_PRINTF("Current GW slv IP:\n");
		for(uint32_t i = 0; i < GW_COUNT; i++)
		{
			_PRINTF("\t%d.%d.%d.%d\n", g_ip_addr_gw[4 * i + 0], g_ip_addr_gw[4 * i + 1], g_ip_addr_gw[4 * i + 2], g_ip_addr_gw[4 * i + 3]);
		}
	}

	if(n != 5)
	{
		_PRINTF("Bad args\n");
		*ret = CON_CB_ERR_BAD_PARAM;
		return;
	}

	if(ptr >= GW_COUNT)
	{
		_PRINTF("Bad selector, max is %d\n", GW_COUNT);
		*ret = CON_CB_ERR_BAD_PARAM;
		return;
	}

	if((a > 255) || (b > 255) || (c > 255) || (d > 255))
	{
		_PRINTF("Bad address\n");
		*ret = CON_CB_ERR_BAD_PARAM;
		return;
	}

	g_ip_addr_gw[4 * ptr + 0] = a;
	g_ip_addr_gw[4 * ptr + 1] = b;
	g_ip_addr_gw[4 * ptr + 2] = c;
	g_ip_addr_gw[4 * ptr + 3] = d;

	_PRINTF("OK, New GW slv IP:\n");
	for(uint32_t i = 0; i < GW_COUNT; i++)
	{
		_PRINTF("\t%d.%d.%d.%d\n", g_ip_addr_gw[4 * i + 0], g_ip_addr_gw[4 * i + 1], g_ip_addr_gw[4 * i + 2], g_ip_addr_gw[4 * i + 3]);
	}
}

static void read_cb(const char *arg, int l, int *ret)
{
	int sts = config_validate();
	if(sts == 0) config_read_storage();
	_PRINTF("READ config status: %d\n", sts);

	for(uint32_t i = 0; i < sizeof(g_device_config_count); i++)
	{
		_PRINTF("%s (%d) @ x%x [", g_device_config[i].key, g_device_config[i].size, g_device_config[i].data_abs_address);
		for(uint32_t j = 0; j < g_device_config[i].size; j++)
		{
			_PRINTF("x%02x ", ((uint8_t *)g_device_config[i].data)[j]);
		}
		_PRINTF("]\n");
	}

	uint8_t buffer[256];
	memcpy(buffer, (void *)((uint32_t)&__cfg_start), sizeof(buffer));

	for(uint32_t i = 0; i < sizeof(buffer); i++)
	{
		_PRINTF("x%02x ", buffer[i]);
		if((i % 16) == 15) _PRINTF("\n");
	}
	// _PRINTF("\n");
}

static void save_cb(const char *arg, int l, int *ret)
{
	int sts = config_write_storage();
	if(sts != 0) _PRINTF("SAVE ERROR: %d\n", sts);
	if(sts == 0)
	{
		sts = config_validate();
		_PRINTF("SAVE config status: %d\n", sts);
	}
}

static void cfg_state_cb(const char *arg, int l, int *ret)
{
	int sts = config_validate();
	_PRINTF("Config status: %d\n", sts);
}

static void lon_cb(const char *arg, int l, int *ret)
{
	load_switcher_on();
	_PRINTF("on ok\n");
}

static void loff_cb(const char *arg, int l, int *ret)
{
	load_switcher_off();
	_PRINTF("off ok\n");
}

static void adc_cb(const char *arg, int l, int *ret)
{
	_PRINTF("VIN:   %.2f\n", adc_val.vin);
	_PRINTF("VOUT:  %.2f\n", adc_val.vout);
	_PRINTF("Is:    %.3f\n", adc_val.i_sns);
	_PRINTF("IR:    %.0f\n", adc_val.ir);
	_PRINTF("t MCU: %.0f\n", adc_val.t_mcu);
}

static void pc_cb(const char *arg, int l, int *ret)
{
	unsigned int v = 0;
	int n = sscanf(arg, "%u", &v);
	if(n == 1)
	{
		pc_switch();
		_PRINTF("Switching PC\n");
	}
	_PRINTF("PC is %d\n", pc_is_on());
}

static void energy_cb(const char *arg, int l, int *ret)
{
	_PRINTF("Energy: %.3f | Power: %.3f\n", emeter_get_energy_kwh(), emeter_get_power_kw());
}

static void sli_cb(const char *arg, int l, int *ret)
{
	_PRINTF("lwip_assert_counter %d\n", lwip_assert_counter);
	// _PRINTF("state1: %d\n", sock_cli_get_state(cc_sock));
	// _PRINTF("state2: x%x\n", cc_sock->state);

	struct stats_mem *mem = &lwip_stats.mem;
	_PRINTF("\tavail: %" U32_F "\n\t", (u32_t)mem->avail);
	_PRINTF("used: %" U32_F "\n\t", (u32_t)mem->used);
	_PRINTF("max: %" U32_F "\n\t", (u32_t)mem->max);
	_PRINTF("err: %" U32_F "\n", (u32_t)mem->err);

	// _PRINTF("Trying ...\n");
	// struct pbuf *p[25];
	// for(uint32_t i = 0; i < 25; i++)
	// {
	// 	p[i] = pbuf_alloc(PBUF_TRANSPORT, 7, PBUF_POOL);
	// 	_PRINTF("ALLOC %d x%x\n", i, p[i]);
	// 	_PRINTF("pb free ref %d\n", p[i]->ref);
	// 	int r = pbuf_free(p[i]);
	// 	_PRINTF("DEALLOC %d x%x\n", i, r);
	// }
}

static void rtc_cb(const char *arg, int l, int *ret)
{
	RTC_DateTypeDef d;
	RTC_TimeTypeDef t;
	mcu_rtc_get_date(&d);
	mcu_rtc_get_time(&t);
	_PRINTF("gs %d\n", gsts);
	_PRINTF("RTC: %d %d %d : %d - %d %d %d\n", d.RTC_Month, d.RTC_Date, d.RTC_Year, d.RTC_WeekDay, t.RTC_Hours, t.RTC_Minutes, t.RTC_Seconds);
}

static void co_gtw_cb(const char *arg, int l, int *ret)
{
	while(*arg == ' ')
		arg++;
	if(!arg)
	{
		*ret = CON_CB_ERR_ARGS;
		return;
	}
	_PRINTF("gtw(%d):", strlen(arg));
	for(uint32_t i = 0; i < strlen(arg); i++)
		_PRINTF("x%x ", arg[i]);
	size_t len = CO_GTWA_write(CO->gtwa, arg, strlen(arg));
	_PRINTF("\ngtw act (%d)\n", len);
}

const console_cmd_t console_cmd[] = {
	{"fw", fw_cb},
	{"reset", reset_cb},
	{"read", read_cb},
	{"save", save_cb},
	{"ip", ip_cb},
	{"cfgstate", cfg_state_cb},
	{"lon", lon_cb},
	{"loff", loff_cb},
	{"adc", adc_cb},
	{"pc", pc_cb},
	{"rtc", rtc_cb},

	{"sock_mst_ip", sock_mst_ip_cb},
	{"sock_gw_ip", sock_gw_ip_cb},

	{"chb", can_hb_cb},
	{"cfw", can_fw_cb},
	{"csdor", sdo_rd_cb},
	{"csdow", sdo_wr_cb},
	{"csave", can_save_cb},
	{"crstcomm", can_rst_comm_cb},
	{"crstfrz", can_rst_freeze_cb},
	{"crst", can_rst_cb},
	{"cid", can_id_cb},
	{"cemcy", can_emcy_cb},
	{"cbaud", can_baud_cb},

	{"crole", can_role_cb},

	{"cmeteo", can_meteo_cb},

	{"energy", energy_cb},
	// {"cli", cli_cb},
	{"sli", sli_cb},

	{"gtw", co_gtw_cb},
};

const uint32_t console_cmd_sz = sizeof(console_cmd) / sizeof(console_cmd[0]);