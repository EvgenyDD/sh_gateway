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
#include "pc.h"
#include "platform.h"
#include "sdo.h"
#include "sock_cli.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

extern CO_t *CO;
#include "console_can.h"

int fw_cb(const char *arg, int l)
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
	return CON_CB_SILENT;
}

int reset_cb(const char *arg, int l)
{
	platform_reset();
	return CON_CB_SILENT;
}

int ip_cb(const char *arg, int l)
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
		return CON_CB_SILENT;
	}

	if((n != 4) || (a > 255) || (b > 255) || (c > 255) || (d > 255))
	{
		_PRINTF("Bad address\n");
		return CON_CB_ERR_BAD_PARAM;
	}

	g_ip_address[0] = a;
	g_ip_address[1] = b;
	g_ip_address[2] = c;
	g_ip_address[3] = d;

	_PRINTF("OK, New IP: %d.%d.%d.%d\n", g_ip_address[0], g_ip_address[1], g_ip_address[2], g_ip_address[3]);
	return CON_CB_SILENT;
}

int read_cb(const char *arg, int l)
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
	return CON_CB_SILENT;
}

int save_cb(const char *arg, int l)
{
	int sts = config_write_storage();
	if(sts != 0) _PRINTF("SAVE ERROR: %d\n", sts);
	if(sts == 0)
	{
		sts = config_validate();
		_PRINTF("SAVE config status: %d\n", sts);
	}
	return CON_CB_SILENT;
}

int cfg_state_cb(const char *arg, int l)
{
	int sts = config_validate();
	_PRINTF("Config status: %d\n", sts);
	return CON_CB_SILENT;
}

int lon_cb(const char *arg, int l)
{
	load_switcher_on();
	_PRINTF("on ok\n");
	return CON_CB_SILENT;
}

int loff_cb(const char *arg, int l)
{
	load_switcher_off();
	_PRINTF("off ok\n");
	return CON_CB_SILENT;
}

int adc_cb(const char *arg, int l)
{
	_PRINTF("VIN:  %.2f\n", adc_val.vin);
	_PRINTF("VOUT: %.2f\n", adc_val.vout);
	_PRINTF("Is:   %.3f\n", adc_val.i_sns);
	_PRINTF("IR:   %.0f\n", adc_val.ir);

	return CON_CB_SILENT;
}

int pc_cb(const char *arg, int l)
{
	unsigned int v = 0;
	int n = sscanf(arg, "%d", &v);
	if(n == 1)
	{
		pc_switch();
		_PRINTF("Switching PC\n");
	}
	_PRINTF("PC is %d\n", pc_is_on());
	return CON_CB_SILENT;
}

int energy_cb(const char *arg, int l)
{
	_PRINTF("Energy: %.3f | Power: %.3f\n", emeter_get_energy_kwh(), emeter_get_power_kw());
	return CON_CB_SILENT;
}

static struct tcp_pcb cc_sock;
int cli_cb(const char *arg, int l)
{
	uint8_t ipaddr[] = {7, 7, 7, 1};
	int sts = sock_cli_init(&cc_sock, ipaddr, 5000);
	_PRINTF("STS: %d\n", sts);
	return CON_CB_OK;
}
int sli_cb(const char *arg, int l)
{
	_PRINTF("flags: %d\n", cc_sock.flags);
	return CON_CB_OK;
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
	{"cli", cli_cb},
	{"sli", sli_cb},
};

const uint32_t console_cmd_sz = sizeof(console_cmd) / sizeof(console_cmd[0]);