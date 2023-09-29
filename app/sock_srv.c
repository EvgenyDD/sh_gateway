#include "sock_srv.h"
#include "lwip/debug.h"
#include "lwip/stats.h"

static struct tcp_pcb *tcp_echoserver_pcb;

enum tcp_echoserver_states
{
	ES_NONE = 0,
	ES_ACCEPTED,
	ES_RECEIVED,
	ES_CLOSING
};

struct tcp_echoserver_struct
{
	u8_t state;
	struct tcp_pcb *pcb;
	struct pbuf *p;
};

static void tcp_echoserver_error(void *arg, err_t err)
{
	struct tcp_echoserver_struct *es = (struct tcp_echoserver_struct *)arg;
	if(es != NULL) mem_free(es);
}

static void tcp_echoserver_send(struct tcp_pcb *tpcb, struct tcp_echoserver_struct *es)
{
	err_t wr_err = ERR_OK;
	while((wr_err == ERR_OK) &&
		  (es->p != NULL) &&
		  (es->p->len <= tcp_sndbuf(tpcb)))
	{
		struct pbuf *ptr = es->p;
		wr_err = tcp_write(tpcb, ptr->payload, ptr->len, 1);

		if(wr_err == ERR_OK)
		{
			u16_t plen = ptr->len;
			es->p = ptr->next;
			if(es->p != NULL) pbuf_ref(es->p);
			pbuf_free(ptr);			// free pbuf: will free pbufs up to es->p (because es->p has a reference count > 0)
			tcp_recved(tpcb, plen); // Update tcp window size to be advertized : should be called when received data (with the amount plen) has been processed by the application layer
		}
		else if(wr_err == ERR_MEM)
		{
			es->p = ptr; // we are low on memory, try later / harder, defer to poll
		}
		else
		{
			// other problem ??
		}
	}
}

static void tcp_echoserver_connection_close(struct tcp_pcb *tpcb, struct tcp_echoserver_struct *es)
{
	tcp_arg(tpcb, NULL);
	tcp_sent(tpcb, NULL);
	tcp_recv(tpcb, NULL);
	tcp_err(tpcb, NULL);
	tcp_poll(tpcb, NULL, 0);
	if(es != NULL) mem_free(es);
	tcp_close(tpcb);
}

static err_t tcp_echoserver_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	struct tcp_echoserver_struct *es = (struct tcp_echoserver_struct *)arg;

	if(es->p != NULL)
	{
		tcp_echoserver_send(tpcb, es);
	}
	else if(es->state == ES_CLOSING)
	{
		tcp_echoserver_connection_close(tpcb, es);
	}
	return ERR_OK;
}

static err_t tcp_echoserver_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	struct tcp_echoserver_struct *es = (struct tcp_echoserver_struct *)arg;
	if(p == NULL)
	{
		es->state = ES_CLOSING;
		if(es->p == NULL)
		{
			tcp_echoserver_connection_close(tpcb, es);
		}
		else
		{
			tcp_sent(tpcb, tcp_echoserver_sent);
			tcp_echoserver_send(tpcb, es);
		}
		return ERR_OK;
	}
	else if(err != ERR_OK) // a non empty frame was received from client but for some reason err != ERR_OK
	{
		es->p = NULL;
		pbuf_free(p);
		return err;
	}
	else if(es->state == ES_ACCEPTED)
	{
		es->state = ES_RECEIVED;
		es->p = p;
		tcp_sent(tpcb, tcp_echoserver_sent);
		tcp_echoserver_send(tpcb, es);
		return ERR_OK;
	}
	else if(es->state == ES_RECEIVED)
	{
		if(es->p == NULL)
		{
			es->p = p;
			tcp_echoserver_send(tpcb, es);
		}
		else
		{
			struct pbuf *ptr = es->p;
			pbuf_chain(ptr, p);
		}
		return ERR_OK;
	}
	else // data received when connection already closed
	{
		tcp_recved(tpcb, p->tot_len);
		es->p = NULL;
		pbuf_free(p);
		return ERR_OK;
	}
}

static err_t tcp_echoserver_poll(void *arg, struct tcp_pcb *tpcb)
{
	struct tcp_echoserver_struct *es = (struct tcp_echoserver_struct *)arg;
	if(es != NULL)
	{
		if(es->p != NULL)
		{
			tcp_echoserver_send(tpcb, es);
		}
		else if(es->state == ES_CLOSING)
		{
			tcp_echoserver_connection_close(tpcb, es);
		}
		return ERR_OK;
	}
	else
	{
		tcp_abort(tpcb);
		return ERR_ABRT;
	}
}

static err_t tcp_echoserver_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	tcp_setprio(newpcb, TCP_PRIO_MIN);
	struct tcp_echoserver_struct *es = (struct tcp_echoserver_struct *)mem_malloc(sizeof(struct tcp_echoserver_struct));
	if(es != NULL)
	{
		es->state = ES_ACCEPTED;
		es->pcb = newpcb;
		es->p = NULL;

		tcp_arg(newpcb, es);
		tcp_recv(newpcb, tcp_echoserver_recv);
		tcp_err(newpcb, tcp_echoserver_error);
		tcp_poll(newpcb, tcp_echoserver_poll, 1);

		return ERR_OK;
	}
	else
	{
		tcp_echoserver_connection_close(newpcb, es);
		return ERR_MEM;
	}
}

int sock_srv_init(struct tcp_pcb *inst, int port)
{
	inst = tcp_new();
	if(inst != NULL)
	{
		err_t err = tcp_bind(inst, IP_ADDR_ANY, port);

		if(err == ERR_OK)
		{
			inst = tcp_listen(inst);
			tcp_accept(inst, tcp_echoserver_accept);
			return 0;
		}
		else
		{
			memp_free(MEMP_TCP_PCB, inst);
			return 1;
		}
	}
	else
	{
		return 2; // Can not create new pcb
	}
}