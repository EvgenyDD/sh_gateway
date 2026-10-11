#ifndef PDO_ROUTER_H__
#define PDO_ROUTER_H__

#include "can_driver.h"
#include "platform.h"

void pdo_switch_en_global(bool value);
void pdo_switch_poll(void);
bool pdo_switch_state(void);

#endif // PDO_ROUTER_H__