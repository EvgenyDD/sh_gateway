#include "CANopen.h"
#include "CO_driver_app.h"
#include "OD.h"
#include "adc.h"
#include "aht21.h"
#include "app.h"
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
#include "mains_sense.h"
#include "mqtt_app.h"
#include "pc.h"
#include "pdo_router.h"
#include "platform.h"
#include "poe_switcher.h"
#include "ret_mem.h"
#include "router.h"
#include "rtc.h"
#include "sdo.h"
#include "sock_cli.h"
#include <ctype.h>
#include <string.h>

extern uint32_t pdo_rx_cnt;
extern uint32_t lwip_assert_counter;
extern bool g_cfg_i2c_remap;

extern uint32_t g_cfg_mains_sns_chnl_bits;
extern uint32_t g_cfg_poe_sw_chnl_bits;

extern CO_t *CO;

uint32_t can_rx_msg = 0, can_tx_msg = 0;

#undef BYTE_ORDER
#undef LITTLE_ENDIAN
#undef BIG_ENDIAN
#include <stdio.h>

uint32_t ir_min = 4095, ir_max = 0;

#include "console_can.h"

static void fw_cb(const char *arg, int l, int *ret)
{
	_PRINT_PREFIX();
	_PRINTF("\nReset ldr: %s | app: %s\n", platform_reset_cause_get(ret_mem_get_rst_cause_ldr()), platform_reset_cause_get(ret_mem_get_rst_cause_app()));
	fw_header_check_all();
	const char *p_build_ts = fw_fields_find_by_key_helper(&g_fw_info[FW_TYPE], "build_ts");
	_PRINTF("%s %s : %ld.%ld.%ld : %s\n",
			g_fw_info[FW_APP].field_product_ptr,
			g_fw_info[FW_APP].field_product_name_ptr,
			g_fw_info[FW_APP].ver_major,
			g_fw_info[FW_APP].ver_minor,
			g_fw_info[FW_APP].ver_patch,
			p_build_ts ? p_build_ts : "");
	_PRINTF("PRELDR: %d | @x%x %ld | %d fields\n", g_fw_info[FW_PRELDR].locked, g_fw_info[FW_PRELDR].addr, g_fw_info[FW_PRELDR].size, g_fw_info[FW_PRELDR].fields_count);
	_PRINTF("LDR   : %d | @x%x %ld | %d fields\n", g_fw_info[FW_LDR].locked, g_fw_info[FW_LDR].addr, g_fw_info[FW_LDR].size, g_fw_info[FW_LDR].fields_count);
	_PRINTF("APP   : %d | @x%x %ld | %d fields\n", g_fw_info[FW_APP].locked, g_fw_info[FW_APP].addr, g_fw_info[FW_APP].size, g_fw_info[FW_APP].fields_count);
	_PRINTF("UID   : x%08lx.x%08lx.x%08lx\n", g_uid[0], g_uid[1], g_uid[2]);
	_PRINTF("IP    : %d.%d.%d.%d\n", LwIP_cfg_ip()[0], LwIP_cfg_ip()[1], LwIP_cfg_ip()[2], LwIP_cfg_ip()[3]);
	_PRINTF("MAC   : x%x:x%x:x%x:x%x:x%x:x%x\n", ethernetif_cfg_mac()[0], ethernetif_cfg_mac()[1], ethernetif_cfg_mac()[2], ethernetif_cfg_mac()[3], ethernetif_cfg_mac()[4], ethernetif_cfg_mac()[5]);
	_PRINTF("Sys freq: %.2f Hz\n", sys_freq_1s);
}

static void can_cb(const char *arg, int l, int *ret)
{
	_PRINTF("RST COMM %d | APP %d\n", diag.can.cnt_rst_comm, diag.can.cnt_rst_app);
	_PRINTF("NMT state: %d [%s]\n", CO_NMT_getInternalState(CO->NMT), nmt_state2str(CO_NMT_getInternalState(CO->NMT)));
	_PRINTF("Err reg: x%x | Err status: x%x | BO %d (CNT %ld) | REC %d | TEC %d\n",
			*CO->em->errorRegister,
			CO->CANmodule->CANerrorStatus,
			CO->CANmodule->diag.is_bus_off,
			CO->CANmodule->diag.bus_off_err_cnt,
			CO->CANmodule->diag.rx_err_cnt,
			CO->CANmodule->diag.tx_err_cnt);
	_PRINTF("x1005 x%lx | x1006 x%lx\n", OD_PERSIST_COMM.x1005_COB_ID_SYNCMessage, OD_PERSIST_COMM.x1006_communicationCyclePeriod);
	_PRINTF("PDO (EN %d) RX %ld rx %ld tx %ld\n\t", pdo_switch_state(), pdo_rx_cnt, can_rx_msg, can_tx_msg);
	can_hb_cb(arg, l, ret);
}

static void info_cb(const char *arg, int l, int *ret)
{
	_PRINTF("\taht21 %d\n", diag.aht21_init_sts);
	_PRINTF("\tfram %d\n", diag.fram_init_sts);
	_PRINTF("\trtc %d\n", diag.rtc_init_sts);
	_PRINTF("\tcfg %d\n", diag.cfg_init_sts);
	_PRINTF("\tconsole_udp %d\n", diag.console_udp_init_sts);
	_PRINTF("\ttftpd %d\n", diag.tftpd_init_sts);
	_PRINTF("\tsock_srv %d\n", diag.sock_srv_init_sts);

	_PRINTF("energy %.3f | power %.3f | CNT %ld\n", emeter_get_energy_kwh(), emeter_get_power_kw(), emeter_get_cnt());
	_PRINTF("AHT exist %d | temp %.2f | hum %.2f\n", aht21.exist, aht21.temp, aht21.hum);
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
		arg++;

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

	g_cfg_ip_address[0] = a;
	g_cfg_ip_address[1] = b;
	g_cfg_ip_address[2] = c;
	g_cfg_ip_address[3] = d;

	_PRINTF("OK, New IP: %d.%d.%d.%d\n", g_cfg_ip_address[0], g_cfg_ip_address[1], g_cfg_ip_address[2], g_cfg_ip_address[3]);
}

static void sock_mst_ip_cb(const char *arg, int l, int *ret)
{
	unsigned int a = 0, b = 0, c = 0, d = 0;
	int n = sscanf(arg, "%u.%u.%u.%u", &a, &b, &c, &d);

	while(isspace((int)*arg) && (*arg != 0))
		arg++;

	if(!*arg)
	{
		_PRINTF("Current MASTER slv IP: %d.%d.%d.%d\n", g_cfg_ip_addr_master[0], g_cfg_ip_addr_master[1], g_cfg_ip_addr_master[2], g_cfg_ip_addr_master[3]);
		return;
	}

	if((n != 4) || (a > 255) || (b > 255) || (c > 255) || (d > 255))
	{
		_PRINTF("Bad address\n");
		*ret = CON_CB_ERR_BAD_PARAM;
		return;
	}

	g_cfg_ip_addr_master[0] = a;
	g_cfg_ip_addr_master[1] = b;
	g_cfg_ip_addr_master[2] = c;
	g_cfg_ip_addr_master[3] = d;

	_PRINTF("OK, New IP: %d.%d.%d.%d\n", g_cfg_ip_addr_master[0], g_cfg_ip_addr_master[1], g_cfg_ip_addr_master[2], g_cfg_ip_addr_master[3]);
}

static void sock_gw_ip_cb(const char *arg, int l, int *ret)
{
	unsigned int ptr = 0, a = 0, b = 0, c = 0, d = 0;
	int n = sscanf(arg, "%u:%u.%u.%u.%u", &ptr, &a, &b, &c, &d);

	while(isspace((int)*arg) && (*arg != 0))
		arg++;

	if(!*arg)
	{
		_PRINTF("Current GW slv IP:\n");
		for(uint32_t i = 0; i < GW_COUNT; i++)
		{
			_PRINTF("\t%d.%d.%d.%d\n", g_cfg_ip_addr_gw[4 * i + 0], g_cfg_ip_addr_gw[4 * i + 1], g_cfg_ip_addr_gw[4 * i + 2], g_cfg_ip_addr_gw[4 * i + 3]);
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

	g_cfg_ip_addr_gw[4 * ptr + 0] = a;
	g_cfg_ip_addr_gw[4 * ptr + 1] = b;
	g_cfg_ip_addr_gw[4 * ptr + 2] = c;
	g_cfg_ip_addr_gw[4 * ptr + 3] = d;

	_PRINTF("OK, New GW slv IP:\n");
	for(uint32_t i = 0; i < GW_COUNT; i++)
	{
		_PRINTF("\t%d.%d.%d.%d\n", g_cfg_ip_addr_gw[4 * i + 0], g_cfg_ip_addr_gw[4 * i + 1], g_cfg_ip_addr_gw[4 * i + 2], g_cfg_ip_addr_gw[4 * i + 3]);
	}
}

static void read_cb(const char *arg, int l, int *ret)
{
	int sts = config_validate();
	if(sts == 0) config_read_storage();
	_PRINTF("READ config status: %d\n", sts);

	for(uint32_t i = 0; i < sizeof(g_device_config_count); i++)
	{
		_PRINTF("%s (%d) @ x%lx [", g_device_config[i].key, g_device_config[i].size, g_device_config[i].data_abs_address);
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
	_PRINTF("I POE: %.3f %.3f %.3f\n", adc_val.poe_sns[0], adc_val.poe_sns[1], adc_val.poe_sns[2]);
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

static void sli_cb(const char *arg, int l, int *ret)
{
	_PRINTF("lwip_assert_counter %ld\n", lwip_assert_counter);
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

static void timedate_cb(const char *req, int len, int *ret)
{
	if(len)
	{
		unsigned int year, weekday, month, date;
		unsigned int hours, minutes, seconds;
		int n = sscanf(req, "%u %u %u %u %u %u %u",
					   &year, &month, &date, &weekday,
					   &hours, &minutes, &seconds);
		if(n == 7)
		{
			int sts0 = mcu_rtc_set_date(year, weekday, month, date);
			int sts1 = mcu_rtc_set_time(hours, minutes, seconds);
			_PRINTF("Set: %u/%u/%u (%u) - %u:%u:%u | status [%d %d]\n",
					year, month, date, weekday,
					hours, minutes, seconds,
					sts0, sts1);
			return;
		}
	}
	RTC_DateTypeDef date;
	RTC_TimeTypeDef time;
	mcu_rtc_get_date(&date);
	mcu_rtc_get_time(&time);

	_PRINTF("%04d/%02d/%02d (%s) - %02d:%02d:%02d\n",
			date.RTC_Year_True, date.RTC_Month, date.RTC_Date, mcu_rtc_weekday(date.RTC_WeekDay),
			time.RTC_Hours, time.RTC_Minutes, time.RTC_Seconds);
}

static void i2c_remap_cb(const char *arg, int l, int *ret)
{
	unsigned int v;
	int n = sscanf(arg, "%u", &v);
	if(n == 1)
	{
		g_cfg_i2c_remap = v;
		return;
	}
	_PRINTF("I2C Remap: %d\n", g_cfg_i2c_remap);
}

static void fram_cb(const char *arg, int l, int *ret)
{
#define FRAM_RD_QUANT 16
	unsigned int v;
	int n = sscanf(arg, "%u", &v);
	if(n == 1)
	{
		uint8_t buf[FRAM_RD_QUANT];
		int sts = fram_read(v, buf, FRAM_RD_QUANT);
		_PRINTF("FRAM (sts %d) @x%x: ", sts, v);
		for(uint32_t i = 0; i < FRAM_RD_QUANT; i++)
			_PRINTF("%x ", buf[i]);
		_PRINTF("\n");
	}
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

#define BIT_WR(reg, bit, val) ((val) ? ((reg) |= (1UL << (bit))) : ((reg) &= ~(1UL << (bit))))

static void cfg_mains_sns_cb(const char *arg, int l, int *ret)
{
	unsigned int ch = 0, en = 0;
	int n = sscanf(arg, "%u %u", &ch, &en);
	if(n != 2)
	{
		_PRINTF("Wrong format! \"chnl en\"\n");
		return;
	}
	if(ch < 32) BIT_WR(g_cfg_mains_sns_chnl_bits, ch, en);
	_PRINTF("New Mains Sense mask: x%lx\n", g_cfg_mains_sns_chnl_bits);
}

static void cfg_poe_sw_cb(const char *arg, int l, int *ret)
{
	unsigned int ch = 0, en = 0;
	int n = sscanf(arg, "%u %u", &ch, &en);
	if(n != 2)
	{
		_PRINTF("Wrong format! \"chnl en\"\n");
		return;
	}
	if(ch < 32) BIT_WR(g_cfg_poe_sw_chnl_bits, ch, en);
	_PRINTF("New POE Switch mask: x%lx\n", g_cfg_poe_sw_chnl_bits);
}

static void poe_sw_cb(const char *arg, int l, int *ret)
{
	unsigned int ch = 0, en = 0;
	int n = sscanf(arg, "%u %u", &ch, &en);
	if(n != 2)
	{
		_PRINTF("POE #0: %d\n", poe_switcher_is_on(0));
		return;
	}

	if(en)
	{
		_PRINTF("POE ON\n");
		poe_switcher_on(ch);
	}
	else
	{
		_PRINTF("POE OFF\n");
		poe_switcher_off(ch);
	}
}

static void mains_get_cb(const char *arg, int l, int *ret)
{
	_PRINTF("Mains: %ld %ld\n", mains_stat_get_cnt(0), mains_stat_get_hit(0));
	_PRINTF("IR: %ld %ld\n", ir_min, ir_max);
}

static void mains_rst_cb(const char *arg, int l, int *ret)
{
	mains_stat_rst(0);
	_PRINTF("Mains resetted\n");
	ir_max = 0;
	ir_min = 4095;
}

static void mqtt_sts_cb(const char *arg, int l, int *ret)
{
	_PRINTF("sts: %d | conn: %d | cnt: %d\n", mqtt_dbg.status, mqtt_dbg.conn_code, mqtt_dbg.conn_cb_cnt);
	_PRINTF("TX Q OVF: %ld | Err publ: %ld\n", mqtt_dbg.tx_q_ovf, mqtt_dbg.err_publ);
}

static void route_cb(const char *arg, int l, int *ret)
{
	if(l > 1)
	{
		int sts = router_add_entry(arg);
		_PRINTF("sts: %d \n", sts);
	}
	else
	{
		router_print();
	}
}

const console_cmd_t console_cmd[] = {
	{"fw", fw_cb},
	{"info", info_cb},
	{"can", can_cb},
	{"reset", reset_cb},
	{"read", read_cb},
	{"save", save_cb},
	{"ip", ip_cb},
	{"cfgstate", cfg_state_cb},
	{"lon", lon_cb},
	{"loff", loff_cb},
	{"adc", adc_cb},
	{"pc", pc_cb},
	{"time", timedate_cb},
	{"i2c_remap", i2c_remap_cb},
	{"fram", fram_cb},

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

	// {"cli", cli_cb},
	{"sli", sli_cb},

	{"gtw", co_gtw_cb},

	{"cfg_mains_sns", cfg_mains_sns_cb},
	{"cfg_poe_sw", cfg_poe_sw_cb},
	{"poe_sw", poe_sw_cb},
	{"mg", mains_get_cb},
	{"mr", mains_rst_cb},

	{"mqtt", mqtt_sts_cb},
	{"route", route_cb},
};

const uint32_t console_cmd_sz = sizeof(console_cmd) / sizeof(console_cmd[0]);