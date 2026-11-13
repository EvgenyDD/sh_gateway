#include "router.h"
#include "309/CO_gateway_ascii.h"
#include "CANopen.h"
#include "OD.h"
#include "eth_con/console_udp.h"
#include "mqtt_app.h"
#include "platform.h"
#include "sdo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern CO_t *CO;

router_t router = {0};
uint32_t pdo_rx_cnt = 0;

void router_clear_cfg(void)
{
	router.entries_cnt = 0;
	memset(&router, 0, sizeof(router));
}

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
	route_cfg_t *rc = &router.route_cfg[router.entries_cnt];
	memset(&router.routes[router.entries_cnt], 0, sizeof(route_t));

	// %31[^=] - читать до 31 символа, пока не встретится '='
	// =         - пропустить символ '='
	// %63[^ ]   - читать до 63 символов, пока не встретится пробел
	// %n        - записать количество прочитанных байт в offset
	while(sscanf(ptr, " %31[^=]=%63[^ ]%n", key, val, &offset) == 2)
	{
		if(strcmp(key, "id") == 0)
		{
			int id = atoi(val);
			rc->can_id = id;
			_PRINTF("Parsed 'id': %d\n", id);
			if(id == 0 || id > 127)
			{
				_PRINTF("[E] CAN ID wrong\n");
				return -3;
			}
		}
		else if(strcmp(key, "off") == 0)
		{
			int off = atoi(val);
			rc->offset = off;
			_PRINTF("Parsed 'offset': %d\n", off);
			if(off < 0 || off > 7)
			{
				_PRINTF("[E] offset wrong\n");
				return -4;
			}
		}
		else if(strcmp(key, "pdo") == 0)
		{
			int pdo_n = atoi(val);
			rc->pdo_n = pdo_n;
			_PRINTF("Parsed 'PDO#': %d\n", pdo_n);
			if(pdo_n < 0 || pdo_n > 3)
			{
				_PRINTF("[E] PDO N wrong\n");
				return -5;
			}
		}
		// else if(strcmp(key, "retain") == 0)
		// {
		// 	r->retain = true;
		// 	_PRINTF("IS RETAIN\n");
		// }
		// else if(strcmp(key, "qos") == 0)
		// {
		// 	int qos = atoi(val);
		// 	r->qos = qos;
		// 	if(qos < 0 || qos > 2)
		// 	{
		// 		_PRINTF("[E] QOS wrong\n");
		// 		return -6;
		// 	}
		// 	_PRINTF("Parsed 'qos': %d\n", r->qos);
		// }
		else if(strcmp(key, "interval") == 0)
		{
			int interval = atoi(val);
			rc->publish_interval_ms = interval;
			_PRINTF("Parsed 'interval': %d\n", interval);
		}
		else if(strcmp(key, "topic") == 0 && val[0] != 0)
		{
			snprintf(rc->producer, sizeof(rc->producer) - 1, "%s", val);
			r->topic[0] = 0;
			_PRINTF("Parsed 'topic': %s\n", rc->producer);
		}
		else if(strcmp(key, "type") == 0)
		{
			bool_t err = false;
			const CO_GTWA_dataType_t *dt = CO_GTWA_getDataType(val, &err, &rc->type);
			_PRINTF("Parsed 'type': %d | err %d\n", dt != NULL, err);
			if(dt != NULL)
			{
				_PRINTF("\tlen %d\n", dt->length);
			}
			if(dt == NULL) return -7;
		}
		else
		{
			_PRINTF("Key not found: %s\n", key);
			return -2; // no key found
		}

		ptr += offset;
	}
	if(rc->type == 0 || rc->type >= sizeof(dataTypes) / sizeof(dataTypes[0]))
	{
		_PRINTF("Type not set\n");
		return -7;
	}
	if(rc->offset + dataTypes[rc->type].length > 8)
	{
		_PRINTF("Wrong total offset %d\n", rc->offset + dataTypes[rc->type].length);
		return -8;
	}

	for(uint32_t i = 0; i < router.entries_cnt; i++)
	{
		if(strcmp(rc->producer, router.route_cfg[i].producer) == 0 &&
		   rc->can_id == router.route_cfg[i].can_id)
		{
			_PRINTF("[E]Topic %s already exist @%ld for id%d!\n", r->topic, i, rc->can_id);
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
		_PRINTF("%ld Route %s | ID %d | PDO %d(@%d) type %d | Time %d | last data (pub %ld ms.): ",
				i, router.routes[i].topic,
				router.route_cfg[i].can_id,
				router.route_cfg[i].pdo_n,
				router.route_cfg[i].offset,
				router.route_cfg[i].type,
				router.route_cfg[i].publish_interval_ms,
				router.routes[i].last_pub_td);
		for(uint8_t j = 0; j < dataTypes[router.route_cfg[i].type].length; j++)
		{
			_PRINTF("%x ", router.routes[i].data[j]);
		}
		_PRINTF("\n");
	}
}

static void check_try_send(uint32_t it)
{
	if(router.route_cfg[it].publish_interval_ms > 0) // on time (min)
	{
		if(router.routes[it].last_pub_td < (uint32_t)router.route_cfg[it].publish_interval_ms) return; // not send too frequent
	}

	int sts = mqtt_queue_publish(router.routes[it].topic,
								 router.routes[it].data, dataTypes[router.route_cfg[it].type].length,
								 DFLT_QOS /*router.routes[it].qos*/,
								 DFLT_RETAIN /*router.routes[it].retain*/);
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
		if(router.route_cfg[i].pdo_n == rpdo_n &&
		   router.route_cfg[i].can_id == (msg->id.std & 0x7F))
		{
			uint8_t l = dataTypes[router.route_cfg[i].type].length;
			memcpy(router.routes[i].data, &msg->data[router.route_cfg[i].offset], l);

			if(router.route_cfg[i].publish_interval_ms < 0) // on change
			{
				if(memcmp(router.routes[i].data, router.routes[i].data_prev, l) == 0) continue; // not set new msg flag if no change
			}
			memcpy(router.routes[i].data_prev, router.routes[i].data, l);

			router.routes[i].flag_new_msg = true;
			check_try_send(i);
		}
	}
}

void router_init(void)
{
	for(uint32_t i = 0; i < MAX_ROUTES; i++)
	{
		if(router.route_cfg[i].can_id > 0) router.entries_cnt++;
	}
}

void router_poll(uint32_t diff_ms)
{
	for(uint32_t i = 0; i < router.entries_cnt; i++)
	{
		router.routes[i].last_pub_td += diff_ms;

		if(router.routes[i].topic[0] == 0) // construct topic
		{
			if(router.route_cfg[i].can_id == g_cfg_pending_can_node_id)
			{
				snprintf(router.routes[i].topic, sizeof(router.routes[i].topic), "%d/%s%d/%s/%s",
						 g_cfg_ip_address[3] % 10, "gw", router.route_cfg[i].can_id, router.route_cfg[i].producer,
						 dataTypes[router.route_cfg[i].type].syntax);
			}
			else
			{
				bool exit = false;
				for(uint32_t nd = 0; nd < CO->HBcons->numberOfMonitoredNodes; nd++)
				{
					if(CO->HBcons->monitoredNodes[nd].HBstate != CO_HBconsumer_UNKNOWN)
					{
						if(router.route_cfg[i].can_id == CO->HBcons->monitoredNodes[nd].nodeId)
						{
							uint8_t prod_id[16] = {0};
							size_t rd = sizeof(prod_id);
							CO_SDO_abortCode_t sts = read_SDO(CO->SDOclient, router.route_cfg[i].can_id, 0x1F58, 0, prod_id, sizeof(prod_id), &rd, 800);
							if(sts == 0)
							{
								snprintf(router.routes[i].topic, sizeof(router.routes[i].topic), "%d/%s%d/%s/%s",
										 g_cfg_ip_address[3] % 10, prod_id, router.route_cfg[i].can_id, router.route_cfg[i].producer,
										 dataTypes[router.route_cfg[i].type].syntax);
							}
							exit = true;
							break;
						}
					}
					if(exit) break;
				}
			}
		}
		if(router.routes[i].topic[0] != 0 && router.routes[i].flag_new_msg) check_try_send(i);
	}
}