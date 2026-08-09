#include "debounce.h"
#include <string.h>

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define CLIP(a, l, h) (MAX((MIN((a), (h))), (l)))

void debounce_init(debounce_t *b)
{
	memset(b, 0, sizeof(b));
	b->cfg_max_thrs = TRIG_THRS;
	b->cfg_release_thrs = TRIG_CLR_THRS;
	b->cfg_press_thrs = TRIG_SET_THRS;
}

void debounce_update(debounce_t *b, bool state, uint32_t time_diff)
{
	if(time_diff != 0)
	{
		b->cnt_fltr += (state ? 1 : -1) * (int)time_diff;
		b->cnt_fltr = CLIP(b->cnt_fltr, 0, b->cfg_max_thrs);

		if(b->cnt_fltr > b->cfg_press_thrs)
		{
			b->int_state = TRIG_SET;
			b->cnt_press = CLIP(b->cnt_press + (int)time_diff, 0, b->cfg_press_thrs);
		}
		if(b->cnt_fltr < b->cfg_release_thrs)
		{
			b->int_state = 0;
			b->cnt_press = 0;
		}
	}
	if(b->int_state_prev != b->int_state)
	{
		b->int_state_prev = b->int_state;

		if(b->int_state == TRIG_SET)
			b->state = TRIG_SET_SHOT;
		else if(b->int_state == 0)
			b->state = TRIG_CLR_SHOT;
	}
	else
	{
		b->state = b->int_state;
	}
}
