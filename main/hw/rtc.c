/* rtc.c — PCF85063A RTC */
#include "rtc.h"
#include "board.h"
#include <pcf85063a.h>        /* waveshare/pcf85063a 组件 */

static pcf85063a_handle_t s_rtc = NULL;

void rtc_init(void)
{
    i2c_master_bus_handle_t i2c_bus = bsp_i2c_get_handle();
    s_rtc = pcf85063a_create(i2c_bus, PCF85063A_I2C_ADDRESS);
}

void rtc_update(void)
{
    /* 时间每秒更新一次即可，不需要高频刷新 */
}

void rtc_get_time(struct tm *timeinfo)
{
    if (!s_rtc || !timeinfo) return;
    pcf85063a_get_time_date(s_rtc, timeinfo);
}

void rtc_set_time(const struct tm *timeinfo)
{
    if (!s_rtc || !timeinfo) return;
    pcf85063a_set_time_date(s_rtc, timeinfo);
}
