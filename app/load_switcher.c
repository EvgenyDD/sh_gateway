#include "load_switcher.h"
#include "adc.h"
#include "stm32f4xx.h"
#include <math.h>
#include <stdbool.h>

#define TIMEOUT_ON_MS 200
#define TIMEOUT_CUR_LONGTERM_MS 4000
#define CURRENT_FAULT_INSTANT_A 1.0f
#define CURRENT_FAULT_LONGTERM_A 0.25f

static bool enabled = false;
static uint32_t timeout_on_ms = 0;
static uint32_t timeout_cur_longterm_ms = 0;

void load_switcher_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_14;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOE, &GPIO_InitStruct);

	load_switcher_off();

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOE, &GPIO_InitStruct);
}

void load_switcher_on(void)
{
	timeout_on_ms = TIMEOUT_ON_MS;
	enabled = true;
	GPIOE->BSRRL = GPIO_Pin_14;
}

void load_switcher_off(void)
{
	GPIOE->BSRRH = GPIO_Pin_14;
	enabled = false;
}

int load_switcher_poll(uint32_t diff_ms)
{
	timeout_on_ms = timeout_on_ms > diff_ms ? timeout_on_ms - diff_ms : 0;

	if(timeout_on_ms == 0) // fault pin
	{
		if(fabsf(adc_val.i_sns) >=  CURRENT_FAULT_LONGTERM_A)
		{
			timeout_cur_longterm_ms += diff_ms;
		}
		else
		{
			timeout_cur_longterm_ms = 0;
		}

		if(!(GPIOE->IDR & GPIO_Pin_15))
		{
			load_switcher_off();
			return enabled ? 1 : 0;
		}
		if(fabsf(adc_val.i_sns) >= CURRENT_FAULT_INSTANT_A)
		{
			load_switcher_off();
			return enabled ? 2 : 0;
		}
		if(timeout_cur_longterm_ms >= TIMEOUT_CUR_LONGTERM_MS)
		{
			load_switcher_off();
			return enabled ? 3 : 0;
		}
	}
	return 0;
}