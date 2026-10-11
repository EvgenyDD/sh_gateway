#ifndef LOAD_SWITCHER_H__
#define LOAD_SWITCHER_H__

#include <stdbool.h>
#include <stdint.h>

void load_switcher_init(void);
int load_switcher_poll(uint32_t diff_ms);

void load_switcher_on(void);
void load_switcher_off(void);

bool load_switcher_is_on(void);

#endif // LOAD_SWITCHER_H__