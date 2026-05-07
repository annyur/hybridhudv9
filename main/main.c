/* main.c — 入口 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "esp_task_wdt.h"
#include "esp_log.h"

#include "screen.h"
#include "imu.h"
#include "rtc.h"
#include "ble.h"
#include "conn.h"
#include "obd.h"
#include "bluetooth.h"
#include "key.h"

static const char *TAG = "MAIN";

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
    /* NVS 初始化 */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 启动显示系统 */
    bsp_display_start();

    /* 业务初始化 */
    ble_init();
    ble_enable();   /* 开机默认启用蓝牙 */
    obd_init();
    key_init();

    /* UI 初始化 */
    bsp_display_lock(-1);
    screen_init();
    bsp_display_unlock();

    xTaskCreatePinnedToCore(sensor_task, "SENS", 4096, NULL, 3, NULL, 1);

    while (1) {
        bsp_display_lock(-1);
        key_poll();
        screen_update();
        bluetooth_auto_reconnect();
        /* OBD 自动启动：不依赖蓝牙界面，后台持续检查 */
        if (conn_is_ready() && !obd_is_running()) {
            obd_start();
            ESP_LOGI(TAG, "OBD auto started");
        }
        obd_update();
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}