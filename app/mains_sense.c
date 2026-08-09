#include "mains_sense.h"
#include "platform.h"

extern uint32_t g_mains_sns_chnl_bits;

#define NUM_CH 32

#define TIME_ON_MS 25
#define TIME_OFF_MS 500

static uint32_t pins_sns[NUM_CH] = {GPIO_Pin_3};
static GPIO_TypeDef *port_sns[NUM_CH] = {GPIOE};
static uint32_t pins_ctrl[NUM_CH] = {GPIO_Pin_7};
static GPIO_TypeDef *port_ctrl[NUM_CH] = {GPIOD};

static struct
{
	uint32_t tmr_off;
	uint32_t tmr_on;
	bool on_phase;
	bool on;
	uint32_t cnt_on;
	uint32_t cnt_hit;
	uint32_t cnt_on_max;
	uint32_t cnt_on_max_hit;
} inst[NUM_CH] = {0};

void mains_stat_rst(uint8_t ch)
{
	if(ch >= NUM_CH) return;
	inst[ch].cnt_on_max = 0;
}

uint32_t mains_stat_get_cnt(uint8_t ch)
{
	if(ch >= NUM_CH) return 0;
	return inst[ch].cnt_on_max;
}

uint32_t mains_stat_get_hit(uint8_t ch)
{
	if(ch >= NUM_CH) return 0;
	return inst[ch].cnt_on_max_hit;
}

void mains_sense_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;

	for(uint32_t i = 0; i < 32; i++)
	{
		if(g_mains_sns_chnl_bits & (1 << i))
		{
			if(pins_sns[i] && port_sns[i] && pins_ctrl[i] && port_ctrl[i])
			{
				GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
				GPIO_InitStruct.GPIO_Pin = pins_sns[i];
				GPIO_Init(port_sns[i], &GPIO_InitStruct);

				GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
				GPIO_InitStruct.GPIO_Pin = pins_ctrl[i];
				GPIO_Init(port_ctrl[i], &GPIO_InitStruct);
			}
		}
	}
}

bool mains_sense_is_on(uint8_t ch)
{
	if(ch >= NUM_CH) return false;
	return inst[ch].on;
}

void mains_sense_poll(uint32_t diff_ms)
{
	for(uint32_t i = 0; i < 32; i++)
	{
		if(g_mains_sns_chnl_bits & (1 << i))
		{
			if(inst[i].on_phase)
			{
				inst[i].tmr_on += diff_ms;
				if(!(port_sns[i]->IDR & pins_sns[i])) inst[i].cnt_on++;

				if(inst[i].tmr_on >= TIME_ON_MS)
				{
					inst[i].tmr_on = 0;
					inst[i].on = inst[i].cnt_on > 0;
					if(inst[i].cnt_on_max < inst[i].cnt_on)
					{
						inst[i].cnt_on_max = inst[i].cnt_on;
						inst[i].cnt_on_max_hit = inst[i].cnt_hit;
					}
					inst[i].on_phase = false;
					port_ctrl[i]->BSRRH = pins_ctrl[i]; // off
				}
			}
			else
			{
				inst[i].tmr_off += diff_ms;
				if(inst[i].tmr_off >= TIME_OFF_MS)
				{
					inst[i].tmr_off = 0;
					inst[i].on_phase = true;
					inst[i].cnt_on = 0;
					inst[i].cnt_hit = 0;
					port_ctrl[i]->BSRRL = pins_ctrl[i]; // on
				}
			}
		}
	}
}
