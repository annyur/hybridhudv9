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

    /* 业务初始化（暗屏） */
    ble_init();
    obd_init();

    /* UI 初始化 + 开机动画 */
    bsp_display_lock(-1);
    screen_init();
    general_boot_animation();
    bsp_display_unlock();

    /* 强制 LVGL 刷新周期为 100ms (10Hz)，减轻 QSPI 带宽压力 */
    lv_display_t *disp = lv_display_get_default();
    if (disp) {
        lv_timer_t *refr = lv_display_get_refr_timer(disp);
        if (refr) {
            lv_timer_set_period(refr, 100);
            ESP_LOGI(TAG, "LVGL refresh period set to 100ms");
        }
    }

    bsp_display_backlight_on();

    xTaskCreatePinnedToCore(sensor_task, "SENS", 4096, NULL, 3, NULL, 1);

    while (1) {
        bsp_display_lock(-1);
        screen_update();
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}