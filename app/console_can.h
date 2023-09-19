int can_emcy_cb(const char *arg, int l)
{
	co_emcy_print_hist();
	return CON_CB_SILENT;
}

int can_hb_cb(const char *arg, int l)
{
	uint32_t count_online = 0, count_offline = 0;
	for(uint32_t nd = 0; nd < CO->HBcons->numberOfMonitoredNodes; nd++)
	{
		if(CO->HBcons->monitoredNodes[nd].HBstate == CO_HBconsumer_ACTIVE) count_online++;
		if(CO->HBcons->monitoredNodes[nd].HBstate == CO_HBconsumer_TIMEOUT) count_offline++;
	}
	_PRINTF("Nodes: %d online | %d offline\n", count_online, count_offline);
	for(uint32_t i = 0; i < CO->HBcons->numberOfMonitoredNodes; i++)
	{
		if(CO->HBcons->monitoredNodes[i].HBstate != CO_HBconsumer_UNKNOWN)
			_PRINTF("\t%d -> %d\n", CO->HBcons->monitoredNodes[i].nodeId, CO->HBcons->monitoredNodes[i].HBstate);
	}
	return CON_CB_SILENT;
}

int can_fw_cb(const char *arg, int l)
{
	unsigned int id = 0;
	int n = sscanf(arg, "%d", &id);
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
					return CON_CB_SILENT;
				}
			}

			uint8_t str[64];
			rd = sizeof(str);
			memset(str, 0, sizeof(str));
			sts = read_SDO(CO->SDOclient, id, 0x1018, 8, str, sizeof(str), &rd, 800);
			if(sts != 0)
			{
				_PRINTF("SDO read %d abort: x%x\n", id, sts);
				return CON_CB_SILENT;
			}

			_PRINTF("Node %d UID: x%08x.x%08x.x%08x %s |", id, uid[0], uid[1], uid[2], str);

			rd = sizeof(str);
			memset(str, 0, sizeof(str));
			sts = read_SDO(CO->SDOclient, id, 0x1008, 0, str, sizeof(str), &rd, 800);
			if(sts != 0)
			{
				_PRINTF("SDO read %d abort: x%x\n", id, sts);
				return CON_CB_SILENT;
			}
			_PRINTF(" %s |", str);

			rd = sizeof(str);
			memset(str, 0, sizeof(str));
			sts = read_SDO(CO->SDOclient, id, 0x1009, 0, str, sizeof(str), &rd, 800);
			if(sts != 0)
			{
				_PRINTF("SDO read %d abort: x%x\n", id, sts);
				return CON_CB_SILENT;
			}
			_PRINTF(" %s |", str);

			rd = sizeof(str);
			memset(str, 0, sizeof(str));
			sts = read_SDO(CO->SDOclient, id, 0x100A, 0, str, sizeof(str), &rd, 800);
			if(sts != 0)
			{
				_PRINTF("SDO read %d abort: x%x\n", id, sts);
				return CON_CB_SILENT;
			}
			_PRINTF(" %s\n", str);
			if(br) break;
		}
	}
	return CON_CB_SILENT;
}

int sdo_rd_cb(const char *arg, int l)
{
	unsigned int id = 0, idx = 0, subidx = 0;
	int n = sscanf(arg, "%d %x %u", &id, &idx, &subidx);
	if(n != 3)
	{
		_PRINTF("Wrong format\n");
		return CON_CB_SILENT;
	}

	_PRINTF("Read %d node x%x:%d\n", id, idx, subidx);

	size_t rd = 256;
	uint8_t data[256];
	CO_SDO_abortCode_t sts = read_SDO(CO->SDOclient, id, idx, subidx, data, sizeof(data), &rd, 3000);
	_PRINTF("%d -> %d | ", sts, rd);
	for(uint32_t i = 0; i < rd; i++)
	{
		_PRINTF("x%x ", data[i]);
	}
	_PRINTF("\n");

	return CON_CB_SILENT;
}

int sdo_wr_cb(const char *arg, int l)
{
	_PRINTF("not implemented\n");
	return CON_CB_SILENT;
}

int can_save_cb(const char *arg, int l)
{
	unsigned int id = 0;
	int n = sscanf(arg, "%d", &id);
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
				return CON_CB_SILENT;
			}
			_PRINTF("Save ID %d OK\n", id);
			if(br) break;
		}
	}
	return CON_CB_SILENT;
}

int can_rst_comm_cb(const char *arg, int l)
{
	unsigned int id = 0;
	int n = sscanf(arg, "%d", &id);
	if(n != 1) id = 0;
	if(id == 0)
	{
		_PRINTF("Resetting communication ALL nodes\n");
	}
	else
		_PRINTF("Resetting communication %d node\n", id);
	CO_NMT_sendCommand(CO->NMT, CO_NMT_RESET_COMMUNICATION, id);
	return CON_CB_SILENT;
}

int can_rst_cb(const char *arg, int l)
{
	unsigned int id = 0;
	int n = sscanf(arg, "%d", &id);
	if(n != 1) id = 0;
	if(id == 0)
		_PRINTF("Resetting ALL nodes\n");
	else
		_PRINTF("Resetting %d node\n", id);
	CO_NMT_sendCommand(CO->NMT, CO_NMT_RESET_NODE, id);
	return CON_CB_SILENT;
}

int can_rst_freeze_cb(const char *arg, int l)
{
	unsigned int id = 0;
	int n = sscanf(arg, "%d", &id);
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

	return CON_CB_SILENT;
}

int can_id_cb(const char *arg, int l)
{
	unsigned int id = 0, new_id = 0;
	int n = sscanf(arg, "%d %d", &id, &new_id);
	if(n != 2)
	{
		_PRINTF("Wrong format\n");
		return CON_CB_SILENT;
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
			return CON_CB_SILENT;
		}
	}

	sts = lss_helper_select(&addr);
	if(sts < 0)
	{
		_PRINTF("Error, select: %d\n", sts);
		return 5;
	}

	sts = lss_helper_cfg_node_id(new_id);
	if(sts < 0)
	{
		_PRINTF("Error, cfg. node id: %d\n", sts);
		return 6;
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

	return CON_CB_SILENT;
}

int can_baud_cb(const char *arg, int l)
{
	unsigned int baud = 0;
	int n = sscanf(arg, "%d", &baud);
	if(n != 1 || baud <= 0)
	{
		_PRINTF("Wrong format, current baud: %d\n", pending_can_baud * 1000);
		return CON_CB_SILENT;
	}

	if(can_drv_check_set_bitrate(CO->CANmodule->CANptr, (int32_t)(baud * 1000U), false) != (int32_t)(baud * 1000U))
	{
		_PRINTF("Error! Baud %d not supported\n");
		return CON_CB_SILENT;
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
					return CON_CB_SILENT;
				}
			}
			stsi = lss_helper_select(&addr);
			if(stsi < 0)
			{
				_PRINTF("Error select node %d: (%d)\n", CO->HBcons->monitoredNodes[i].nodeId, stsi);
				return CON_CB_SILENT;
			}

			stsi = lss_helper_cfg_bit_timing(baud); // kbit/s
			if(stsi < 0)
			{
				lss_helper_deselect();
				_PRINTF("Error cfg. bit timing node %d: (%d)\n", CO->HBcons->monitoredNodes[i].nodeId, stsi);
				return CON_CB_SILENT;
			}
			lss_helper_deselect();
			_PRINTF("activate %d ok\n", CO->HBcons->monitoredNodes[i].nodeId);
		}
	}

	pending_can_baud = baud;
	_PRINTF("activate self ok\n");

	// Now all the nodes are configured, let's apply new baud
	stsi = lss_helper_select(NULL);
	if(stsi < 0)
	{
		lss_helper_deselect();
		_PRINTF("Error global select: (%d)\n", stsi);
		return CON_CB_SILENT;
	}

	stsi = lss_helper_activate_bit_timing(1000);
	if(stsi < 0)
	{
		lss_helper_deselect();
		_PRINTF("Error activate bit timing: (%d)\n", stsi);
		return CON_CB_SILENT;
	}

	lss_helper_deselect();

	_PRINTF("OK\n");
	return CON_CB_SILENT;
}