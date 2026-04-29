/* main.c — 入口 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"

#include "board.h"
#include "screen.h"
#include "imu.h"
#include "rtc.h"

static const char *TAG = "main";

static void ui_task(void *pv)
{
    /* 初始化 BSP 显示（已包含 I2C、LCD、触摸、背光） */
    bsp_display_start();
    bsp_display_backlight_on();

    /* 初始化 GUI Guider 和 screen 路由 */
    screen_init();

    /* LVGL 心跳循环 */
    while (1) {
        bsp_display_lock(0);
        lv_timer_handler();
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(5));   /* ~200fps 刷新 */
    }
}

static void sensor_task(void *pv)
{
    imu_init();
    rtc_init();

    while (1) {
        /* 这里可以定期读取传感器 */
        vTaskDelay(pdMS_TO_TICKS(20));  /* 50Hz */
    }
}

void app_main(void)
{
    /* BSP I2C 在 bsp_display_start() 内部已初始化，
       无需手动调用 bsp_i2c_init() */

    xTaskCreatePinnedToCore(ui_task,     "UI",    8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(sensor_task, "SENS",  4096, NULL, 3, NULL, 1);

    /* app_main 返回后，FreeRTOS 任务继续运行 */
}