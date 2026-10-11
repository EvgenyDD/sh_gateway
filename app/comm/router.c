#include "router.h"
#include "309/CO_gateway_ascii.h"
#include "eth_con/console_udp.h"
#include "mqtt_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

router_t router = {0};
uint32_t pdo_rx_cnt = 0;

int router_add_entry(const char *s)
{
	const char *ptr = s;
	int offset = 0;

	char key[32];
	char val[64];

	if(router.entries_cnt + 1 >= MAX_ROUTES)
	{
		_PRINTF("[E]No size left!\n");
		return -1;
	}
	route_t *r = &router.routes[router.entries_cnt];
	memset(&router.routes[router.entries_cnt], 0, sizeof(route_t));

	// %31[^=] — читать до 31 символа, пока не встретится '='
	// =         — пропустить символ '='
	// %63[^ ]   — читать до 63 символов, пока не встретится пробел
	// %n        — записать количество прочитанных байт в offset
	while(sscanf(ptr, " %31[^=]=%63[^ ]%n", key, val, &offset) == 2)
	{
		if(strcmp(key, "id") == 0)
		{
			int id = atoi(val);
			r->can_id = id;
			_PRINTF("Parsed 'id': %d\n", id);
			if(id <= 1 || id > 127)
			{
				_PRINTF("[E] CAN ID wrong");
				return -3;
			}
		}
		else if(strcmp(key, "off") == 0)
		{
			int off = atoi(val);
			r->offset = off;
			_PRINTF("Parsed 'offset': %d\n", off);
			if(off < 0 || off > 7)
			{
				_PRINTF("[E] offset wrong");
				return -4;
			}
		}
		else if(strcmp(key, "pdo") == 0)
		{
			int pdo_n = atoi(val);
			r->pdo_n = pdo_n;
			_PRINTF("Parsed 'PDO#': %d\n", pdo_n);
			if(pdo_n < 0 || pdo_n > 3)
			{
				_PRINTF("[E] PDO N wrong");
				return -5;
			}
		}
		else if(strcmp(key, "retain") == 0)
		{
			r->retain = true;
			_PRINTF("IS RETAIN\n");
		}
		else if(strcmp(key, "qos") == 0)
		{
			int qos = atoi(val);
			r->qos = qos;
			if(qos < 0 || qos > 2)
			{
				_PRINTF("[E] QOS wrong");
				return -6;
			}
			_PRINTF("Parsed 'qos': %d\n", r->qos);
		}
		else if(strcmp(key, "interval") == 0)
		{
			int interval = atoi(val);
			r->publish_interval_ms = interval;
			_PRINTF("Parsed 'interval': %d\n", interval);
		}
		else if(strcmp(key, "topic") == 0)
		{
			snprintf(r->topic, sizeof(r->topic) - 1, "%s", val);
			_PRINTF("Parsed 'topic': %s\n", r->topic);
		}
		else if(strcmp(key, "type") == 0)
		{
			bool_t err = false;
			const CO_GTWA_dataType_t *dt = CO_GTWA_getDataType(val, &err, &r->type);
			_PRINTF("Parsed 'type': %d | err %d\n", dt != NULL, err);
			if(dt != NULL)
			{
				_PRINTF("\tlen %d\n", dt->length);
			}
			if(dt == NULL) return -7;
		}
		else if(strcmp(key, "is_cmd") == 0)
		{
			r->is_mqtt_to_pdo = true;
			_PRINTF("IS CMD\n");
		}
		else
		{
			_PRINTF("Key not found: %s\n", key);
			return -2; // no key found
		}

		ptr += offset;
	}
	if(r->type == 0 || r->type >= sizeof(dataTypes) / sizeof(dataTypes[0]))
	{
		_PRINTF("Type not set\n");
		return -7;
	}
	if(r->offset + dataTypes[r->type].length > 8)
	{
		_PRINTF("Wrong total offset %d\n", r->offset + dataTypes[r->type].length);
		return -8;
	}

	for(uint32_t i = 0; i < router.entries_cnt; i++)
	{
		if(strcmp(r->topic, router.routes[i].topic) == 0)
		{
			_PRINTF("[E]Topic %s already exist @%ld!\n", r->topic, i);
			return -9;
		}
	}
	router.entries_cnt++;
	return 0;
}

void router_print(void)
{
	_PRINTF("%ld routes\n", router.entries_cnt);
	for(uint32_t i = 0; i < router.entries_cnt; i++)
	{
		_PRINTF("%ld Route %s %s| ID %d | PDO %d(@%d) type %d | Time %ld | last data (pub %ld ms.): ",
				i, router.routes[i].topic,
				router.routes[i].is_mqtt_to_pdo ? "is_CMD" : "",
				router.routes[i].can_id,
				router.routes[i].pdo_n,
				router.routes[i].offset,
				router.routes[i].type,
				router.routes[i].publish_interval_ms,
				router.routes[i].last_pub_td);
		for(uint8_t j = 0; j < dataTypes[router.routes[i].type].length; j++)
		{
			_PRINTF("%x ", router.routes[i].data[j]);
		}
		_PRINTF("\n");
	}
}

static void check_try_send(uint32_t it)
{
	if(router.routes[it].publish_interval_ms > 0) // on time (min)
	{
		if(router.routes[it].last_pub_td < (uint32_t)router.routes[it].publish_interval_ms) return; // not send too frequent
	}

	int sts = mqtt_queue_publish(router.routes[it].topic,
								 router.routes[it].data, dataTypes[router.routes[it].type].length,
								 router.routes[it].qos,
								 router.routes[it].retain);
	if(sts >= 0)
	{
		router.routes[it].flag_new_msg = false;
		router.routes[it].last_pub_td = 0;
	}
}

void router_add_pdo(const can_msg_t *msg, uint8_t rpdo_n)
{
	pdo_rx_cnt++;
	for(uint32_t i = 0; i < router.entries_cnt; i++)
	{
		if(router.routes[i].pdo_n == rpdo_n &&
		   router.routes[i].can_id == (msg->id.std & 0x7F))
		{
			uint8_t l = dataTypes[router.routes[i].type].length;
			memcpy(router.routes[i].data, &msg->data[router.routes[i].offset], l);

			if(router.routes[i].publish_interval_ms < 0) // on change
			{
				if(memcmp(router.routes[i].data, router.routes[i].data_prev, l) == 0) continue; // not set new msg flag if no change
			}
			memcpy(router.routes[i].data_prev, router.routes[i].data, l);

			router.routes[i].flag_new_msg = true;
			check_try_send(i);
		}
	}
}

void router_poll(uint32_t diff_ms)
{
	for(uint32_t i = 0; i < router.entries_cnt; i++)
	{
		router.routes[i].last_pub_td += diff_ms;
		if(router.routes[i].flag_new_msg) check_try_send(i);
	}
}