/* rtc.h — PCF85063A RTC */
#ifndef RTC_H
#define RTC_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

void hud_rtc_init(void);
void hud_rtc_update(void);
void hud_rtc_get_time(struct tm *timeinfo);
void hud_rtc_set_time(const struct tm *timeinfo);

#ifdef __cplusplus
}
#endif

#endif