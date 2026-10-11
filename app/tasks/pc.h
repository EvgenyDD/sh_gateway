#ifndef PC_H__
#define PC_H__

#include <stdbool.h>
#include <stdint.h>

void pc_init(void);
void pc_poll(uint32_t diff_ms);

bool pc_is_on(void);
void pc_switch(void);

#endif // PC_H__