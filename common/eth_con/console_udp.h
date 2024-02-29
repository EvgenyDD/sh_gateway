#ifndef CONSOLE_UDP_H__
#define CONSOLE_UDP_H__

#include <stdint.h>

#define DT_FMT_S "%ldd %02ld:%02ld:%02ld"
#define DT_FMT_MS "%ldd %02ld:%02ld:%02ld.%03ld"
#define DT_FMT_US "%ldd %02ld:%02ld:%02ld.%03ld.%03ld"

#define DT_FMT_MS_SIMPLE "%02ld:%02ld:%02ld.%03ld"

#define DT_DATA_S(x) (uint32_t)(x / (86400ULL)),           \
					 (uint32_t)((x % 86400ULL) / 3600ULL), \
					 (uint32_t)((x % 3600ULL) / 60ULL),    \
					 (uint32_t)((x % 60ULL))

#define DT_DATA_MS(x) (uint32_t)(x / (86400000ULL)),              \
					  (uint32_t)((x % 86400000ULL) / 3600000ULL), \
					  (uint32_t)((x % 3600000ULL) / 60000ULL),    \
					  (uint32_t)((x % 60000ULL) / 1000ULL),       \
					  (uint32_t)((x % 1000ULL))
#define DT_DATA_US(x) (uint32_t)(x / (86400000000ULL)),                 \
					  (uint32_t)((x % 86400000000ULL) / 3600000000ULL), \
					  (uint32_t)((x % 3600000000ULL) / 60000000ULL),    \
					  (uint32_t)((x % 60000000ULL) / 1000000ULL),       \
					  (uint32_t)((x % 1000000ULL) / 1000ULL),           \
					  (uint32_t)((x % 1000ULL))

#define DT_DATA_MS_SIMPLE(x) (uint32_t)((x % 86400000ULL) / 3600000ULL), \
							 (uint32_t)((x % 3600000ULL) / 60000ULL),    \
							 (uint32_t)((x % 60000ULL) / 1000ULL),       \
							 (uint32_t)((x % 1000ULL))

typedef enum
{
	CON_CB_OK,
	CON_CB_SILENT,
	CON_CB_ERR_CUSTOM,
	CON_CB_ERR_ARGS,
	CON_CB_ERR_BAD_PARAM,
	CON_CB_ERR_UNSAFE,
	CON_CB_ERR_NO_SPACE,
} console_cmd_cb_res_t;

typedef int (*console_cmd_cb_t)(const char *, int);

typedef struct
{
	const char *name;
	console_cmd_cb_t cb;
} console_cmd_t;

extern const console_cmd_t console_cmd[];
extern const uint32_t console_cmd_sz;

void _PRINTF(const char *fmt, ...);
void _PRINT_PREFIX(void);
int console_udp_init(void);

#include "netconf.h"

#define PRINTF(...)  \
	_PRINT_PREFIX(); \
	_PRINTF(__VA_ARGS__);

#endif // CONSOLE_UDP_H__