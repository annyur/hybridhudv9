/* screen.c -- simplified: race + bluetooth only */
#include "gui_guider.h"
#include "events_init.h"
#include "race.h"
#include "bluetooth.h"
#include "screen.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* guider_ui 全局变量定义（GUI Guider 生成代码中缺少定义） */
lv_ui guider_ui;

static screen_id_t s_current = SCREEN_RACE;
static uint32_t    s_last_switch_ms = 0;
static screen_id_t s_switch_request = SCREEN_NONE;  /* 屏幕切换请求标志 */
static SemaphoreHandle_t s_screen_mutex = NULL;     /* 屏幕状态互斥锁 */

void screen_init(void)
{
    /* 创建互斥锁保护屏幕状态 */
    s_screen_mutex = xSemaphoreCreateMutex();
    if (s_screen_mutex == NULL) {
        ESP_LOGE("SCREEN", "Failed to create screen mutex");
    }

    setup_ui(&guider_ui);
    events_init(&guider_ui);

    /* setup_ui() 只创建了 race screen，手动创建 bluetooth screen */
    setup_scr_bluetooth(&guider_ui);

    /* 蓝牙界面事件注册 — 必须在 setup_ui 之后，且不在 screen_switch 路径中 */
    bluetooth_ui_init();

    s_current = SCREEN_RACE;
    s_switch_request = SCREEN_NONE;
    lv_screen_load(guider_ui.race);
    race_enter();
}

void screen_request_switch(screen_id_t id)
{
    if (id == SCREEN_NONE) return;

    /* 加锁保护共享变量访问 */
    if (s_screen_mutex && xSemaphoreTake(s_screen_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (id != s_current) {
            s_switch_request = id;
        }
        xSemaphoreGive(s_screen_mutex);
    }
}

static void screen_switch_internal(screen_id_t id)
{
    /* 加锁保护屏幕切换 */
    if (s_screen_mutex && xSemaphoreTake(s_screen_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        screen_id_t old_current = s_current;

        if (id == old_current) {
            xSemaphoreGive(s_screen_mutex);
            return;
        }

        uint32_t now = lv_tick_get();
        if (now - s_last_switch_ms < 200) {
            xSemaphoreGive(s_screen_mutex);
            return;
        }
        s_last_switch_ms = now;

        if (old_current == SCREEN_RACE)      race_exit();
        if (old_current == SCREEN_BLUETOOTH) bluetooth_exit();
        s_current = id;

        xSemaphoreGive(s_screen_mutex);

        /* LVGL 操作不需要锁保护 */
        lv_obj_t *target = (id == SCREEN_RACE) ? guider_ui.race : guider_ui.bluetooth;
        if (target) lv_screen_load(target);

        if (id == SCREEN_RACE)      race_enter();
        if (id == SCREEN_BLUETOOTH) bluetooth_enter();
    }
}

void screen_switch(screen_id_t id)
{
    screen_request_switch(id);
}

screen_id_t screen_current(void)
{
    screen_id_t current = SCREEN_RACE;
    if (s_screen_mutex && xSemaphoreTake(s_screen_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        current = s_current;
        xSemaphoreGive(s_screen_mutex);
    }
    return current;
}

void screen_update(void)
{
    screen_id_t req_id = SCREEN_NONE;

    /* 加锁读取切换请求 */
    if (s_screen_mutex && xSemaphoreTake(s_screen_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        req_id = s_switch_request;
        xSemaphoreGive(s_screen_mutex);
    }

    /* 处理屏幕切换请求（在主循环中执行，避免阻塞其他任务） */
    if (req_id != SCREEN_NONE) {
        screen_switch_internal(req_id);
        /* 清除请求标志 */
        if (s_screen_mutex && xSemaphoreTake(s_screen_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            s_switch_request = SCREEN_NONE;
            xSemaphoreGive(s_screen_mutex);
        }
        return;  /* 切换完成后跳过本次更新 */
    }

    /* 加锁读取当前屏幕 */
    screen_id_t current = SCREEN_RACE;
    if (s_screen_mutex && xSemaphoreTake(s_screen_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        current = s_current;
        xSemaphoreGive(s_screen_mutex);
    }

    if (current == SCREEN_RACE) {
        race_update();
    } else if (current == SCREEN_BLUETOOTH) {
        bluetooth_update();
    }
}