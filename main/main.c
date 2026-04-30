/* main.c — 入口 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"

#include "screen.h"
#include "imu.h"
#include "rtc.h"
#include "ble.h"

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
    /* NVS 初始化（WiFi/BT 需要） */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    bsp_display_start();
    bsp_display_backlight_off();

    ble_init();

    bsp_display_lock(0);
    screen_init();
    bsp_display_unlock();
    bsp_display_backlight_on();

    xTaskCreatePinnedToCore(sensor_task, "SENS", 4096, NULL, 3, NULL, 1);

    while (1) {
        screen_update();
        vTaskDelay(pdMS_TO_TICKS(16));
    }
}