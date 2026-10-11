#include "logger.h"
#include "fram.h"
#include "platform.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define LOG_ENTRIES 128
#define LOG_MSG_LEN 64

static char log_mem[LOG_ENTRIES][LOG_MSG_LEN] = {0};
static uint32_t log_ptr = 0, log_num = 0;

void logger_print(const char *format, ...)
{
	va_list va;
	va_start(va, format);
	log_ptr = (log_ptr + 1) % LOG_ENTRIES;
	vsnprintf(log_mem[log_ptr], LOG_MSG_LEN, format, va);
	log_num++;
	va_end(va);
}

uint32_t logger_get_entry_count(void) { return log_num; }

// #define FRAM_QUANT 256
// enum
// {
// 	__assert_fram_entries_count = 1 / (sizeof(fram_entries_t) > FRAM_LOG_OFFSET ? 0 : 1),
// 	__assert_fram_logger_align = 1 / (((FRAM_SIZE - FRAM_LOG_OFFSET) % FRAM_QUANT) != 0 ? 0 : 1)
// };

// static uint32_t last_addr = 0;
// static uint32_t count = 0; // entries count

// int logger_init(void)
// {
// 	fram_data.logger_start = fram_data.logger_end = FRAM_LOG_OFFSET;

// 	uint8_t array[FRAM_QUANT];
// 	for(uint32_t i = FRAM_LOG_OFFSET; i < (FRAM_SIZE - FRAM_LOG_OFFSET) / FRAM_QUANT; i++)
// 	{
// 		int sts = fram_read(FRAM_LOG_OFFSET + i * FRAM_QUANT, array, sizeof(array));
// 		if(sts) return sts;

// 		for(uint32_t j = 0; j < FRAM_QUANT; j++)
// 		{
// 		}
// 	}
// 	return 0;
// }

// int logger_erase(void)
// {
// 	last_addr = 0;

// 	int sts;
// 	uint8_t array[FRAM_QUANT];
// 	memset(array, 0xFF, sizeof(array));
// 	for(uint32_t i = 0; i < (FRAM_SIZE - FRAM_LOG_OFFSET) / FRAM_QUANT; i++)
// 	{
// 		sts = fram_write(FRAM_LOG_OFFSET + i * FRAM_QUANT, array, sizeof(array));
// 		if(sts) return sts;
// 	}
// 	return 0;
// }

// void logger_log(const char *msg)
// {
// 	size_t len = strlen(msg);
// 	if(len == 0) return;
// }

// size_t logger_get_count(void) { return count; }

// void logger_get_entry(size_t n)
// {
// }