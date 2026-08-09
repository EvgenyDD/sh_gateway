#include "poe_switcher.h"
#include "adc.h"
#include "stm32f4xx.h"
#include <math.h>
#include <stdbool.h>

#define NUM_CH 8
#define CURRENT_FAULT_A 1.0f

extern uint32_t g_poe_sw_chnl_bits;

static struct
{
	bool on;
} inst[NUM_CH] = {0};

static const uint32_t pins_ctrl[NUM_CH] = {GPIO_Pin_2, GPIO_Pin_7, GPIO_Pin_8, GPIO_Pin_9, GPIO_Pin_10, GPIO_Pin_11, GPIO_Pin_12, GPIO_Pin_9};
static GPIO_TypeDef *const port_ctrl[NUM_CH] = {GPIOB, GPIOE, GPIOE, GPIOE, GPIOE, GPIOE, GPIOE, GPIOD};
static const uint32_t adc_idx[NUM_CH] = {0, 0, 1, 1, 1, 1, 1, 1};

void poe_switcher_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;

	for(uint32_t i = 0; i < NUM_CH; i++)
	{
		if(g_poe_sw_chnl_bits & (1 << i))
		{
			if(pins_ctrl[i] && port_ctrl[i])
			{
				GPIO_InitStruct.GPIO_Pin = pins_ctrl[i];
				GPIO_Init(port_ctrl[i], &GPIO_InitStruct);

				poe_switcher_off(i);
			}
		}
	}
}

bool poe_switcher_is_on(uint8_t ch)
{
	if(ch >= NUM_CH) return false;
	return inst[ch].on;
}

void poe_switcher_on(uint8_t ch)
{
	if(ch >= NUM_CH) return;
	inst[ch].on = true;
	port_ctrl[ch]->BSRRL = pins_ctrl[ch];
}

void poe_switcher_off(uint8_t ch)
{
	if(ch >= NUM_CH) return;
	port_ctrl[ch]->BSRRH = pins_ctrl[ch];
	inst[ch].on = false;
}

void poe_switcher_poll(uint32_t diff_ms)
{
	for(uint32_t i = 0; i < NUM_CH; i++)
	{
		if(g_poe_sw_chnl_bits & (1 << i))
		{
			if(fabsf(adc_val.poe_sns[adc_idx[i]]) >= CURRENT_FAULT_A) poe_switcher_off(i);
		}
	}
}