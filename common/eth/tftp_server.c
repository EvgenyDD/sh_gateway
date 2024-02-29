#include "tftp_server.h"
#include "console_udp.h"
#include "platform.h"
#include "stm32f4xx.h"
#include "tftp_utils.h"
#include <string.h>

extern bool g_stay_in_boot;
extern tftp_cb_t tftp_cb;

#define TFTP_PORT 69

typedef struct
{
	int op; /* RRQ/WRQ */

	/* last block read */
	char data[TFTP_DATA_PKT_LEN_MAX];
	uint32_t data_len;

	/* destination ip:port */
	struct ip_addr to_ip;
	int to_port;

	/* next block number */
	int block;

	/* total number of bytes transferred */
	uint32_t tot_bytes;
} tftp_connection_args;

struct udp_pcb *UDPpcb;

const char *tftp_errorcode_string[] = {
	"not defined",
	"file not found",
	"access violation",
	"disk full",
	"illegal operation",
	"unknown transfer id",
	"file already exists",
	"no such user",
};

static void recv_callback_tftp(void *arg, struct udp_pcb *upcb, struct pbuf *pkt_buf, struct ip_addr *addr, u16_t port);

static void tftp_cleanup_rd(struct udp_pcb *upcb, tftp_connection_args *args)
{
	mem_free(args);
	udp_disconnect(upcb);
	udp_remove(upcb);
	udp_recv(UDPpcb, recv_callback_tftp, NULL);
}

static void tftp_cleanup_wr(struct udp_pcb *upcb, tftp_connection_args *args)
{
	mem_free(args);
	udp_disconnect(upcb);
	udp_remove(upcb);
	udp_recv(UDPpcb, recv_callback_tftp, NULL);
}

static uint32_t tftp_construct_error_message(char *buf, tftp_errorcode err)
{
	tftp_set_opcode(buf, TFTP_ERROR);
	tftp_set_errorcode(buf, err);
	const char *s = err >= sizeof(tftp_errorcode_string) / sizeof(tftp_errorcode_string[0]) ? "unknown" : tftp_errorcode_string[err];
	tftp_set_errormsg(buf, s);
	return 4 + strlen(s) + 1;
}

static err_t tftp_send_message(struct udp_pcb *upcb, struct ip_addr *to_ip, unsigned short to_port, char *buf, unsigned short buflen)
{
	struct pbuf *pkt_buf = pbuf_alloc(PBUF_TRANSPORT, buflen, PBUF_POOL);
	if(!pkt_buf) return ERR_MEM;
	memcpy(pkt_buf->payload, buf, buflen);
	err_t err = udp_sendto(upcb, pkt_buf, to_ip, to_port);
	pbuf_free(pkt_buf);
	return err;
}

static int tftp_send_error_message(struct udp_pcb *upcb, struct ip_addr *to, int to_port, tftp_errorcode err)
{
	PRINTF("TFTP abt: %d\n", err);
	char buf[512];
	uint32_t error_len = tftp_construct_error_message(buf, err);
	return tftp_send_message(upcb, to, to_port, buf, error_len);
}

static int tftp_send_ack_packet(struct udp_pcb *upcb, struct ip_addr *to, int to_port, unsigned short block)
{
	char packet[TFTP_ACK_PKT_LEN];
	tftp_set_opcode(packet, TFTP_ACK);

	/* Specify the block number being ACK'd.
	 * If we are ACK'ing a DATA pkt then the block number echoes that of the DATA pkt being ACK'd (duh)
	 * If we are ACK'ing a WRQ pkt then the block number is always 0
	 * RRQ packets are never sent ACK pkts by the server, instead the server sends DATA pkts to the
	 * host which are, obviously, used as the "acknowledgement" */
	tftp_set_block(packet, block);
	return tftp_send_message(upcb, to, to_port, packet, TFTP_ACK_PKT_LEN);
}

/**
 * @brief  receive callback during tftp write operation (not on standard port 69)
 * @param  arg: pointer on argument passed to callback
 * @param  udp_pcb: pointer on the udp pcb
 * @param  pkt_buf: pointer on the received pbuf
 * @param  addr: pointer on remote IP address
 * @param  port: pointer on remote port
 * @retval None
 */
static void wrq_recv_callback(void *arg, struct udp_pcb *upcb, struct pbuf *pkt_buf, struct ip_addr *addr, u16_t port)
{
	tftp_connection_args *args = (tftp_connection_args *)arg;

	/* we expect to receive only one pbuf (pbuf size should be configured > max TFTP frame size */
	if(pkt_buf->len != pkt_buf->tot_len) return;

	/* Does this packet have any valid data to write? */
	if((pkt_buf->len > TFTP_DATA_PKT_HDR_LEN) &&
	   (tftp_extract_block(pkt_buf->payload) == (args->block + 1)))
	{
		int sts = tftp_cb.write(args->tot_bytes, (uint8_t *)pkt_buf->payload + TFTP_DATA_PKT_HDR_LEN, pkt_buf->len - TFTP_DATA_PKT_HDR_LEN);
		if(sts)
		{
			tftp_send_error_message(upcb, addr, port, sts + 10);
			tftp_cleanup_wr(upcb, args); /* close the connection */
		}

		args->block++;
		(args->tot_bytes) += (pkt_buf->len - TFTP_DATA_PKT_HDR_LEN);
	}
	else if(tftp_extract_block(pkt_buf->payload) == (args->block + 1))
	{
		args->block++;
	}

	/* Send the appropriate ACK pkt (the block number sent in the ACK pkt echoes
	 * the block number of the DATA pkt we just received - see RFC1350)
	 * NOTE!: If the DATA pkt we received did not have the appropriate block
	 * number, then the args->block (our block number) is never updated and
	 * we simply send "duplicate ACK" which has the same block number as the
	 * last ACK pkt we sent.  This lets the host know that we are still waiting
	 * on block number args->block+1.
	 */
	tftp_send_ack_packet(upcb, addr, port, args->block);

	/* If the last write returned less than the maximum TFTP data pkt length,
	 * then we've received the whole file and so we can quit (this is how TFTP
	 * signals the end of a transfer!)
	 */
	if(pkt_buf->len < TFTP_DATA_PKT_LEN_MAX)
	{
		tftp_cb.write(args->tot_bytes, NULL, 0);
		tftp_cleanup_wr(upcb, args);
		pbuf_free(pkt_buf);
	}
	else
	{
		pbuf_free(pkt_buf);
		return;
	}
}

/**
 * @param  upcb: pointer on a udp pcb
 * @param  args: pointer on structure of type tftp_connection_args
 * @param  to_ip: pointer on remote IP address
 * @param  to_port: pointer on remote udp port
 * @retval None
 */
static void tftp_rd_send_next_block(struct udp_pcb *upcb, tftp_connection_args *args, struct ip_addr *to_ip, u16_t to_port)
{
	int sts = tftp_cb.read(args->tot_bytes, (uint8_t *)args->data, TFTP_DATA_LEN_MAX, &args->data_len);
	if(sts)
	{
		tftp_send_error_message(upcb, to_ip, to_port, TFTP_ERR_ILLEGALOP);
		tftp_cleanup_rd(upcb, args);
	}

	char packet[TFTP_DATA_PKT_LEN_MAX]; /* (512+4) bytes */

	args->tot_bytes += args->data_len;
	tftp_set_opcode(packet, TFTP_DATA);
	tftp_set_block(packet, args->block);
	tftp_set_data_message(packet, args->data, args->data_len);
	tftp_send_message(upcb, to_ip, to_port, packet, args->data_len + 4);
}

/**
 * @brief  receive callback during tftp read operation (not on standard port 69)
 * @param  arg: pointer on argument passed to callback
 * @param  udp_pcb: pointer on the udp pcb
 * @param  pkt_buf: pointer on the received pbuf
 * @param  addr: pointer on remote IP address
 * @param  port: pointer on remote udp port
 * @retval None
 */
static void rrq_recv_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, struct ip_addr *addr, u16_t port)
{
	tftp_connection_args *args = (tftp_connection_args *)arg;

	if(tftp_is_correct_ack(p->payload, args->block))
	{
		/* increment block # */
		args->block++;
	}
	else
	{
		/* we did not receive the expected ACK, so do not update block #. This causes the current block to be resent. */
	}

	/* if the last read returned less than the requested number of bytes (i.e. TFTP_DATA_LEN_MAX), then we've sent the whole file and we can quit */
	if(args->data_len < TFTP_DATA_LEN_MAX)
	{
		tftp_cleanup_rd(upcb, args);
		pbuf_free(p);
	}

	/* if the whole file has not yet been sent then continue  */
	tftp_rd_send_next_block(upcb, args, addr, port);

	pbuf_free(p);
}

/**
 * @brief  processes tftp read operation
 * @param  upcb: pointer on udp pcb
 * @param  to: pointer on remote IP address
 * @param  to_port: pointer on remote udp port
 * @param  file_name: pointer on filename to be read
 * @retval error code
 */
static int tftp_process_read(struct udp_pcb *upcb, struct ip_addr *to, unsigned short to_port, char *file_name)
{
	tftp_connection_args *args = NULL;

	if(tftp_server_check_file(false, file_name))
	{
		tftp_send_error_message(upcb, to, to_port, TFTP_ERR_FILE_NOT_FOUND);
		tftp_cleanup_wr(upcb, args);
		return 0;
	}

	args = mem_malloc(sizeof *args);
	if(!args)
	{
		tftp_send_error_message(upcb, to, to_port, TFTP_ERR_NOTDEFINED);
		tftp_cleanup_rd(upcb, args);
		return 0;
	}

	/* initialize connection structure  */
	args->op = TFTP_RRQ;
	args->to_ip.addr = to->addr;
	args->to_port = to_port;
	args->block = 1; /* block number starts at 1 (not 0) according to RFC1350  */
	args->tot_bytes = 0;

	/* set callback for receives on this UDP PCB  */
	udp_recv(upcb, rrq_recv_callback, args);

	/* initiate the transaction by sending the first block of data, further blocks will be sent when ACKs are received */
	tftp_rd_send_next_block(upcb, args, to, to_port);

	return 1;
}

/**
 * @brief  processes tftp write operation
 * @param  upcb: pointer on upd pcb
 * @param  to: pointer on remote IP address
 * @param  to_port: pointer on remote udp port
 * @param  file_name: pointer on filename to be written
 * @retval error code
 */
static int tftp_process_write(struct udp_pcb *upcb, struct ip_addr *to, unsigned short to_port, char *file_name)
{
	tftp_connection_args *args = NULL;

	/* Can not create file */
	if(tftp_server_check_file(true, file_name))
	{
		tftp_send_error_message(upcb, to, to_port, TFTP_ERR_NOTDEFINED);
		tftp_cleanup_wr(upcb, args);
		return 0;
	}

	args = mem_malloc(sizeof *args);
	if(!args)
	{
		tftp_send_error_message(upcb, to, to_port, TFTP_ERR_NOTDEFINED);
		tftp_cleanup_wr(upcb, args);
		return 0;
	}

	args->op = TFTP_WRQ;
	args->to_ip.addr = to->addr;
	args->to_port = to_port;
	/* the block # used as a positive response to a WRQ is _always_ 0!!! (see RFC1350)  */
	args->block = 0;
	args->tot_bytes = 0;

	udp_recv(upcb, wrq_recv_callback, args);

	tftp_send_ack_packet(upcb, to, to_port, args->block);

	return 0;
}

static void process_tftp_request(struct pbuf *pkt_buf, struct ip_addr *addr, u16_t port)
{
	tftp_opcode op = tftp_decode_op(pkt_buf->payload);
	char file_name[32];

	struct udp_pcb *upcb = udp_new();
	if(!upcb) return;

	/* bind to port 0 to receive next available free port */
	/* NOTE:  This is how TFTP works.  There is a UDP PCB for the standard port
	 * 69 which al transactions begin communication on, however all subsequent
	 * transactions for a given "stream" occur on another port!  */
	err_t err = udp_bind(upcb, IP_ADDR_ANY, 0);
	if(err != ERR_OK) return;

	switch(op)
	{
	case TFTP_RRQ:
		tftp_extract_filename(file_name, pkt_buf->payload);
		tftp_process_read(upcb, addr, port, file_name);
		break;

	case TFTP_WRQ: /* TFTP WRQ (write request)   */
		tftp_extract_filename(file_name, pkt_buf->payload);
		tftp_process_write(upcb, addr, port, file_name);
		break;

	default:
		tftp_send_error_message(upcb, addr, port, TFTP_ERR_ACCESS_VIOLATION);
		udp_remove(upcb);
		break;
	}
}

static void recv_callback_tftp(void *arg, struct udp_pcb *upcb, struct pbuf *pkt_buf, struct ip_addr *addr, u16_t port)
{
	g_stay_in_boot = true;
	process_tftp_request(pkt_buf, addr, port);
	pbuf_free(pkt_buf);
}

int tftpd_init(void)
{
	UDPpcb = udp_new();
	if(!UDPpcb) return -1;

	err_t err = udp_bind(UDPpcb, IP_ADDR_ANY, TFTP_PORT);
	if(err != ERR_OK) return -2;

	udp_recv(UDPpcb, recv_callback_tftp, NULL);
	return 0;
}
