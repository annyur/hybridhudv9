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

/* OBD 任务状态 */
static uint32_t s_conn_ready_ms = 0;
static bool s_obd_waiting = false;

static void sensor_task(void *pv)
{
    hud_imu_init();
    hud_rtc_init();
    /* 初始化后可以删除任务，节省资源 */
    vTaskDelete(NULL);
}

static void obd_task(void *pv)
{
    (void)pv;
    obd_init();
    
    while (1) {
        if (conn_is_ready() && !obd_is_running()) {
            if (!s_obd_waiting) {
                s_conn_ready_ms = lv_tick_get();
                s_obd_waiting = true;
                ESP_LOGI(TAG, "OBD waiting for BT stabilization...");
            } else {
                /* 等待 300ms 让蓝牙链路稳定（进一步减少延迟）*/
                if (lv_tick_get() - s_conn_ready_ms >= 300) {
                    obd_start();
                    ESP_LOGI(TAG, "OBD auto started");
                    s_obd_waiting = false;
                }
            }
        } else if (!conn_is_ready()) {
            s_obd_waiting = false;  /* 蓝牙断开，重置等待状态 */
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
        vTaskDelay(pdMS_TO_TICKS(33));  /* 30Hz 按键扫描（降低轮询频率，减少CPU开销）*/
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

    xTaskCreatePinnedToCore(sensor_task,         "SENS",  2048, NULL, 3, NULL, 1);  /* 减少栈大小 */
    xTaskCreatePinnedToCore(obd_task,           "OBD",   3584, NULL, 5, NULL, 0);  /* 调整到更合理的大小 */
    xTaskCreatePinnedToCore(key_task,           "KEY",   1536, NULL, 4, NULL, 0);  /* 减少栈大小 */
    xTaskCreatePinnedToCore(ble_reconnect_task, "BLE",   2048, NULL, 2, NULL, 0);  /* 减少栈大小 */

    /* 主循环：精确 60Hz UI 更新 */
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(16);  /* 16ms ≈ 60Hz */
    
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);  /* 精确帧间隔 */
        
        bsp_display_lock(-1);
        screen_update();
        bsp_display_unlock();
    }
}