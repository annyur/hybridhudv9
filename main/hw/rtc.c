/* rtc.c — RTC 时间获取（占位实现，实际项目可接 SNTP/RTC 芯片） */
#include "rtc.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

void hud_rtc_init(void) { }

void hud_rtc_get_time(struct tm *ti)
{
    if (!ti) return;

    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        struct tm *tm_info = localtime(&tv.tv_sec);
        if (tm_info) {
            *ti = *tm_info;
            return;
        }
    }
    /* 保底：清零 */
    memset(ti, 0, sizeof(struct tm));
}