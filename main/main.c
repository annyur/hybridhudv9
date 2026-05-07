/* main.c — 入口 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"

#include "lvgl.h"
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

static void service_task(void *pv)
{
    (void)pv;
    uint32_t last_key_scan = 0;
    const uint32_t key_scan_interval = 33;   /* 30Hz 按键扫描 */

    /* 初始化事件驱动的BLE重连（仅执行一次） */
    bluetooth_reconnect_init();

    while (1) {
        uint32_t now = lv_tick_get();

        /* 按键扫描 30Hz */
        if (now - last_key_scan >= key_scan_interval) {
            last_key_scan = now;
            key_poll();
        }

        /* BLE重连现在由事件驱动，不再需要轮询 */

        vTaskDelay(pdMS_TO_TICKS(10));  /* 100Hz 服务任务轮询 */
    }
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
                /* 等待 300ms 让蓝牙链路稳定 */
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
    hud_imu_init();   /* 在 service_task 运行前初始化 IMU */
    hud_rtc_init();   /* 初始化 RTC */

    /* 初始化 OBD 消息队列（非阻塞数据同步）*/
    obd_queue_init();

    /* UI 初始化 */
    bsp_display_lock(-1);
    screen_init();
    bsp_display_unlock();

    /* 创建任务：OBD(优先级5) + 综合服务(优先级3) */
    xTaskCreatePinnedToCore(obd_task,     "OBD",     4096, NULL, 5, NULL, 0);  /* OBD数据采集 */
    xTaskCreatePinnedToCore(service_task, "SERVICE", 4096, NULL, 3, NULL, 0);  /* 按键+蓝牙+综合服务 */

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