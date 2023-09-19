#include "fw_header.h"
#include "platform.h"
#include "tftp_server.h"
#include <stdio.h>
#include <string.h>

tftp_cb_t tftp_cb = {0};

int cb_wr_gw_app(uint32_t addr, const uint8_t *data, uint32_t size)
{
	if(addr != 0 && size == 0)
	{
		fw_header_check_all();
		return g_fw_info[FW_LDR].locked;
	}
	if(addr + size > (uint32_t)&__app_end - (uint32_t)&__app_start) return 6;
	return platform_flash_write((uint32_t)&__app_start + addr, data, size);
}

static int gw_read(uint32_t fw_type, uint32_t start, uint32_t end, uint32_t addr, uint8_t *data, uint32_t size_buffer, uint32_t *size_read)
{
	if(g_fw_info[fw_type].locked == 0)
	{
		end = end - start > g_fw_info[fw_type].size ? start + g_fw_info[fw_type].size : end;
	}
	int len = end > addr + start ? end - addr - start : 0;
	if(len > size_buffer) len = size_buffer;
	if(len) memcpy(data, (void *)(start + addr), len);
	*size_read = len;
	return 0;
}
int cb_gw_rd_preldr(uint32_t addr, uint8_t *data, uint32_t size_buffer, uint32_t *size_read) { return gw_read(FW_PRELDR, (uint32_t)&__preldr_start, (uint32_t)&__preldr_end, addr, data, size_buffer, size_read); }
int cb_gw_rd_ldr(uint32_t addr, uint8_t *data, uint32_t size_buffer, uint32_t *size_read) { return gw_read(FW_LDR, (uint32_t)&__ldr_start, (uint32_t)&__ldr_end, addr, data, size_buffer, size_read); }
int cb_gw_rd_app(uint32_t addr, uint8_t *data, uint32_t size_buffer, uint32_t *size_read) { return gw_read(FW_APP, (uint32_t)&__app_start, (uint32_t)&__app_end, addr, data, size_buffer, size_read); }

static struct
{
	const char *name;
	int (*p_rd)(uint32_t addr, uint8_t *data, uint32_t size_buffer, uint32_t *size_read);
} gw_rd[] = {{"sh_gw_preldr", cb_gw_rd_preldr},
			 {"sh_gw_ldr", cb_gw_rd_ldr},
			 {"sh_gw_app", cb_gw_rd_app}};

uint8_t get_can_id(const char *s)
{
	if(*s == '\0') return 0;
	unsigned int id = 0;
	int n = sscanf(s, "%d", &id);
	if(n != 1) return 0;
	return id;
}

int tftp_server_check_file(bool is_write, char *file_name)
{
	fw_header_check_all();
	if(strcmp(file_name, "sh_gw_app") == 0 && is_write)
	{
		platform_flash_erase_flag_reset();
		tftp_cb.write = cb_wr_gw_app;
		return 0;
	}
	for(uint32_t i = 0; i < sizeof(gw_rd) / sizeof(gw_rd[0]); i++)
	{
		if(strcmp(file_name, gw_rd[i].name) == 0 && is_write == false)
		{
			tftp_cb.read = gw_rd[i].p_rd;
			return 0;
		}
	}

	return 1;
}