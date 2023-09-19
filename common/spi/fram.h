#ifndef FRAM_H__
#define FRAM_H__

#include <stdint.h>

#define FRAM_SIZE 131072

enum
{
	FRAM_ERR_ADDR_OVF = -100,
};

int fram_init(void);

int fram_read(uint32_t addr, uint8_t *data, uint32_t size);
int fram_write(uint32_t addr, const uint8_t *data, uint32_t size);

uint8_t fram_read_sts(void);
int fram_check_sn(void);

#endif // FRAM_H__