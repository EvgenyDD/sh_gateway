#include "pc.h"
#include "stm32f4xx.h"

#define SWITCH_DELAY_MS 600

#define PC_SW_PRESS() GPIOC->BSRRL = GPIO_Pin_10
#define PC_SW_RELEASE() GPIOC->BSRRH = GPIO_Pin_10

static uint32_t press_tmr = 0;

bool pc_is_on(void) { return GPIOC->IDR & GPIO_Pin_11; }

void pc_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOC, &GPIO_InitStruct);

	PC_SW_RELEASE();

	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void pc_switch(void)
{
	press_tmr = SWITCH_DELAY_MS;
	PC_SW_PRESS();
}

void pc_poll(uint32_t diff_ms)
{
	if(press_tmr != 0)
	{
		if(press_tmr > diff_ms)
		{
			press_tmr -= diff_ms;
		}
		else
		{
			press_tmr = 0;
			PC_SW_RELEASE();
		}
	}
}