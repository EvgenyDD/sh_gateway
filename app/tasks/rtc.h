#ifndef RTC_H__
#define RTC_H__

#include "platform.h"
#include <stdint.h>

int rtc_init(void);
void mcu_rtc_get_date(RTC_DateTypeDef *RTC_DateStructure);
void mcu_rtc_get_time(RTC_TimeTypeDef *RTC_TimeStructure);

int mcu_rtc_set_time(uint8_t hours, uint8_t minutes, uint8_t seconds);
int mcu_rtc_set_date(uint16_t year, uint8_t weekday, uint8_t month, uint8_t date);
void rtc_set_td(void);

const char *mcu_rtc_weekday(uint8_t weekday);

#endif // RTC_H__