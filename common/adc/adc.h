#ifndef ANALOG_H__
#define ANALOG_H__

#include <stdint.h>
typedef struct
{
	float vin, vout, ir, i_sns, t_mcu, poe_sns[3];
} adc_val_t;

void adc_init(void);
void adc_track(void);

extern adc_val_t adc_val;

#endif // ANALOG_H__