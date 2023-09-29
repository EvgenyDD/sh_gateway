#ifndef SOCK_MASTER_H__
#define SOCK_MASTER_H__

#include "lwip/tcp.h"

int sock_master_init(struct tcp_pcb *inst, int port);

#endif // SOCK_MASTER_H__