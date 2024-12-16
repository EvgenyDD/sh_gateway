#include "console_udp.h"
#include "_printf.h"
#include "lwip/udp.h"
#include "platform.h"
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define UDP_PORT_CONSOLE 2000

extern bool g_stay_in_boot;

static struct
{
	struct udp_pcb *udp_pcb_con; // The actual UDP port
	struct ip_addr addr;
	uint16_t port;
} con_udp = {NULL, {0}, 0};

static char print_buf[256];
static const char *error_str = "";

void _PRINTF(const char *fmt, ...)
{
	if(!con_udp.port) return;

	va_list ap;
	va_start(ap, fmt);
	int act_len = vsnprintf(print_buf, sizeof(print_buf) - 1, fmt, ap);
	print_buf[act_len] = 0;
	act_len++;
	va_end(ap);

	struct pbuf *p_send = pbuf_alloc(PBUF_TRANSPORT, act_len, PBUF_RAM);
	memcpy(p_send->payload, print_buf, (size_t)act_len);
	udp_sendto(con_udp.udp_pcb_con, p_send, &con_udp.addr, con_udp.port);

	pbuf_free(p_send);
}

void _PRINT_PREFIX(void)
{
	if(system_time >= 86400000ULL)
		_PRINTF("[" DEV " %d.%d.%d.%d " DT_FMT_MS "]:", LwIP_cfg_ip()[0], LwIP_cfg_ip()[1], LwIP_cfg_ip()[2], LwIP_cfg_ip()[3], DT_DATA_MS(system_time));
	else
		_PRINTF("[" DEV " %d.%d.%d.%d " DT_FMT_MS_SIMPLE "]:", LwIP_cfg_ip()[0], LwIP_cfg_ip()[1], LwIP_cfg_ip()[2], LwIP_cfg_ip()[3], DT_DATA_MS_SIMPLE(system_time));
}

static uint8_t prev_request[256] = {0};
static int prev_request_len = 0;
static uint16_t prev_port = 0;

static void cb_udp_console(void *arg, struct udp_pcb *udp, struct pbuf *p, struct ip_addr *addr, uint16_t port)
{
	g_stay_in_boot = true;

	memcpy(&con_udp.addr, addr, sizeof(struct ip_addr));
	con_udp.port = port;
	if(port != prev_port)
	{
		prev_port = port;
		prev_request[0] = '\0';
	}

	uint8_t *data = (uint8_t *)p->payload;
	data[p->len] = 0;
	if(p->len >= sizeof(prev_request)) return;

	uint16_t len_req = p->len;

	if(len_req == 1 && data[0] == '\n')
	{
		data = prev_request;
		len_req = prev_request_len;
		if(prev_request[0] != '\n' && prev_request[0] != '\0') _PRINTF(">");
	}
	else
	{
		memcpy(prev_request, data, len_req);
		prev_request[len_req] = 0;
		prev_request_len = len_req;
	}

	uint16_t l;
	int32_t idx;

	const char *param = 0;

	int error_code = CON_CB_SILENT;

	if(strncmp((char *)data, "help", 4) == 0)
	{
		_PRINTF("Available commands:\n");
		for(uint32_t i = 0; i < console_cmd_sz; i++)
		{
			_PRINTF("\t%s\n", console_cmd[i].name);
		}
	}
	else if((strcmp((char *)data, "\n") != 0) && (data[0] != '#') && data[0])
	{
		for(uint32_t i = 0; i < console_cmd_sz; i++)
		{
			if(console_cmd[i].cb)
			{
				l = strlen(console_cmd[i].name);
				if(strncmp((char *)data, (const char *)console_cmd[i].name, l) == 0)
				{
					if((len_req - l) > 0)
					{
						param = data + l;
					}
					else
					{
						param = 0;
					}

					console_cmd[i].cb(param, len_req - l, &error_code);
					if(error_code > CON_CB_SILENT)
					{
						_PRINTF("Error: ");
						switch(error_code)
						{
						case CON_CB_ERR_CUSTOM:
							_PRINTF("%s\n", error_str);
							break;
						case CON_CB_ERR_UNSAFE:
							_PRINTF("Unsafe operation\n");
							break;
						case CON_CB_ERR_NO_SPACE:
							_PRINTF("No space left\n");
							break;
						case CON_CB_ERR_BAD_PARAM:
							_PRINTF("Bad parameter\n");
							break;
						case CON_CB_ERR_ARGS:
							_PRINTF("To few arguments\n");
							break;
						default: break;
						}
					}
					else if(!error_code)
					{
						_PRINTF("Ok\n");
					}
					break;
				}
			}
			if(i == console_cmd_sz - 1)
			{
				PRINTF("command not found\n");
			}
		}
	}
	else
	{
		PRINTF("\n");
	}

	pbuf_free(p);
}

int console_udp_init(void)
{
	con_udp.udp_pcb_con = udp_new();
	if(con_udp.udp_pcb_con == NULL) return -1;

	if(udp_bind(con_udp.udp_pcb_con, IP_ADDR_ANY, UDP_PORT_CONSOLE) != ERR_OK) return -2;

	udp_recv(con_udp.udp_pcb_con, cb_udp_console, NULL);
	return 0;
}