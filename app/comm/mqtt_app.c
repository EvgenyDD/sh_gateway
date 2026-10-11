#include "mqtt_app.h"
#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"
#include "lwip/timeouts.h"
#include "platform.h"
#include <string.h>

uint32_t test_v2 = 0;

#define ROOT_MQTT_IP IPADDR4_INIT_BYTES(7, 7, 7, 1)

#define RECONN_TO_MS 5000

#define MQTT_QUEUE_SIZE 64
#define MQTT_MAX_TOPIC_LEN 40
#define MQTT_MAX_PAYLOAD_LEN 16

typedef struct
{
	char topic[MQTT_MAX_TOPIC_LEN];
	uint8_t payload[MQTT_MAX_PAYLOAD_LEN];
	uint16_t payload_len;
	uint8_t qos;
	uint8_t retain;
} mqtt_msg_t;

typedef struct
{
	mqtt_msg_t storage[MQTT_QUEUE_SIZE];
	volatile uint8_t head;
	volatile uint8_t tail;
	volatile bool is_sending; // Флаг, занят ли lwIP отправкой прямо сейчас
} mqtt_queue_t;

mqtt_dbg_t mqtt_dbg = {0};

static mqtt_queue_t mq = {0};

static char mqtt_client_id_buffer[30];
static mqtt_client_t *static_client = NULL;

static int mqtt_queue_send_next(void);

static void mqtt_pub_request_cb(void *arg, err_t result)
{
	mq.is_sending = false;
	mq.tail = (mq.tail + 1) % MQTT_QUEUE_SIZE;
	if(mq.head != mq.tail) mqtt_queue_send_next();
}

static int mqtt_queue_send_next(void)
{
	mq.is_sending = true;
	mqtt_msg_t *msg = &mq.storage[mq.tail];
	err_t err = mqtt_publish(static_client, msg->topic, msg->payload, msg->payload_len,
							 msg->qos, msg->retain, mqtt_pub_request_cb, NULL);

	if(err != ERR_OK)
	{
		// Если lwIP сразу вернул ошибку буфера (ERR_MEM), сбрасываем флаг отправки,
		// чтобы следующая попытка (или цикл) могла протолкнуть очередь.
		mq.is_sending = false;
		mqtt_dbg.err_publ++;
		// Опционально: обработка ошибки (например, сдвиг хвоста, если сообщение "битое")
		return -1;
	}
	return 0;
}

static void incom_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) // ВАЖНО: данные приходят сырыми, без '\0' на конце!
{
	// char payload[128];
	// if(len < sizeof(payload))
	{
		// memcpy(payload, data, len);
		// payload[len] = '\0';

		// test_v2 = mqtt_payload_to_uint32(data, len);
		// if(test_v2 == 0) test_v2 = 111;
	}
}

static void incom_publ_cb(void *arg, const char *topic, u32_t tot_len) // Вызывается ПЕРВЫМ (сообщает имя топика)
{
	// Вызывается первым, когда брокер присылает сообщение в топик
	// Здесь можно проверить, какой именно топик пришел (если подписаны на несколько)
}

static void reconnect_cb(void *arg)
{
	mqtt_app_init_client();
}

static void conn_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
	mqtt_dbg.conn_cb_cnt++;
	if(status == MQTT_CONNECT_ACCEPTED)
	{
		mqtt_dbg.status = STATE_CONNECTED;
		mqtt_set_inpub_callback(client, incom_publ_cb, incom_data_cb, arg); // Настройка колбэков для приема сообщений (нужно делать сразу при коннекте)
		mqtt_subscribe(client, "stm32/cmd", 0, NULL, NULL);					// Подписываемся на топик для ПРИЕМА команд (QoS 0)
	}
	else
	{
		mqtt_dbg.status = STATE_CONN_RETRY;
		sys_untimeout(reconnect_cb, NULL);			   // Удаляем старый таймер, если он вдруг был запущен, чтобы не плодить их
		sys_timeout(RECONN_TO_MS, reconnect_cb, NULL); // try again later
	}
}

int mqtt_queue_publish(const char *topic, const void *payload, uint16_t len, uint8_t qos, uint8_t retain)
{
	// Проверяем, есть ли место в очереди (следующий head не должен равен tail)
	uint8_t next_head = (mq.head + 1) % MQTT_QUEUE_SIZE;
	if(next_head == mq.tail)
	{
		mqtt_dbg.tx_q_ovf++;
		return -1;
	}

	mqtt_msg_t *msg = &mq.storage[mq.head];
	strncpy(msg->topic, topic, MQTT_MAX_TOPIC_LEN - 1);
	msg->topic[MQTT_MAX_TOPIC_LEN - 1] = '\0';

	uint16_t copy_len = (len > MQTT_MAX_PAYLOAD_LEN) ? MQTT_MAX_PAYLOAD_LEN : len;
	memcpy(msg->payload, payload, copy_len);
	msg->payload_len = copy_len;

	msg->qos = qos;
	msg->retain = retain;

	mq.head = next_head;

	if(mq.is_sending) return 1;
	return mqtt_queue_send_next() ? 1 : 0;
}

// void mqtt_app_publish_message(const char *topic, const char *message)
// {
// 	if(static_client != NULL && mqtt_client_is_connected(static_client))
// 	{
// 		err_t err = mqtt_publish(static_client, topic, message, strlen(message), 0, 0, NULL, NULL);
// 		if(err != ERR_OK)
// 		{
// 			// Ошибка отправки (например, переполнен буфер lwIP)
// 		}
// 	}
// }

void mqtt_app_init_client(void)
{
	ip4_addr_t broker_ip = ROOT_MQTT_IP;

	if(static_client != NULL)
	{
		mqtt_disconnect(static_client);
		static_client = NULL;
	}

	static_client = mqtt_client_new();
	if(static_client != NULL)
	{
		struct mqtt_connect_client_info_t ci = {0};

		snprintf(mqtt_client_id_buffer, sizeof(mqtt_client_id_buffer), "GW_%d.%d.%d.%d", g_cfg_ip_address[0], g_cfg_ip_address[1], g_cfg_ip_address[2], g_cfg_ip_address[3]);

		ci.client_id = mqtt_client_id_buffer;
		ci.keep_alive = 60;	   // Пинг-таймаут в секундах
		ci.client_user = NULL; // Анонимно
		ci.client_pass = NULL; // Анонимно

		int sts = mqtt_client_connect(static_client, &broker_ip, 1883, conn_cb, NULL, &ci);
		if(sts != ERR_OK)
		{
			sys_timeout(RECONN_TO_MS, reconnect_cb, NULL); // try again later
		}
		mqtt_dbg.conn_code = sts;
	}
	else
	{
		mqtt_dbg.status = STATE_OUT_OF_MEM;
		sys_timeout(RECONN_TO_MS, reconnect_cb, NULL); // try again later
	}
}