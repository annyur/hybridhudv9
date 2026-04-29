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
    imu_init();
    rtc_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    bsp_display_start();
    bsp_display_backlight_on();
    /* bsp_i2c_deinit();  <-- 删掉这行！设备还在总线上 */

    bsp_display_lock(0);
    screen_init();
    bsp_display_unlock();

    xTaskCreatePinnedToCore(sensor_task, "SENS", 4096, NULL, 3, NULL, 1);

    while (1) {
        screen_update();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}