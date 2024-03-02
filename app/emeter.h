#ifndef EMETER_H__
#define EMETER_H__

#include <stdint.h>

void emeter_init(void);
void emeter_poll(uint32_t diff_ms);

uint32_t emeter_get_cnt(void);
float emeter_get_power_kw(void);
float emeter_get_energy_kwh(void);

#endif // EMETER_H__