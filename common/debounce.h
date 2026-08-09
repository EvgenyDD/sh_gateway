#ifndef DEBOUNCE_H
#define DEBOUNCE_H

#include <stdbool.h>
#include <stdint.h>

#define TRIG_THRS 30 // ms
#define TRIG_SET_THRS (TRIG_THRS * 9 / 10)
#define TRIG_CLR_THRS (TRIG_THRS * 1 / 10)

#define TRIG_SET (1 << 0)
#define TRIG_CLR_SHOT (1 << 2)
#define TRIG_SET_SHOT ((1 << 3) | TRIG_SET)

typedef struct
{
	int state;
	int int_state;
	int int_state_prev;
	int cnt_fltr;
	int cnt_press;
	int cfg_release_thrs;
	int cfg_press_thrs;
	int cfg_max_thrs;
} debounce_t;

void debounce_init(debounce_t *inst);
void debounce_update(debounce_t *inst, bool state, uint32_t time_diff);

#endif // DEBOUNCE_H
