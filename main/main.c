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
#include "obd.h"
#include "general.h"
#include "bluetooth.h"

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

    /* 显示顺时针旋转 90° */
    lv_display_t *disp = lv_display_get_default();
    if (disp) {
        ESP_LOGI(TAG, "before rotation: %d", lv_display_get_rotation(disp));
        lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
        ESP_LOGI(TAG, "after rotation: %d", lv_display_get_rotation(disp));
        ESP_LOGI(TAG, "hor_res=%d, ver_res=%d", lv_display_get_horizontal_resolution(disp), lv_display_get_vertical_resolution(disp));
    } else {
        ESP_LOGE(TAG, "no default display found!");
    }

    /* 业务初始化 */
    ble_init();
    obd_init();

    /* UI 初始化 + 开机动画 */
    bsp_display_lock(-1);
    screen_init();
    general_boot_animation();
    bsp_display_unlock();

    bsp_display_backlight_on();

    xTaskCreatePinnedToCore(sensor_task, "SENS", 4096, NULL, 3, NULL, 1);

    while (1) {
        bsp_display_lock(-1);
        screen_update();
        bluetooth_auto_reconnect();
        obd_update();  /* OBD 轮询 */
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}