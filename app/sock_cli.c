#include "sock_cli.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include <string.h>

#undef BYTE_ORDER
#undef LITTLE_ENDIAN
#undef BIG_ENDIAN
#include <stdio.h>

#include "console_udp.h"

u8_t recev_buf[50];
volatile uint32_t message_count = 0;

#define SOCK_CLI_TO_MS 5000

u8_t data[100];

static err_t tcp_echoclient_poll(void *arg, struct tcp_pcb *tpcb);

enum echoclient_states
{
	ES_NOT_CONNECTED = 0,
	ES_CONNECTED,
	ES_CLOSING,
	ES_UNDEFINED,
};

struct echoclient
{
	enum echoclient_states state; /* connection status */
	struct tcp_pcb *pcb;		  /* pointer on the current tcp_pcb */
	struct pbuf *p_tx;			  /* pointer on pbuf to be transmitted */
};

static void tcp_echoclient_connection_close(struct tcp_pcb *tpcb, struct echoclient *es)
{
	_PRINTF("CLI close\n");
	tcp_recv(tpcb, NULL);
	tcp_sent(tpcb, NULL);
	tcp_poll(tpcb, NULL, 0);
	if(es != NULL) mem_free(es);
	tcp_close(tpcb);
}

static void tcp_echoclient_send(struct tcp_pcb *tpcb, struct echoclient *es)
{
	err_t wr_err = ERR_OK;
	while((wr_err == ERR_OK) &&
		  (es->p_tx != NULL) &&
		  (es->p_tx->len <= tcp_sndbuf(tpcb)))
	{
		struct pbuf *ptr = es->p_tx;
		wr_err = tcp_write(tpcb, ptr->payload, ptr->len, 1);

		if(wr_err == ERR_OK)
		{
			es->p_tx = ptr->next;
			if(es->p_tx != NULL) pbuf_ref(es->p_tx);
			pbuf_free(ptr);
		}
		else if(wr_err == ERR_MEM)
		{
			es->p_tx = ptr; // we are low on memory, try later, defer to poll
		}
		else
		{
			// other problem ??
		}
	}
}

static err_t tcp_echoclient_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	_PRINTF("CLI sent\n");
	struct echoclient *es = (struct echoclient *)arg;
	if(es->p_tx != NULL) tcp_echoclient_send(tpcb, es);
	return ERR_OK;
}

static err_t tcp_echoclient_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	struct echoclient *es = (struct echoclient *)arg;

	if(p == NULL) // if we receive an empty tcp frame from server => close connection
	{
		es->state = ES_CLOSING; // remote host closed connection
		_PRINTF("CLI _e\n");
		if(es->p_tx == NULL)
		{
			tcp_echoclient_connection_close(tpcb, es);
		}
		else
		{
			tcp_echoclient_send(tpcb, es);
		}
		return ERR_OK;
	}
	else if(err != ERR_OK) // a non empty frame was received from echo server but for some reason err != ERR_OK
	{
		pbuf_free(p);
		return err;
	}
	else if(es->state == ES_CONNECTED)
	{
		message_count++;
		tcp_recved(tpcb, p->tot_len);
		pbuf_free(p);
		_PRINTF("CLI 2\n");
		sprintf((char *)data, "=> %ld <=\n", (uint32_t)message_count);
		es->p_tx = pbuf_alloc(PBUF_TRANSPORT, strlen((char *)data), PBUF_POOL);
		if(es->p_tx)
		{
			pbuf_take(es->p_tx, (char *)data, strlen((char *)data));
			tcp_arg(tpcb, es);
			tcp_recv(tpcb, tcp_echoclient_recv);
			tcp_sent(tpcb, tcp_echoclient_sent);
			tcp_poll(tpcb, tcp_echoclient_poll, 1);
			tcp_echoclient_send(tpcb, es);
		}
		// tcp_echoclient_connection_close(tpcb, es);
		return ERR_OK;
	}
	else // data received when connection already closed
	{
		tcp_recved(tpcb, p->tot_len);
		pbuf_free(p);
		return ERR_OK;
	}
}

static err_t tcp_echoclient_poll(void *arg, struct tcp_pcb *tpcb)
{
	struct echoclient *es = (struct echoclient *)arg;
	if(es != NULL)
	{
		if(es->p_tx != NULL)
		{
			tcp_echoclient_send(tpcb, es);
		}
		else if(es->state == ES_CLOSING)
		{
			_PRINTF("CLI _clos\n");
			tcp_echoclient_connection_close(tpcb, es);
		}
		return ERR_OK;
	}
	else
	{
		tcp_abort(tpcb);
		return ERR_ABRT;
	}
}

static err_t tcp_echoclient_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
	_PRINTF("CLI connected\n");
	struct echoclient *es = NULL;
	if(err == ERR_OK)
	{
		_PRINTF("=>alloc\n");
		es = (struct echoclient *)mem_malloc(sizeof(struct echoclient));

		if(es != NULL)
		{
			es->state = ES_CONNECTED;
			es->pcb = tpcb;
			// _PRINTF(":PCB:x%lx (addr x%lx)\n", ((uint32_t)tpcb), ((uint32_t)(tpcb->remote_ip)));
			sprintf((char *)data, "=> %ld <=\n", (uint32_t)message_count);
			es->p_tx = pbuf_alloc(PBUF_TRANSPORT, strlen((char *)data), PBUF_POOL);

			if(es->p_tx)
			{
				pbuf_take(es->p_tx, (char *)data, strlen((char *)data));
				tcp_arg(tpcb, es);
				tcp_recv(tpcb, tcp_echoclient_recv);
				tcp_sent(tpcb, tcp_echoclient_sent);
				tcp_poll(tpcb, tcp_echoclient_poll, 1);
				tcp_echoclient_send(tpcb, es);
				return ERR_OK;
			}
		}
		else
		{
			_PRINTF("CLI wtf 1\n");
			tcp_echoclient_connection_close(tpcb, es);
			return ERR_MEM;
		}
	}
	else
	{
		_PRINTF("CLI wtf 2\n");
		tcp_echoclient_connection_close(tpcb, es);
	}
	return err;
}

int sock_cli_init(sock_cli_t *c, const uint8_t *ipaddr, int port)
{
	c->inst = tcp_new();
	c->port = port;
	c->timeout_cnt = 0;
	if(c->inst != NULL)
	{
		IP4_ADDR(&c->DestIPaddr, ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);
		tcp_connect(c->inst, &c->DestIPaddr, c->port, tcp_echoclient_connected);
		return 0;
	}
	else
	{
		memp_free(MEMP_TCP_PCB, c->inst);
		return 1;
	}
}

void sock_cli_poll(sock_cli_t *c, uint32_t diff_ms)
{
	bool nc = true;
	if(c->inst != NULL)
	{
		if(sock_cli_get_state(c) == ES_CONNECTED)
		{
			nc = false;
		}
	}

	if(nc)
	{
		if(c->timeout_cnt >= SOCK_CLI_TO_MS)
		{
			_PRINTF("TO, Reconnecting...\n");
			c->timeout_cnt = 0;
			tcp_echoclient_connection_close(c->inst, NULL);
			c->inst = tcp_new();
			if(c->inst != NULL)
			{
				tcp_connect(c->inst, &c->DestIPaddr, c->port, tcp_echoclient_connected);
			}
		}
		else
		{
			c->timeout_cnt += diff_ms;
		}
	}
}

int sock_cli_get_state(sock_cli_t *c)
{
	struct echoclient *es = (struct echoclient *)c->inst->callback_arg;
	if(es != NULL)
	{
		return es->state;
	}
	return ES_UNDEFINED;
}