#include "sock_srv.h"
#include "lwip/debug.h"
#include "lwip/stats.h"

#include "console_udp.h"

extern void cb_srv_sock_rx(struct tcp_pcb *tpcb, const struct pbuf *p);

static struct tcp_pcb *tcp_echoserver_pcb;

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
		tcp_output(tpcb);

		if(wr_err == ERR_OK)
		{
			u16_t plen = ptr->len;
			// _PRINTF("... x%x x%x | %d\n", ptr, ptr->next, ptr->ref);
			es->p = ptr->next;
			if(es->p != NULL)
			{
				_PRINTF("pb ref\n");
				pbuf_ref(es->p);
			}
			int rc = pbuf_free(ptr); // free pbuf: will free pbufs up to es->p (because es->p has a reference count > 0)
									 // _PRINTF("\tdestroy x%x unref %d\n", ptr, rc);
									 // tcp_recved(tpcb, plen); // Update tcp window size to be advertized : should be called when received data (with the amount plen) has been processed by the application layer
		}
		else if(wr_err == ERR_MEM)
		{
			_PRINTF("ERR MEM\n");
			es->p = ptr; // we are low on memory, try later / harder, defer to poll
		}
		else
		{
			_PRINTF("other prblm\n");
			// other problem ??
		}
	}
}

static void tcp_echoserver_connection_close(struct tcp_pcb *tpcb, struct tcp_echoserver_struct *es)
{
	_PRINTF("CLOSING!\n");
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
	if(p == NULL) /* if we receive an empty tcp frame from client => close connection */
	{
		es->state = ES_CLOSING;
		if(es->p == NULL)
		{
			tcp_echoserver_connection_close(tpcb, es);
		}
		else /* we're not done yet */
		{
			tcp_sent(tpcb, tcp_echoserver_sent); /* acknowledge received packet */
			tcp_echoserver_send(tpcb, es);		 /* send remaining data */
		}
		return ERR_OK;
	}
	else if(err != ERR_OK) // a non empty frame was received from client but for some reason err != ERR_OK
	{
		_PRINTF("WTF111!\n");
		es->p = NULL;
		pbuf_free(p);
		return err;
	}
	else if(es->state == ES_ACCEPTED)
	{
		es->state = ES_RECEIVED; /* first data chunk in p->payload */
		// es->p = p;							 /* store reference to incoming pbuf (chain) */
		tcp_sent(tpcb, tcp_echoserver_sent); /* initialize LwIP tcp_sent callback function */
		cb_srv_sock_rx(tpcb, p);
		pbuf_free(p);
		// tcp_echoserver_send(tpcb, es);		 /* send back the received data (echo) */
		return ERR_OK;
	}
	else if(es->state == ES_RECEIVED) /* more data received from client and previous data has been already sent*/
	{
		cb_srv_sock_rx(tpcb, p);
		pbuf_free(p);
		// if(es->p == NULL)
		// {
		// 	es->p = p;
		// 	tcp_echoserver_send(tpcb, es); /* send back received data */
		// }
		// else
		// {
		// 	struct pbuf *ptr = es->p; /* chain pbufs to the end of what we recv'ed previously  */
		// 	pbuf_chain(ptr, p);
		// }
		return ERR_OK;
	}
	else // data received when connection already closed
	{
		_PRINTF("CLOSED!\n");
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

		// _PRINTF(":PCB:x%x (addr x%x x%x)\n", newpcb, newpcb->remote_ip, newpcb->local_ip);
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

int sock_srv_send(struct tcp_pcb *pcb, const uint8_t *data, size_t len)
{
	if(pcb == NULL) return -1;

	struct tcp_echoserver_struct *es = (struct tcp_echoserver_struct *)pcb->callback_arg;
	if(es == NULL) return -2;

	struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_POOL);
	if(!p)
	{
		// _PRINTF("pbuf alloc fail | %d x%x\n", len, p);
		return -3;
	}

	int sts = pbuf_take(p, data, len);
	if(sts)
	{
		// _PRINTF("pbuf take fail\n");
		return sts;
	}

	if(es->p == NULL)
	{
		es->p = p;
		tcp_echoserver_send(pcb, es);
	}
	else
	{
		pbuf_chain(es->p, p);
		// _PRINTF("chain\n");
	}
	return 0;
}
