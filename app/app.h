#ifndef APP_H__
#define APP_H__

#include <stdint.h>

typedef struct
{
	int aht21_init_sts;
	int fram_init_sts;
	int rtc_init_sts;
	int cfg_init_sts;
	int console_udp_init_sts;
	int tftpd_init_sts;
	int sock_srv_init_sts;

	struct
	{
		uint8_t cnt_rst_comm;
		uint8_t cnt_rst_app;
	} can;
} diag_t;

extern diag_t diag;

#endif // APP_H__