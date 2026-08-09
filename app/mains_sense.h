#ifndef MAIN_SENSE_H__
#define MAIN_SENSE_H__

#include <stdbool.h>
#include <stdint.h>

void mains_sense_init(void);
void mains_sense_poll(uint32_t diff_ms);
bool mains_sense_is_on(uint8_t ch);

#endif // MAIN_SENSE_H__