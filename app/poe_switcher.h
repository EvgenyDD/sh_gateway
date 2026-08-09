#ifndef POE_SWITCHER_H__
#define POE_SWITCHER_H__

#include <stdbool.h>
#include <stdint.h>

void poe_switcher_init(void);
void poe_switcher_poll(uint32_t diff_ms);

bool poe_switcher_is_on(uint8_t ch);
void poe_switcher_on(uint8_t ch);
void poe_switcher_off(uint8_t ch);

#endif // POE_SWITCHER_H__