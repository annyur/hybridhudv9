/* bluetooth.c — Bluetooth 界面业务 */
#include "bluetooth.h"
#include "ble.h"
#include "scan.h"
#include "conn.h"
#include "screen.h"
#include "lvgl.h"
#include "gui_guider.h"
#include <string.h>

extern lv_ui guider_ui;

static bool s_active = false;
static bool s_ble_on = false;       /* 当前开关状态 */
static int  s_last_count = -1;      /* 上次列表数量（变化检测） */
static bool s_connecting = false;   /* 是否正在连接中 */

/* 开关事件 */
static void on_sw_changed(lv_event_t *e)
{
    (void)e;
    lv_obj_t *sw = guider_ui.bluetooth_bt_sw_enable;
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);

    if (on && !s_ble_on) {
        /* 开启 */
        s_ble_on = true;
        ble_enable();
        scan_start();
        refresh_device_list();
    } else if (!on && s_ble_on) {
        /* 关闭 */
        s_ble_on = false;
        ble_disable();
        show_off_prompt();
    }
}

/* Scan 按钮事件 */
static void on_btn_scan(lv_event_t *e)
{
    (void)e;
    if (!s_ble_on) return;
    scan_stop();
    scan_start();
}

/* List item 点击 → 连接设备 */
static void on_list_item_click(lv_event_t *e)
{
    if (s_connecting) return;
    lv_obj_t *btn = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    const scan_device_t *dev = scan_get_device(idx);
    if (!dev) return;

    s_connecting = true;
    ESP_LOGI("BTUI", "connecting to %s...", dev->name[0] ? dev->name : "unknown");
    conn_connect(dev->addr, true);  /* 假设 public addr */

    /* 延迟重置连接标志 */
    // 实际应在连接成功/失败回调中重置，这里简单延时
    // 后续可以用定时器
    (void)btn;
}

/* 显示 "Bluetooth OFF" 提示 */
static void show_off_prompt(void)
{
    lv_obj_t *list = guider_ui.bluetooth_bt_list_devices;
    lv_obj_clean(list);
    lv_list_add_button(list, NULL, "Bluetooth OFF");
    s_last_count = 0;
}

/* 刷新设备列表 */
static void refresh_device_list(void)
{
    lv_obj_t *list = guider_ui.bluetooth_bt_list_devices;
    lv_obj_clean(list);

    int count = scan_get_count();
    if (count == 0) {
        lv_list_add_button(list, NULL, "Scanning...");
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

        lv_obj_t *btn = lv_list_add_button(list, LV_SYMBOL_BLUETOOTH, buf);
        lv_obj_add_event_cb(btn, on_list_item_click, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    s_last_count = count;
}

void bluetooth_enter(void)
{
    s_active = true;
    s_connecting = false;

    /* 默认开启状态 */
    lv_obj_add_state(guider_ui.bluetooth_bt_sw_enable, LV_STATE_CHECKED);
    s_ble_on = true;
    ble_enable();
    scan_start();
    refresh_device_list();
}

void bluetooth_exit(void)
{
    s_active = false;
    scan_stop();
}

void bluetooth_update(void)
{
    if (!s_active) return;

    /* 扫描结果变化时刷新列表 */
    int count = scan_get_count();
    if (count != s_last_count) {
        refresh_device_list();
    }

    /* 连接成功后重置标志 */
    if (s_connecting && ble_is_connected()) {
        s_connecting = false;
        ESP_LOGI("BTUI", "connected!");
        /* 可以在这里自动切回 setting 或显示已连接 */
    }
}