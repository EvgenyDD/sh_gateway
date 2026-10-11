#ifndef MQTT_APP_H__
#define MQTT_APP_H__

#include <stdint.h>
#include <stdbool.h>

enum
{
	STATE_OFF = 0,
	STATE_OUT_OF_MEM,
	STATE_CONN_RETRY,
	STATE_CONNECTED,
};

#define QOS0 0 // no ack
#define QOS1 1 // with ack

#define RETAIN_NO 0	 // not retainde on MQTT server
#define RETAIN_YES 1 // will be retained on MQTT server

typedef struct
{
	int status;
	int conn_code;
	int conn_cb_cnt;
	uint32_t tx_q_ovf;
	uint32_t err_publ;
} mqtt_dbg_t;

void mqtt_app_init_client(void);
int mqtt_queue_publish(const char *topic, const void *payload, uint16_t len, uint8_t qos, uint8_t retain);

extern mqtt_dbg_t mqtt_dbg;

#endif // MQTT_APP_H__