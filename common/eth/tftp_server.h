
#ifndef __TFTPSERVER_H_
#define __TFTPSERVER_H_

#include "lwip/mem.h"
#include "lwip/udp.h"
#include <stdbool.h>
#include <stdint.h>

#define TFTP_OPCODE_LEN 2
#define TFTP_BLKNUM_LEN 2
#define TFTP_ERRCODE_LEN 2
#define TFTP_DATA_LEN_MAX 512
#define TFTP_DATA_PKT_HDR_LEN (TFTP_OPCODE_LEN + TFTP_BLKNUM_LEN)
#define TFTP_ERR_PKT_HDR_LEN (TFTP_OPCODE_LEN + TFTP_ERRCODE_LEN)
#define TFTP_ACK_PKT_LEN (TFTP_OPCODE_LEN + TFTP_BLKNUM_LEN)
#define TFTP_DATA_PKT_LEN_MAX (TFTP_DATA_PKT_HDR_LEN + TFTP_DATA_LEN_MAX)

/* TFTP opcodes as specified in RFC1350   */
typedef enum
{
	TFTP_RRQ = 1,
	TFTP_WRQ = 2,
	TFTP_DATA = 3,
	TFTP_ACK = 4,
	TFTP_ERROR = 5
} tftp_opcode;

/* TFTP error codes as specified in RFC1350  */
typedef enum
{
	TFTP_ERR_NOTDEFINED = 0,
	TFTP_ERR_FILE_NOT_FOUND,
	TFTP_ERR_ACCESS_VIOLATION,
	TFTP_ERR_DISKFULL,
	TFTP_ERR_ILLEGALOP,
	TFTP_ERR_UKNOWN_TRANSFER_ID,
	TFTP_ERR_FILE_ALREADY_EXISTS,
	TFTP_ERR_NO_SUCH_USER,
} tftp_errorcode;

typedef struct
{
	int (*write)(uint32_t addr, const uint8_t *data, uint32_t size);
	int (*read)(uint32_t addr, uint8_t *data, uint32_t size_buffer, uint32_t *size_read);
} tftp_cb_t;

int tftpd_init(void);

extern int tftp_server_check_file(bool is_write, char *file_name);

#endif // __TFTPSERVER_H_