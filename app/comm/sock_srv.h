#ifndef SOCK_SRV_H__
#define SOCK_SRV_H__

#include "lwip/tcp.h"
#include <stddef.h>
#include <stdint.h>

typedef enum
{
	ES_NONE = 0,
	ES_ACCEPTED,
	ES_RECEIVED,
	ES_CLOSING
} tcp_echoserver_states;

struct tcp_echoserver_struct
{
	tcp_echoserver_states state;
	struct tcp_pcb *pcb;
	struct pbuf *p;
};

int sock_srv_init(struct tcp_pcb *inst, int port);
int sock_srv_send(struct tcp_pcb *inst, const uint8_t *data, size_t len);

#endif // SOCK_SRV_H__