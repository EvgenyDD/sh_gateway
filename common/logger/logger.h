#ifndef LOGGER_H__
#define LOGGER_H__

#include <stddef.h>

void logger_init(void);
void logger_erase(void);
void logger_log(const char *msg);
size_t logger_get_count(void);
void logger_get_entry(size_t n);

#endif // LOGGER_H__