#ifndef LOAD_SWITCHER_H__
#define LOAD_SWITCHER_H__

#include <stdint.h>

void load_switcher_init(void);
int load_switcher_poll(uint32_t diff_ms);

void load_switcher_on(void);
void load_switcher_off(void);

#endif // LOAD_SWITCHER_H__