#ifndef PLATFORM_H
#define PLATFORM_H

#include "stm32f4xx.h"
#include <stdbool.h>
#include <stdint.h>

#define GW_COUNT 5

#define _BV(x) (1ULL << (x))

#define PAGE_SIZE (2 * 1024)

#define FLASH_LEN (0x00080000U)
#define FLASH_START FLASH_BASE
#define FLASH_ORIGIN FLASH_BASE
#define FLASH_FINISH (FLASH_BASE + FLASH_LEN)
#define FLASH_SIZE FLASH_LEN

#define PIN_SET(x) x##_GPIO_Port->BSRR = x##_Pin
#define PIN_CLR(x) x##_GPIO_Port->BSRR = ((uint32_t)(x##_Pin)) << 16
#define PIN_WR(x, v) x##_GPIO_Port->BSRR = ((uint32_t)(x##_Pin)) << ((!(v)) * 16)
#define PIN_GET(x) !!(x##_GPIO_Port->IDR & x##_Pin)
#define PIN_GET_ODR(x) !!(x##_GPIO_Port->ODR & x##_Pin)

#define PIN_SET_(x, y) x->BSRR = (uint32_t)(y)
#define PIN_CLR_(x, y) x->BSRR = ((uint32_t)(y)) << 16
#define PIN_WR_(x, y, v) x->BSRR = ((uint32_t)(y)) << ((!(v)) * 16)
#define PIN_GET_(x) !!(x->IDR & (y))
#define PIN_GET_ODR_(x) !!(x->ODR & (y))

#define UNIQUE_ID 0x1FFF7A10

void platform_flash_erase_flag_reset(void);
void platform_flash_erase_flag_reset_sect_cfg(void);

void platform_flash_unlock(void);
void platform_flash_lock(void);
int platform_flash_read(uint32_t addr, uint8_t *src, uint32_t sz);
int platform_flash_write(uint32_t dest, const uint8_t *src, uint32_t sz);

void platform_deinit(void);
void platform_reset(void);
void platform_run_address(uint32_t address);

void platform_get_uid(uint32_t *id);

void delay_ms(volatile uint32_t delay_ms);
void delay_ms_w_can_poll(volatile uint32_t delay_ms);

void platform_watchdog_init(void);
static inline void platform_watchdog_reset(void) { IWDG_ReloadCounter(); }

uint32_t platform_handle_reset_cause(void);
const char *platform_reset_cause_get(uint32_t v);

extern int __preldr_start, __preldr_end;
extern int __ldr_start, __ldr_end;
extern int __cfg_start, __cfg_end;
extern int __app_start, __app_end;
extern int __header_offset;

extern uint8_t g_cfg_ip_address[4];
extern uint8_t g_cfg_ip_addr_master[4];
extern uint8_t g_cfg_ip_addr_gw[4 * GW_COUNT];
extern uint32_t g_uid[3];
extern volatile uint64_t system_time;
extern float sys_freq_1s;
extern uint16_t g_cfg_pending_can_baud;

#endif // PLATFORM_H
