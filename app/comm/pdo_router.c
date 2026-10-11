#include "pdo_router.h"
#include "CANopen.h"
#include "OD.h"
#include "load_switcher.h"
#include "router.h"
#include <string.h>

extern CO_t *CO;

#define PDO_LOOP_FREQ 50

static bool pdo_permit = false, pdo_en = false;

static void pdo_enable(bool en)
{
	if(en)
	{
		pdo_en = true;
		OD_PERSIST_COMM.x1006_communicationCyclePeriod = (1000 / PDO_LOOP_FREQ) * 1000; /* us */
		SYNC_write_1005(CO->SYNC, (1UL << 30UL) | 0x80);
	}
	else
	{
		pdo_en = false;
		OD_PERSIST_COMM.x1006_communicationCyclePeriod = 20000; /* us */
		SYNC_write_1005(CO->SYNC, 0x80);
	}
}

void pdo_switch_en_global(bool value)
{
	pdo_permit = value;
	pdo_switch_poll();
}

bool pdo_switch_state(void) { return pdo_permit; }

void pdo_switch_poll(void)
{
	if(pdo_permit && load_switcher_is_on() && pdo_en == false)
	{
		pdo_enable(true);
	}
	if(pdo_en && (load_switcher_is_on() == false || pdo_permit == false))
	{
		pdo_enable(false);
	}
}
