#ifndef SOCK_CLI_H__
#define SOCK_CLI_H__

#include "lwip/tcp.h"
#include <stdint.h>

int sock_cli_init(struct tcp_pcb *inst, const uint8_t *ipaddr, int port);

#endif // SOCK_CLI_H__