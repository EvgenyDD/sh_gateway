#ifndef SOCK_CLI_H__
#define SOCK_CLI_H__

#include "lwip/tcp.h"
#include <stdint.h>

int sock_cli_init(struct tcp_pcb **inst, const uint8_t *ipaddr, int port);
void sock_cli_poll(uint32_t diff_ms);
int sock_cli_get_state(struct tcp_pcb *inst);

#endif // SOCK_CLI_H__