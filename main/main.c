/* main.c — 入口 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"

#include "screen.h"
#include "imu.h"
#include "rtc.h"

static const char *TAG = "main";

static void ui_task(void *pv)
{
    /* 初始化 BSP 显示（内部已包含 I2C、LCD、触摸、背光） */
    bsp_display_start();
    bsp_display_backlight_on();

    /* 初始化 GUI Guider 和 screen 路由 */
    screen_init();

    /* LVGL 心跳循环 */
    while (1) {
        bsp_display_lock(0);
        lv_timer_handler();
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void sensor_task(void *pv)
{
    imu_init();
    rtc_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    xTaskCreatePinnedToCore(ui_task,     "UI",    8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(sensor_task, "SENS",  4096, NULL, 3, NULL, 1);
}