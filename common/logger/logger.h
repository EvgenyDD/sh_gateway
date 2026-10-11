#ifndef LOGGER_H__
#define LOGGER_H__

#include <stddef.h>
#include <stdint.h>

uint32_t logger_get_entry_count(void);
void logger_print(const char *format, ...) __attribute__((format(printf, 1, 2)));

// void logger_init(void);
// void logger_erase(void);
// void logger_log(const char *msg);
// size_t logger_get_count(void);
// void logger_get_entry(size_t n);

#endif // LOGGER_H__