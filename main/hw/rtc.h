/* rtc.h — RTC 接口 */
#ifndef RTC_H
#define RTC_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

void hud_rtc_init(void);
void hud_rtc_get_time(struct tm *ti);

#ifdef __cplusplus
}
#endif

#endif
