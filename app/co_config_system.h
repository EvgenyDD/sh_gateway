#ifndef CO_CONFIG_SYSTEM_H__
#define CO_CONFIG_SYSTEM_H__

#define CO_CONFIG_PDO                        \
	(CO_CONFIG_RPDO_ENABLE |                 \
	 CO_CONFIG_TPDO_ENABLE |                 \
	 CO_CONFIG_RPDO_TIMERS_ENABLE |          \
	 CO_CONFIG_TPDO_TIMERS_ENABLE |          \
	 CO_CONFIG_PDO_SYNC_ENABLE |             \
	 CO_CONFIG_PDO_OD_IO_ACCESS |            \
	 CO_CONFIG_GLOBAL_RT_FLAG_CALLBACK_PRE | \
	 CO_CONFIG_GLOBAL_FLAG_TIMERNEXT |       \
	 CO_CONFIG_GLOBAL_FLAG_OD_DYNAMIC)

#define CO_CONFIG_SDO_CLI       \
	(CO_CONFIG_SDO_CLI_ENABLE | \
	 CO_CONFIG_SDO_CLI_SEGMENTED)

#define CO_CONFIG_FIFO CO_CONFIG_FIFO_ENABLE

#define CO_CONFIG_NMT CO_CONFIG_NMT_MASTER

#define CO_CONFIG_LSS       \
	(CO_CONFIG_LSS_MASTER | \
	 CO_CONFIG_LSS_SLAVE)

#define CO_CONFIG_EM (CO_CONFIG_EM_PRODUCER | \
					  CO_CONFIG_EM_HISTORY |  \
					  CO_CONFIG_EM_CONSUMER | \
					  CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE)

#define CO_CONFIG_SDO_SRV          \
	(CO_CONFIG_SDO_SRV_SEGMENTED | \
	 CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE)

#define CO_CONFIG_SDO_SRV_BUFFER_SIZE 534

#define CO_CONFIG_SDO_CLI_BUFFER_SIZE 534

#endif // CO_CONFIG_SYSTEM_H__