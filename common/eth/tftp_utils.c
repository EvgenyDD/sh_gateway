#include "tftp_utils.h"
#include <string.h>

/**
 * @brief  Extracts the opcode from a TFTP message
 **/
tftp_opcode tftp_decode_op(char *buf)
{
	return (tftp_opcode)(buf[1]);
}

/**
 * @brief Extracts the block number from TFTP message
 **/
u16_t tftp_extract_block(char *buf)
{
	uint16_t b;
	memcpy(&b, &buf[2], sizeof(b));
	return ntohs(b);
}

/**
 * @brief Extracts the filename from TFTP message
 **/
void tftp_extract_filename(char *fname, char *buf)
{
	strcpy(fname, buf + 2);
}

/**
 * @brief set the opcode in TFTP message: RRQ / WRQ / DATA / ACK / ERROR
 **/
void tftp_set_opcode(char *buffer, tftp_opcode opcode)
{

	buffer[0] = 0;
	buffer[1] = (u8_t)opcode;
}

/**
 * @brief Set the errorcode in TFTP message
 **/
void tftp_set_errorcode(char *buffer, tftp_errorcode errCode)
{

	buffer[2] = 0;
	buffer[3] = (u8_t)errCode;
}

/**
 * @brief Sets the error message
 **/
void tftp_set_errormsg(char *buffer, const char *errormsg)
{
	strcpy(buffer + 4, errormsg);
}

/**
 * @brief Sets the block number
 **/
void tftp_set_block(char *packet, u16_t block)
{
	uint16_t b = htons(block);
	memcpy(&packet[2], &b, sizeof(b));
}

/**
 * @brief Set the data message
 **/
void tftp_set_data_message(char *packet, char *buf, uint32_t buflen)
{
	memcpy(packet + 4, buf, buflen);
}

/**
 * @brief Check if the received acknowledgement is correct
 **/
u32_t tftp_is_correct_ack(char *buf, int block)
{
	/* first make sure this is a data ACK packet */
	if(tftp_decode_op(buf) != TFTP_ACK)
		return 0;

	/* then compare block numbers */
	if(block != tftp_extract_block(buf))
		return 0;

	return 1;
}
