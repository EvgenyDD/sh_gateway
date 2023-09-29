#ifndef SOCK_SRV_H__
#define SOCK_SRV_H__

#include "lwip/tcp.h"

int sock_srv_init(struct tcp_pcb *inst, int port);

#endif // SOCK_SRV_H__