#include "fram.h"
#include "spi_common.h"
#include "stm32f4xx.h"
#include <string.h>

#define CS_H GPIOD->BSRRL = GPIO_Pin_8
#define CS_L GPIOD->BSRRH = GPIO_Pin_8

#define FRAM_CMD_WR_EN 0x06
#define FRAM_CMD_WR_DI 0x04

#define FRAM_CMD_RD_SR 0x05
#define FRAM_CMD_WR_SR 0x01

#define FRAM_CMD_READ 0x03
#define FRAM_CMD_FSTRD 0x0B
#define FRAM_CMD_WRITE 0x02
#define FRAM_CMD_SLEEP 0xB9
#define FRAM_CMD_RDID 0x9F
#define FRAM_CMD_SNR 0xC3

fram_entries_t fram_data;

int fram_init(void)
{
	// D8 - CS
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOD, &GPIO_InitStruct);
	CS_H;

	int sts = fram_check_sn();
	if(sts) return sts; // device not present

	uint8_t data_tx[2], data_rx[2];
	data_tx[0] = FRAM_CMD_WR_EN;
	CS_L;
	spi_trx(data_tx, data_rx, 1);
	CS_H;

	// disable write protection
	data_tx[0] = FRAM_CMD_WR_SR;
	CS_L;
	spi_trx(data_tx, data_rx, 1);
	CS_H;

	return 0;
}

void fram_data_read(void)
{
	uint8_t data_rx[128];
	for(uint32_t i = 0; i * sizeof(data_rx) < sizeof(fram_data); i++)
	{
		fram_read(i * sizeof(data_rx), data_rx, sizeof(data_rx));
		memcpy(&(((uint8_t *)&fram_data)[i * sizeof(data_rx)]),
			   data_rx,
			   (i + 1) * sizeof(data_rx) > sizeof(fram_data)
				   ? (i + 1) * sizeof(data_rx) - sizeof(fram_data)
				   : sizeof(data_rx));
	}
}

void fram_data_write(void)
{
	uint8_t data_tx[128];
	for(uint32_t i = 0; i * sizeof(data_tx) < sizeof(fram_data); i++)
	{
		size_t q = (i + 1) * sizeof(data_tx) > sizeof(fram_data)
					   ? (i + 1) * sizeof(data_tx) - sizeof(fram_data)
					   : sizeof(data_tx);
		memcpy(data_tx,
			   &(((uint8_t *)&fram_data)[i * sizeof(data_tx)]),
			   q);
		fram_write(i * sizeof(data_tx), data_tx, q);
	}
}

uint8_t fram_read_sts(void)
{
	uint8_t data_tx[2] = {FRAM_CMD_RD_SR}, data_rx[2];
	CS_L;
	spi_trx(data_tx, data_rx, 2);
	CS_H;
	return data_rx[1];
}

int fram_check_sn(void)
{
	uint8_t data_tx[10] = {FRAM_CMD_RDID}, data_rx[10];
	CS_L;
	spi_trx(data_tx, data_rx, 10);
	CS_H;

	return (data_rx[1] == 0x7f &&
			data_rx[2] == 0x7f &&
			data_rx[3] == 0x7f &&
			data_rx[4] == 0x7f &&
			data_rx[5] == 0x7f &&
			data_rx[6] == 0x7f &&
			data_rx[7] == 0xc2 &&
			data_rx[8] == 0x24 &&
			data_rx[9] == 0x00)
			   ? 0
			   : 1;
}

int fram_read(uint32_t addr, uint8_t *data, uint32_t size)
{
	if(addr + size >= FRAM_SIZE) return FRAM_ERR_ADDR_OVF;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"
	uint8_t data_tx[4 + size], data_rx[4 + size];
#pragma GCC diagnostic pop

	data_tx[0] = FRAM_CMD_READ;
	data_tx[1] = (addr >> 16) & 0xFF;
	data_tx[2] = (addr >> 8) & 0xFF;
	data_tx[3] = addr & 0xFF;
	CS_L;
	spi_trx(data_tx, data_rx, 4 + size);
	CS_H;
	memcpy(data, &data_rx[4], size);

	return 0;
}

uint8_t stss;

int fram_write(uint32_t addr, const uint8_t *data, uint32_t size)
{
	if(addr + size >= FRAM_SIZE) return FRAM_ERR_ADDR_OVF;

// disable write protection
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"
	uint8_t data_tx[4 + size], data_rx[4 + size];
#pragma GCC diagnostic pop

	data_tx[0] = FRAM_CMD_WR_EN;
	CS_L;
	spi_trx(data_tx, data_rx, 1);
	CS_H;

	stss = fram_read_sts();

	data_tx[0] = FRAM_CMD_WRITE;
	data_tx[1] = (addr >> 16) & 0xFF;
	data_tx[2] = (addr >> 8) & 0xFF;
	data_tx[3] = addr & 0xFF;
	memcpy(&data_tx[4], data, size);
	CS_L;
	spi_trx(data_tx, data_rx, 4 + size);
	CS_H;

	return 0;
}