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

static void sensor_task(void *pv)
{
    hud_imu_init();
    hud_rtc_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    bsp_display_start();
    bsp_display_backlight_on();

    screen_init();

    xTaskCreatePinnedToCore(sensor_task, "SENS", 4096, NULL, 3, NULL, 1);

    while (1) {
        screen_update();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}