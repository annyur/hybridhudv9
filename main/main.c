/* main.c — 入口 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"
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

static void obd_task(void *pv)
{
    (void)pv;
    obd_init();

    static uint32_t conn_ready_ms = 0;
    static bool waiting = false;
    
    while (1) {
        if (conn_is_ready() && !obd_is_running()) {
            if (!waiting) {
                conn_ready_ms = lv_tick_get();
                waiting = true;
                ESP_LOGI(TAG, "OBD waiting for BT stabilization...");
            } else {
                /* 等待 300ms 让蓝牙链路稳定（进一步减少延迟）*/
                if (lv_tick_get() - conn_ready_ms >= 300) {
                    obd_start();
                    ESP_LOGI(TAG, "OBD auto started");
                    waiting = false;
                }
            }
        } else if (!conn_is_ready()) {
            waiting = false;  /* 蓝牙断开，重置等待状态 */
        }
        obd_update();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void key_task(void *pv)
{
    (void)pv;
    while (1) {
        key_poll();
        vTaskDelay(pdMS_TO_TICKS(20));  /* 50Hz 按键扫描 */
    }
}

static void ble_reconnect_task(void *pv)
{
    (void)pv;
    while (1) {
        bluetooth_auto_reconnect();
        vTaskDelay(pdMS_TO_TICKS(1000));  /* 每秒检查一次重连 */
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
    ble_enable();
    key_init();
    
    /* 初始化 OBD 消息队列（非阻塞数据同步）*/
    obd_queue_init();

    /* UI 初始化 */
    bsp_display_lock(-1);
    screen_init();
    bsp_display_unlock();

    xTaskCreatePinnedToCore(sensor_task,         "SENS",  4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(obd_task,           "OBD",   4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(key_task,           "KEY",   2048, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(ble_reconnect_task, "BLE",   4096, NULL, 2, NULL, 0);  /* 从2048增加到4096，防止栈溢出 */

    /* 主循环：只负责 UI 调度 */
    while (1) {
        bsp_display_lock(-1);
        screen_update();
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(16));  /* ~60Hz UI 更新（从30Hz提高）*/
    }
}