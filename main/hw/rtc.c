/* rtc.c — PCF85063A RTC */
#include "rtc.h"
#include "board.h"
#include "pcf85063a.h"
#include "esp_log.h"

static const char *TAG = "RTC";
static pcf85063a_dev_t s_rtc;
static bool s_rtc_inited = false;

void rtc_init(void)
{
    i2c_master_bus_handle_t bus_handle = bsp_i2c_get_handle();
    esp_err_t ret = pcf85063a_init(&s_rtc, bus_handle, PCF85063A_ADDRESS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PCF85063A init failed: %d", ret);
        return;
    }
    s_rtc_inited = true;
    ESP_LOGI(TAG, "PCF85063A initialized");
}

void rtc_update(void) { }

void rtc_get_time(struct tm *timeinfo)
{
    if (!s_rtc_inited || !timeinfo) return;

    pcf85063a_datetime_t dt = {0};
    if (pcf85063a_get_time_date(&s_rtc, &dt) != ESP_OK) return;

    timeinfo->tm_year  = dt.year - 1900;
    timeinfo->tm_mon   = dt.month - 1;
    timeinfo->tm_mday  = dt.day;
    timeinfo->tm_wday  = dt.dotw;
    timeinfo->tm_hour  = dt.hour;
    timeinfo->tm_min   = dt.min;
    timeinfo->tm_sec   = dt.sec;
    timeinfo->tm_isdst = -1;
}

void rtc_set_time(const struct tm *timeinfo)
{
    if (!s_rtc_inited || !timeinfo) return;

    pcf85063a_datetime_t dt = {
        .year  = timeinfo->tm_year + 1900,
        .month = timeinfo->tm_mon + 1,
        .day   = timeinfo->tm_mday,
        .dotw  = timeinfo->tm_wday,
        .hour  = timeinfo->tm_hour,
        .min   = timeinfo->tm_min,
        .sec   = timeinfo->tm_sec,
    };
    pcf85063a_set_time_date(&s_rtc, dt);
}