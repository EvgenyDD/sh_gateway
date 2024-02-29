#ifndef SOCK_CLI_H__
#define SOCK_CLI_H__

#include "lwip/tcp.h"
#include <stdint.h>

typedef struct
{
	struct tcp_pcb *inst;
	uint32_t timeout_cnt;
	struct ip_addr DestIPaddr;
	int port;
} sock_cli_t;

int sock_cli_init(sock_cli_t *c, const uint8_t *ipaddr, int port);
void sock_cli_poll(sock_cli_t *c, uint32_t diff_ms);
int sock_cli_get_state(sock_cli_t *c);

#endif // SOCK_CLI_H__