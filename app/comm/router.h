#ifndef ROUTER_H__
#define ROUTER_H__

#include "can_driver.h"
#include <stdbool.h>
#include <stdint.h>

#define MAX_ROUTES 128
#define ROUTE_MAX_DATA_SZ 8
#define PRODUCER_MAX_TOPIC_LEN 20
#define ROUTE_MAX_TOPIC_LEN 40

#define DFLT_QOS 0
#define DFLT_RETAIN 0

// #define DT_HEX 0			 // hex (non standard)
// #define DT_BOOL 1			 // b / BOOLEAN
// #define DT_INT8 2			 // i8 / INTEGER8
// #define DT_INT16 3			 // i16 / INTEGER16
// #define DT_INT32 4			 // i32 / INTEGER32
// #define DT_INT64 5			 // i64 / INTEGER64
// #define DT_UINT8 6			 // u8 / UNSIGNED8
// #define DT_UINT16 7			 // u16 / UNSIGNED16
// #define DT_UINT32 8			 // u32 / UNSIGNED32
// #define DT_UINT64 9			 // u64 / UNSIGNED64
// #define DT_HEX8 10			 // x8 / UNSIGNED8 (hex display)
// #define DT_HEX16 11			 // x16 / UNSIGNED16 (hex display)
// #define DT_HEX32 12			 // x32 / UNSIGNED32 (hex display)
// #define DT_HEX64 13			 // x64 / UNSIGNED64 (hex display)
// #define DT_REAL32 14		 // r32 / REAL32
// #define DT_REAL64 15		 // r64 / REAL64
// #define DT_VISIBLE_STRING 16 // vs / VISIBLE_STRING
// #define DT_OCTET_STRING 17	 // os / OCTET_STRING base64
// #define DT_UNICODE_STRING 18 // us / UNICODE_STRING base64

typedef struct __attribute__((packed))
{
	uint8_t can_id;
	uint8_t pdo_n;
	uint8_t offset;
	uint8_t type;
	// uint8_t retain;
	// uint8_t qos;
	int16_t publish_interval_ms; // <0 - on change, 0 - every time, >0 - interval
	char producer[PRODUCER_MAX_TOPIC_LEN];
} route_cfg_t;

typedef struct
{
	uint8_t data[ROUTE_MAX_DATA_SZ];
	uint8_t data_prev[ROUTE_MAX_DATA_SZ];

	char topic[ROUTE_MAX_TOPIC_LEN];

	// float scale;

	uint32_t last_pub_td;

	// type: on change
	// type: on interval
	// type: deriv
	bool flag_new_msg;
} route_t;

typedef struct router
{
	route_cfg_t route_cfg[MAX_ROUTES];
	route_t routes[MAX_ROUTES];
	uint32_t entries_cnt;
} router_t;

void router_clear_cfg(void);

int router_add_entry(const char *r);
void router_print(void);

void router_init(void);
void router_poll(uint32_t diff_ms);
void router_add_pdo(const can_msg_t *msg, uint8_t rpdo_n);

extern router_t router;

#endif // ROUTER_H__