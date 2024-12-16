#include "config_system.h"
#include "eth/netconf.h"
#include "eth_con/console_udp.h"
#include "ethernetif.h"
#include "fw_header.h"
#include "platform.h"
#include <ctype.h>
#include <string.h>

static void fw_cb(const char *arg, int l, int *ret)
{
	_PRINTF("Reset from: %s\n", paltform_reset_cause_get());
	fw_header_check_all();
	const char *p_build_ts = fw_fields_find_by_key_helper(&g_fw_info[FW_TYPE], "build_ts");
	_PRINTF("%s %s : %d.%d.%d : %s\n",
			g_fw_info[FW_LDR].field_product_ptr,
			g_fw_info[FW_LDR].field_product_name_ptr,
			g_fw_info[FW_LDR].ver_major,
			g_fw_info[FW_LDR].ver_minor,
			g_fw_info[FW_LDR].ver_patch,
			p_build_ts ? p_build_ts : "");
	_PRINTF("PRELDR: %d\n", g_fw_info[FW_PRELDR].locked);
	_PRINTF("LDR   : %d\n", g_fw_info[FW_LDR].locked);
	_PRINTF("APP   : %d\n", g_fw_info[FW_APP].locked);
	_PRINTF("UID   : x%08x.x%08x.x%08x\n", g_uid[0], g_uid[1], g_uid[2]);
	_PRINTF("IP    : %d.%d.%d.%d\n", LwIP_cfg_ip()[0], LwIP_cfg_ip()[1], LwIP_cfg_ip()[2], LwIP_cfg_ip()[3]);
	_PRINTF("MAC   : x%x:x%x:x%x:x%x:x%x:x%x\n", ethernetif_cfg_mac()[0], ethernetif_cfg_mac()[1], ethernetif_cfg_mac()[2], ethernetif_cfg_mac()[3], ethernetif_cfg_mac()[4], ethernetif_cfg_mac()[5]);
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

static void reset_cb(const char *arg, int l, int *ret)
{
	platform_reset();
}

const console_cmd_t console_cmd[] = {
	{"fw", fw_cb},
	{"reset", reset_cb},
	{"save", save_cb},
	{"cfgstate", cfg_state_cb},
};

const uint32_t console_cmd_sz = sizeof(console_cmd) / sizeof(console_cmd[0]);