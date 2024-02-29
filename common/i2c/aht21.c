#include "aht21.h"
#include "i2c_common.h"
#include <stdbool.h>
#include <stddef.h>

#define POLL_HALF_INTERVAL 100

extern void delay_ms(volatile uint32_t delay_ms);
aht21_t aht21_data = {0};

#define AHT21_ADDR 0x38

#define AHT21_CMD_TRIG_MEAS 0xAC
#define AHT21_CMD_RESET 0xBA
#define AHT21_CMD_INIT 0xBE

static bool is_wait_conv = false;
static uint32_t tmr_poll = 0;

int aht21_read_status(uint8_t *status) { return i2c_read(AHT21_ADDR, status, 1, false); }
int aht21_reset(void) { return i2c_write(AHT21_ADDR, (uint8_t[]){AHT21_CMD_RESET}, 1, false); }
int aht21_is_present(void) { return i2c_write(AHT21_ADDR, NULL, 0, false); }

static void reset_reg(uint8_t reg)
{
	i2c_write(AHT21_ADDR, (uint8_t[]){reg, 0x00, 0x00}, 3, false);
	delay_ms(5);

	uint8_t data[3];
	i2c_read(AHT21_ADDR, data, 3, false);
	delay_ms(10);

	i2c_write(AHT21_ADDR, (uint8_t[]){0xB0 | reg, data[1], data[2]}, 3, false);
}

int aht21_init(void)
{
	delay_ms(40);

	if(aht21_is_present())
	{
		aht21_data.sensor_present = false;
		return 1;
	}

	aht21_data.sensor_present = true;

	aht21_reset();
	delay_ms(40);

	uint8_t status;
	int sts = aht21_read_status(&status);
	if(sts) return sts;
	if((status & (1 << 3)) != (1 << 3))
	{
		sts = i2c_write(AHT21_ADDR, (uint8_t[]){0xA8, 0x00, 0x00}, 3, false);
		sts = i2c_write(AHT21_ADDR, (uint8_t[]){AHT21_CMD_INIT, 0x08, 0x00}, 3, false);
		delay_ms(10);
	}
	if((status & 0x18) != 0x18)
	{
		reset_reg(0x1b);
		reset_reg(0x1c);
		reset_reg(0x1e);
	}

	return aht21_read_status(&status);
}

int aht21_poll(uint32_t diff_ms)
{
	tmr_poll += diff_ms;
	if(tmr_poll >= POLL_HALF_INTERVAL)
	{
		tmr_poll = 0;
		if(is_wait_conv == false)
		{
			is_wait_conv = true;
			int sts = i2c_write(AHT21_ADDR, (uint8_t[]){AHT21_CMD_TRIG_MEAS, 0x33, 0x00}, 3, false);
			return sts;
		}

		uint8_t data[7];
		int sts = i2c_read(AHT21_ADDR, data, 7, false);
		if(sts) return sts;

		if((data[0] & (1 << 7)) == 0)
		{
			is_wait_conv = false;
			uint32_t var = (((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | (uint32_t)data[3]) >> 4;
			aht21_data.hum = (float)var * 100.0f / 1024.0f / 1024.0f;

			var = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[5];
			aht21_data.temp = (float)var * 200.0f / 1024.0f / 1024.0f - 50.0f;
		}
		return sts;
	}
	return 0;
}
