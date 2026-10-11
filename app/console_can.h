#include <math.h>

static const char *nmt_state2str(CO_NMT_internalState_t state)
{
	switch(state)
	{
	case CO_NMT_INITIALIZING: return "INIT";
	case CO_NMT_PRE_OPERATIONAL: return "PREOP";
	case CO_NMT_OPERATIONAL: return "OP";
	case CO_NMT_STOPPED: return "STOP";
	default:
	case CO_NMT_UNKNOWN: return "?";
	}
}

static const char *hb_state2str(CO_HBconsumer_state_t state)
{
	switch(state)
	{
	default:
	case CO_HBconsumer_UNCONFIGURED: return "-"; /**< Consumer entry inactive */
	case CO_HBconsumer_UNKNOWN: return "?";		 /**< Consumer enabled, but no heartbeat received yet */
	case CO_HBconsumer_ACTIVE: return "OK";		 /**< Heartbeat received within set time */
	case CO_HBconsumer_TIMEOUT: return "TO";	 /**< No heatbeat received for set time */
	}
}

static void can_emcy_cb(const char *arg, int l, int *ret)
{
	co_emcy_print_hist();
}

static void can_hb_cb(const char *arg, int l, int *ret)
{
	uint32_t count_online = 0, count_offline = 0;
	for(uint32_t nd = 0; nd < CO->HBcons->numberOfMonitoredNodes; nd++)
	{
		if(CO->HBcons->monitoredNodes[nd].HBstate == CO_HBconsumer_ACTIVE) count_online++;
		if(CO->HBcons->monitoredNodes[nd].HBstate == CO_HBconsumer_TIMEOUT) count_offline++;
	}
	_PRINTF("Nodes: %ld online | %ld offline\n", count_online, count_offline);
	for(uint32_t i = 0; i < CO->HBcons->numberOfMonitoredNodes; i++)
	{
		if(CO->HBcons->monitoredNodes[i].HBstate != CO_HBconsumer_UNKNOWN)
			_PRINTF("\t%d: HB %d [%s] | NMT %d [%s]\n",
					CO->HBcons->monitoredNodes[i].nodeId,
					CO->HBcons->monitoredNodes[i].HBstate,
					hb_state2str(CO->HBcons->monitoredNodes[i].HBstate),
					CO->HBcons->monitoredNodes[i].NMTstate,
					nmt_state2str(CO->HBcons->monitoredNodes[i].NMTstate));
	}
}

static void can_fw_cb(const char *arg, int l, int *ret)
{
	unsigned int id = 0;
	int n = sscanf(arg, "%u", &id);
	if(n != 1) id = 0;
	bool br = id != 0;

	for(uint32_t nd = 0; nd < CO->HBcons->numberOfMonitoredNodes; nd++)
	{
		if(CO->HBcons->monitoredNodes[nd].HBstate != CO_HBconsumer_UNKNOWN || br)
		{
			if(!br) id = CO->HBcons->monitoredNodes[nd].nodeId;

			CO_SDO_abortCode_t sts;
			size_t rd;
			uint32_t uid[3];
			for(uint32_t i = 0; i < 3; i++)
			{
				rd = sizeof(uint32_t);
				sts = read_SDO(CO->SDOclient, id, 0x1018, 5 + i, (uint8_t *)&uid[i], sizeof(uint32_t), &rd, 800);
				if(sts != 0)
				{
					_PRINTF("SDO read %d abort: x%x\n", id, sts);
					return;
				}
			}

			uint8_t str[64];
			rd = sizeof(str);
			memset(str, 0, sizeof(str));
			sts = read_SDO(CO->SDOclient, id, 0x1018, 8, str, sizeof(str), &rd, 800);
			if(sts != 0)
			{
				_PRINTF("SDO read %d abort: x%x\n", id, sts);
				return;
			}

			_PRINTF("Node %d UID: x%08lx.x%08lx.x%08lx %s |", id, uid[0], uid[1], uid[2], str);

			uint32_t dev_type = 0;
			rd = sizeof(dev_type);
			sts = read_SDO(CO->SDOclient, id, 0x1000, 0, (uint8_t *)&dev_type, sizeof(dev_type), &rd, 800);
			if(sts != 0)
			{
				_PRINTF("SDO read %d abort: x%x\n", id, sts);
				return;
			}
			_PRINTF(" x%lx |", dev_type);

			rd = sizeof(str);
			memset(str, 0, sizeof(str));
			sts = read_SDO(CO->SDOclient, id, 0x1008, 0, str, sizeof(str), &rd, 800);
			if(sts != 0)
			{
				_PRINTF("SDO read %d abort: x%x\n", id, sts);
				return;
			}
			_PRINTF(" %s |", str);

			rd = sizeof(str);
			memset(str, 0, sizeof(str));
			sts = read_SDO(CO->SDOclient, id, 0x1009, 0, str, sizeof(str), &rd, 800);
			if(sts != 0)
			{
				_PRINTF("SDO read %d abort: x%x\n", id, sts);
				return;
			}
			_PRINTF(" %s |", str);

			rd = sizeof(str);
			memset(str, 0, sizeof(str));
			sts = read_SDO(CO->SDOclient, id, 0x100A, 0, str, sizeof(str), &rd, 800);
			if(sts != 0)
			{
				_PRINTF("SDO read %d abort: x%x\n", id, sts);
				return;
			}
			_PRINTF(" %s\n", str);
			if(br) break;
		}
	}
}

static void sdo_rd_cb(const char *arg, int l, int *ret)
{
	unsigned int id = 0, idx = 0, subidx = 0;
	int n = sscanf(arg, "%u %x %u", &id, &idx, &subidx);
	if(n != 3 && n != 2)
	{
		_PRINTF("Wrong format! d:id hex:idx [d:sub]\n");
		return;
	}

	uint8_t start_sub = 0;
	uint8_t end_sub = 0;

	if(n == 2)
	{
		size_t rd = 256;
		uint8_t data[256];
		CO_SDO_abortCode_t sts = read_SDO(CO->SDOclient, id, idx, 0, data, sizeof(data), &rd, 3000);

		if(sts != 0 || rd == 0)
		{
			_PRINTF("Failed to read sub 0 count. Status: x%x\n", sts);
			return;
		}

		uint32_t total_subindexes = data[0];

		// _PRINTF("Object x%x: %u sub\n", idx, total_subindexes);
		start_sub = 1;
		end_sub = total_subindexes;
	}
	else
	{
		start_sub = subidx;
		end_sub = subidx;
	}

	for(uint8_t current_sub = start_sub; current_sub <= end_sub; current_sub++)
	{
		_PRINTF("Read %d node x%x:%d", id, idx, current_sub);

		size_t rd = 256;
		uint8_t data[256];
		CO_SDO_abortCode_t sts = read_SDO(CO->SDOclient, id, idx, current_sub, data, sizeof(data), &rd, 3000);
		_PRINTF(" | Status: x%x (%zu): ", sts, rd);

		for(uint32_t i = 0; i < rd * (sts ? 0 : 1); i++)
		{
			_PRINTF("%02x ", data[i]);
		}
		if(sts == 0)
		{
			if(rd == 1)
			{
				int8_t _i8;
				uint8_t _u8;
				memcpy(&_i8, data, 1);
				memcpy(&_u8, data, 1);
				_PRINTF(" | (u8:%u, i8:%d)", _u8, _i8);
			}
			else if(rd == 2)
			{
				int16_t _i16;
				uint16_t _u16;
				memcpy(&_i16, data, 2);
				memcpy(&_u16, data, 2);
				_PRINTF(" | (u16:%u, i16:%d)", _u16, _i16);
			}
			else if(rd == 4)
			{
				int32_t _i32;
				uint32_t _u32;
				float f;
				memcpy(&_i32, data, 4);
				memcpy(&_u32, data, 4);
				memcpy(&f, data, 4);

				// Печатаем целые числа
				_PRINTF(" | (u32:%lu, i32:%ld", _u32, _i32);

				// Проверяем float на валидность (не NaN и не Inf)
				if(isfinite(f))
				{
					_PRINTF(", float:%f)", f);
				}
				else
				{
					_PRINTF(")");
				}
			}
			_PRINTF("\n");
		}
	}
}

static void sdo_wr_cb(const char *arg, int l, int *ret)
{
	unsigned int id = 0, idx = 0, subidx = 0;
	int offset = 0;
	int read_bytes = 0;
	unsigned char data[32];
	size_t data_len;
	int n = sscanf(arg, "%u %x %u%n", &id, &idx, &subidx, &read_bytes);

	if(n < 3)
	{
		_PRINTF("Wrong format! d:id hex:idx d:sub hex:data...\n");
		return;
	}
	offset += read_bytes;
	data_len = 0;

	while(data_len < 32)
	{
		unsigned int byte_val;

		int res = sscanf(arg + offset, "%x%n", &byte_val, &read_bytes);

		if(res != 1) break;

		data[data_len++] = (unsigned char)byte_val;
		offset += read_bytes;
	}

	_PRINTF("Writing %d: x%x:%d (%d): ", id, idx, subidx, data_len);
	for(uint32_t i = 0; i < data_len; i++)
	{
		_PRINTF("%x ", data[i]);
	}
	CO_SDO_abortCode_t sts = write_SDO(CO->SDOclient, id, idx, subidx, data, data_len, 3000);
	_PRINTF("| Status: x%x\n", sts);
}

static void can_role_cb(const char *arg, int l, int *ret)
{
	uint8_t prod_id[16] = {0};
	unsigned int id = 0;
	char str[32];
	int n = sscanf(arg, "%u %s", &id, str);

	if(n <= 0) // print all roles
	{
		for(uint32_t nd = 0; nd < CO->HBcons->numberOfMonitoredNodes; nd++)
		{
			if(CO->HBcons->monitoredNodes[nd].HBstate != CO_HBconsumer_UNKNOWN)
			{
				id = CO->HBcons->monitoredNodes[nd].nodeId;
				size_t rd = sizeof(uint32_t);
				CO_SDO_abortCode_t sts = read_SDO(CO->SDOclient, id, 0x1F58, 0, prod_id, sizeof(prod_id), &rd, 800);
				if(sts != 0)
				{
					_PRINTF("SDO read %d abort: x%x\n", id, sts);
					return;
				}
				_PRINTF("#%d prod_id: %.*s\n", id, rd, prod_id);
			}
		}
	}
	if(n == 2)
	{
		strncpy((char *)prod_id, str, sizeof(prod_id));
		CO_SDO_abortCode_t sts = write_SDO(CO->SDOclient, id, 0x1f58, 0, prod_id, sizeof(prod_id), 800);
		if(sts != 0)
		{
			_PRINTF("SDO write %d abort: x%x\n", id, sts);
			return;
		}
		n = 1;
	}
	if(n == 1)
	{
		size_t rd = sizeof(uint32_t);
		CO_SDO_abortCode_t sts = read_SDO(CO->SDOclient, id, 0x1F58, 0, prod_id, sizeof(prod_id), &rd, 800);
		if(sts != 0)
		{
			_PRINTF("SDO read %d abort: x%x\n", id, sts);
			return;
		}
		_PRINTF("Current (#%d) prod_id: %.*s\n", id, rd, prod_id);
	}
}

static void can_save_cb(const char *arg, int l, int *ret)
{
	unsigned int id = 0;
	int n = sscanf(arg, "%u", &id);
	if(n != 1) id = 0;
	bool br = id != 0;

	for(uint32_t nd = 0; nd < CO->HBcons->numberOfMonitoredNodes; nd++)
	{
		if(CO->HBcons->monitoredNodes[nd].HBstate != CO_HBconsumer_UNKNOWN || br)
		{
			if(!br) id = CO->HBcons->monitoredNodes[nd].nodeId;

			uint32_t save_code = 0x73617665;
			CO_SDO_abortCode_t sts = write_SDO(CO->SDOclient, id, 0x1010, 1, (uint8_t *)&save_code, sizeof(uint32_t), 800);
			if(sts != 0)
			{
				_PRINTF("SDO write %d abort: x%x\n", id, sts);
				return;
			}
			_PRINTF("Save ID %d OK\n", id);
			if(br) break;
		}
	}
}

static void can_rst_comm_cb(const char *arg, int l, int *ret)
{
	unsigned int id = 0;
	int n = sscanf(arg, "%u", &id);
	if(n != 1) id = 0;
	if(id == 0)
	{
		_PRINTF("Resetting communication ALL nodes\n");
	}
	else
		_PRINTF("Resetting communication %d node\n", id);
	CO_NMT_sendCommand(CO->NMT, CO_NMT_RESET_COMMUNICATION, id);
}

static void can_rst_cb(const char *arg, int l, int *ret)
{
	unsigned int id = 0;
	int n = sscanf(arg, "%u", &id);
	if(n != 1) id = 0;
	if(id == 0)
		_PRINTF("Resetting ALL nodes\n");
	else
		_PRINTF("Resetting %d node\n", id);
	CO_NMT_sendCommand(CO->NMT, CO_NMT_RESET_NODE, id);
}

static void can_rst_freeze_cb(const char *arg, int l, int *ret)
{
	unsigned int id = 0;
	int n = sscanf(arg, "%u", &id);
	if(n != 1) id = 0;
	if(id == 0)
		_PRINTF("Resetting ALL nodes\n");
	else
		_PRINTF("Resetting %d node\n", id);
	CO_NMT_sendCommand(CO->NMT, CO_NMT_RESET_NODE, id);

	delay_ms(100);

	for(uint32_t i = 0; i < CO->HBcons->numberOfMonitoredNodes; i++)
	{
		if(CO->HBcons->monitoredNodes[i].HBstate != CO_HBconsumer_UNKNOWN)
		{
			const uint32_t retr = 3;
			for(uint32_t r = 0; r < retr; r++)
			{
				uint32_t cmd = 0;
				CO_SDO_abortCode_t sts = write_SDO(CO->SDOclient, CO->HBcons->monitoredNodes[i].nodeId, 0x1F51, 1, (uint8_t *)&cmd, sizeof(uint32_t), 250);
				if(sts == 0)
				{
					_PRINTF("Node %d freezed\n", CO->HBcons->monitoredNodes[i].nodeId);
					break;
				}
				if(r == retr - 1 && sts) _PRINTF("Node %d freeze fail: x%x\n", CO->HBcons->monitoredNodes[i].nodeId, sts);
			}
		}
	}
}

static void can_id_cb(const char *arg, int l, int *ret)
{
	unsigned int id = 0, new_id = 0;
	int n = sscanf(arg, "%u %u", &id, &new_id);
	if(n != 2)
	{
		_PRINTF("Wrong format\n");
		return;
	}

	CO_LSS_address_t addr;
	CO_SDO_abortCode_t sts;
	for(uint32_t i = 0; i < 4; i++)
	{
		// read identity object to obtain LSS address
		size_t rd = sizeof(uint32_t);
		sts = read_SDO(CO->SDOclient, id, 0x1018, 4 + i, (uint8_t *)&addr.addr[i], sizeof(uint32_t), &rd, 800);
		if(sts != 0 || rd != 4)
		{
			_PRINTF("SDO read %d abort: x%x\n", id, sts);
			return;
		}
	}

	sts = lss_helper_select(&addr);
	if(sts < 0)
	{
		_PRINTF("Error, select: %d\n", sts);
		return;
	}

	sts = lss_helper_cfg_node_id(new_id);
	if(sts < 0)
	{
		_PRINTF("Error, cfg. node id: %d\n", sts);
		return;
	}

#if 0
		sts = lss_helper_cfg_store();
		if(sts < 0)
		{
			_PRINTF("Error, store fail: %d\n", sts);
			return 7;
		}
#endif

	lss_helper_deselect();
	_PRINTF("OK\n");
}

static void can_baud_cb(const char *arg, int l, int *ret)
{
	unsigned int baud = 0;
	int n = sscanf(arg, "%u", &baud);
	if(n != 1 || baud <= 0)
	{
		_PRINTF("Wrong format, current baud: %d\n", g_cfg_pending_can_baud * 1000);
		return;
	}

	if(can_drv_check_set_bitrate(CO->CANmodule->CANptr, (int32_t)(baud * 1000), false) != (int32_t)(baud * 1000))
	{
		_PRINTF("Error! Baud %d not supported\n", baud * 1000);
		return;
	}

	CO_SDO_abortCode_t sts;
	int stsi;
	for(uint32_t i = 0; i < CO->HBcons->numberOfMonitoredNodes; i++)
	{
		if(CO->HBcons->monitoredNodes[i].HBstate != CO_HBconsumer_UNKNOWN)
		{
			CO_LSS_address_t addr;
			for(uint32_t j = 0; j < 4; j++)
			{
				// read identity object to obtain LSS address
				size_t rd = sizeof(uint32_t);
				sts = read_SDO(CO->SDOclient, CO->HBcons->monitoredNodes[i].nodeId, 0x1018, 4 + j, (uint8_t *)&addr.addr[j], sizeof(uint32_t), &rd, 800);
				if(sts != 0 || rd != 4)
				{
					_PRINTF("SDO read %d abort: x%x\n", CO->HBcons->monitoredNodes[i].nodeId, sts);
					return;
				}
			}
			stsi = lss_helper_select(&addr);
			if(stsi < 0)
			{
				_PRINTF("Error select node %d: (%d)\n", CO->HBcons->monitoredNodes[i].nodeId, stsi);
				return;
			}

			stsi = lss_helper_cfg_bit_timing(baud); // kbit/s
			if(stsi < 0)
			{
				lss_helper_deselect();
				_PRINTF("Error cfg. bit timing node %d: (%d)\n", CO->HBcons->monitoredNodes[i].nodeId, stsi);
				return;
			}
			lss_helper_deselect();
			_PRINTF("activate %d ok\n", CO->HBcons->monitoredNodes[i].nodeId);
		}
	}

	g_cfg_pending_can_baud = baud;
	_PRINTF("activate self ok\n");

	// Now all the nodes are configured, let's apply new baud
	stsi = lss_helper_select(NULL);
	if(stsi < 0)
	{
		lss_helper_deselect();
		_PRINTF("Error global select: (%d)\n", stsi);
		return;
	}

	stsi = lss_helper_activate_bit_timing(1000);
	if(stsi < 0)
	{
		lss_helper_deselect();
		_PRINTF("Error activate bit timing: (%d)\n", stsi);
		return;
	}

	lss_helper_deselect();

	_PRINTF("OK\n");
}

#define RSDO(ID, IDX, SUBIDX, VAL)                                                          \
	rd = sizeof(VAL);                                                                       \
	sts = read_SDO(CO->SDOclient, ID, IDX, SUBIDX, (uint8_t *)&VAL, sizeof(VAL), &rd, 800); \
	if(sts != 0 || rd != sizeof(VAL))                                                       \
	{                                                                                       \
		_PRINTF("SDO read %d abort: x%x\n", id, sts);                                       \
		*ret = CON_CB_SILENT;                                                               \
		return;                                                                             \
	}

#define RSDO_T(ID, IDX, SUBIDX, VAL, TYPE)                                                   \
	rd = sizeof(TYPE);                                                                       \
	sts = read_SDO(CO->SDOclient, ID, IDX, SUBIDX, (uint8_t *)&VAL, sizeof(TYPE), &rd, 800); \
	if(sts != 0 || rd != sizeof(TYPE))                                                       \
	{                                                                                        \
		_PRINTF("SDO read %d abort: x%x (x%x:x%x)\n", id, sts, IDX, SUBIDX);                 \
		*ret = CON_CB_SILENT;                                                                \
		return;                                                                              \
	}

static void can_meteo_cb(const char *arg, int l, int *ret)
{
	unsigned int id = 0;
	int n = sscanf(arg, "%u", &id);
	if(n != 1)
	{
		_PRINTF("Wrong format!\n");
		return;
	}

	CO_SDO_abortCode_t sts;
	size_t rd;
	uint8_t v_u8;
	int16_t v_i16;
	uint16_t v_u16;
	int32_t v_i32;
	uint32_t v_u32;
	_PRINTF("ADC: ");
	for(uint32_t i = 0x01; i <= 0x09; i++)
	{
		RSDO(id, 0x6000, i, v_i16);
		_PRINTF("%04d ", v_i16);
	}

	_PRINTF("\nGPS: ");

	RSDO(id, 0x6100, 1, v_u16);
	_PRINTF("%d/", v_u16);
	RSDO(id, 0x6100, 2, v_u8);
	_PRINTF("%d/", v_u8);
	RSDO(id, 0x6100, 3, v_u8);
	_PRINTF("%d ", v_u8);

	RSDO(id, 0x6100, 4, v_u8);
	_PRINTF("%d:", v_u8);
	RSDO(id, 0x6100, 5, v_u8);
	_PRINTF("%d:", v_u8);
	RSDO(id, 0x6100, 6, v_u8);
	_PRINTF("%d | ", v_u8);
	RSDO(id, 0x6100, 0xC, v_i32);
	_PRINTF("h %ld | ", v_i32);
	RSDO(id, 0x6100, 0xD, v_i32);
	_PRINTF("hMSL %ld | ", v_i32);
	RSDO(id, 0x6100, 0x12, v_u8);
	_PRINTF("numSV %d\n", v_u8);

	RSDO(id, 0x6100, 7, v_i32);
	_PRINTF("nano %ld | ", v_i32);
	RSDO(id, 0x6100, 8, v_u32);
	_PRINTF("iTOW %ld | ", v_u32);
	RSDO(id, 0x6100, 9, v_u32);
	_PRINTF("tAcc %ld | ", v_u32);
	RSDO(id, 0x6100, 0xA, v_i32);
	_PRINTF("lon %ld | ", v_i32);
	RSDO(id, 0x6100, 0xB, v_i32);
	_PRINTF("lat %ld | ", v_i32);
	RSDO(id, 0x6100, 0xE, v_u32);
	_PRINTF("hAcc %ld | ", v_u32);
	RSDO(id, 0x6100, 0xF, v_u32);
	_PRINTF("vAcc %ld | ", v_u32);
	RSDO(id, 0x6100, 0x10, v_u32);
	_PRINTF("sAcc %ld\n", v_u32);
	RSDO(id, 0x6100, 0x11, v_u32);
	_PRINTF("headAcc %ld | ", v_u32);
	RSDO(id, 0x6100, 0x13, v_i32);
	_PRINTF("headMot %ld | ", v_i32);
	RSDO(id, 0x6100, 0x14, v_i32);
	_PRINTF("velN %ld | ", v_i32);
	RSDO(id, 0x6100, 0x15, v_i32);
	_PRINTF("velE %ld | ", v_i32);
	RSDO(id, 0x6100, 0x16, v_i32);
	_PRINTF("velD %ld | ", v_i32);
	RSDO(id, 0x6100, 0x17, v_i32);
	_PRINTF("gSpd %ld | ", v_i32);
	RSDO(id, 0x6100, 0x18, v_u16);
	_PRINTF("pDOP %d | ", v_u16);
	RSDO(id, 0x6100, 0x19, v_u32);
	_PRINTF("flags x%lx\n", v_u32);

	_PRINTF("Baro: ");
	RSDO(id, 0x6101, 1, v_u32);
	_PRINTF("%ld | Temp: ", v_u32);
	RSDO(id, 0x6101, 2, v_i16);
	_PRINTF("%d\n", v_i16);

	_PRINTF("AHT21: Temp: ");
	RSDO(id, 0x6103, 1, v_i16);
	_PRINTF("%d | Hum: ", v_i16);
	RSDO(id, 0x6103, 2, v_i16);
	_PRINTF("%d\n", v_i16);

	_PRINTF("Meteo: WindA ");
	RSDO(id, 0x6102, 1, v_u32);
	_PRINTF("%ld | Head: ", v_u32);
	RSDO(id, 0x6102, 0xa, v_u16);
	_PRINTF("%d\n", v_u16);
	uint32_t head[8];
	for(uint32_t i = 0x2; i <= 0x9; i++)
	{
		RSDO(id, 0x6102, i, head[i - 0x2]);
	}
	_PRINTF("E %ld NE %ld N %ld NW %ld W %ld SW %ld S %ld SE %ld\n",
			head[0], head[1], head[2], head[3], head[4], head[5], head[6], head[7]);

	_PRINTF("RainA: ");
	RSDO(id, 0x6102, 0xb, v_u32);
	_PRINTF("%ld Temp: ", v_u32);
	RSDO(id, 0x6102, 0xc, v_i16);
	_PRINTF("%d Solar: ", v_i16);
	RSDO(id, 0x6102, 0xe, v_i16);
	_PRINTF("%d\n", v_i16);
}
