/* rtc.h — PCF85063A RTC */
#ifndef RTC_H
#define RTC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

void rtc_init(void);
void rtc_update(void);
void rtc_get_time(struct tm *timeinfo);
void rtc_set_time(const struct tm *timeinfo);

#ifdef __cplusplus
}
#endif

#endif