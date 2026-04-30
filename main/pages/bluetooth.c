/* bluetooth.c — Bluetooth 界面业务 */
#include "bluetooth.h"
#include "ble.h"
#include "scan.h"
#include "conn.h"
#include "lvgl.h"
#include "gui_guider.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

extern lv_ui guider_ui;

static bool s_active = false;
static bool s_ble_on = false;
static int  s_last_count = -1;
static bool s_connecting = false;
static bool s_events_bound = false;
static bool s_wait_scan = false;

static void refresh_list(void);
static void show_off(void);

static bool get_sw_state(void)
{
    return lv_obj_has_state(guider_ui.bluetooth_bt_sw_enable, LV_STATE_CHECKED);
}

static void set_sw_state(bool on)
{
    if (on) lv_obj_add_state(guider_ui.bluetooth_bt_sw_enable, LV_STATE_CHECKED);
    else    lv_obj_clear_state(guider_ui.bluetooth_bt_sw_enable, LV_STATE_CHECKED);
}

static void on_btn_scan(lv_event_t *e)
{
    (void)e;
    if (!s_ble_on) return;
    scan_stop();
    scan_start();
    refresh_list();
}

static void on_sw_changed(lv_event_t *e)
{
    (void)e;
    bool on = get_sw_state();

    if (on && !s_ble_on) {
        s_ble_on = true;
        ble_enable();
        s_wait_scan = true;
        refresh_list();
    }
    else if (!on && s_ble_on) {
        s_ble_on = false;
        s_wait_scan = false;
        ble_disable();
        show_off();
    }
}

static void on_item_click(lv_event_t *e)
{
    if (s_connecting) return;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    const scan_device_t *dev = scan_get_device(idx);
    if (!dev) return;

    s_connecting = true;
    ESP_LOGI("BTUI", "connecting to %s...", dev->name[0] ? dev->name : "unknown");
    conn_connect(dev->addr, true);
}

static void show_off(void)
{
    lv_obj_t *list = guider_ui.bluetooth_bt_list_devices;
    lv_obj_clean(list);
    lv_list_add_btn(list, NULL, "Bluetooth OFF");
    s_last_count = 0;
}

static void refresh_list(void)
{
    lv_obj_t *list = guider_ui.bluetooth_bt_list_devices;
    lv_obj_clean(list);

    int count = scan_get_count();

    if (!s_ble_on) {
        show_off();
        return;
    }

    if (scan_is_running() && count == 0) {
        lv_list_add_btn(list, NULL, "Scanning...");
        s_last_count = 0;
        return;
    }

    for (int i = 0; i < count; i++) {
        const scan_device_t *dev = scan_get_device(i);
        if (!dev) continue;

        char buf[64];
        if (dev->name[0]) {
            snprintf(buf, sizeof(buf), "%s  (%d dBm)", dev->name, dev->rssi);
        } else {
            snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X  (%d dBm)",
                     dev->addr[0], dev->addr[1], dev->addr[2],
                     dev->addr[3], dev->addr[4], dev->addr[5], dev->rssi);
        }

        lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_BLUETOOTH, buf);
        lv_obj_add_event_cb(btn, on_item_click, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    s_last_count = count;
}

void bluetooth_enter(void)
{
    s_active = true;

    if (!s_events_bound) {
        s_events_bound = true;
        lv_obj_add_event_cb(guider_ui.bluetooth_bt_sw_enable, on_sw_changed,
                            LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(guider_ui.bluetooth_bt_btn_scan, on_btn_scan,
                            LV_EVENT_CLICKED, NULL);
    }

    set_sw_state(true);
    s_ble_on = true;
    ble_enable();
    s_wait_scan = true;
    refresh_list();
}

void bluetooth_exit(void)
{
    s_active = false;
}

void bluetooth_update(void)
{
    if (!s_active) return;

    if (s_wait_scan && ble_is_enabled()) {
        s_wait_scan = false;
        scan_start();
    }

    int count = scan_get_count();
    if (count != s_last_count) {
        refresh_list();
    }

    if (s_connecting && ble_is_connected()) {
        s_connecting = false;
        ESP_LOGI("BTUI", "connected!");
    }
}