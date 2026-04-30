/* bluetooth.c — Bluetooth 界面业务 */
#include "bluetooth.h"
#include "ble.h"
#include "scan.h"
#include "conn.h"
#include "gui_guider.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "BTUI";

/* 外部 GUI 实例 */
extern lv_ui guider_ui;

/* 状态 */
static bool s_active = false;
static bool s_bt_on = true;          /* 蓝牙开关状态（默认开启） */
static int  s_last_count = -1;       /* 上次设备数 */
static int  s_enter_tick = 0;        /* 进入界面后的 tick 计数 */
static bool s_list_inited = false;   /* 列表是否已初始化 */
static bool s_was_connected = false; /* 上次连接状态，用于检测变化 */

/* 函数前向声明 */
static void refresh_list(void);
static void on_sw_enable(lv_event_t *e);
static void on_btn_scan(lv_event_t *e);
static void on_list_item(lv_event_t *e);
static void set_status_label(const char *text);

/* ========== 公共接口 ========== */

void bluetooth_enter(void)
{
    ESP_LOGI(TAG, "=== ENTER ===");
    s_active = true;
    s_enter_tick = 0;
    s_last_count = -1;

    /* 确保 NimBLE 已初始化后启用蓝牙 */
    if (!ble_is_enabled()) {
        ble_enable();
    }

    /* 初始化列表（只清一次） */
    if (!s_list_inited) {
        s_list_inited = true;
        /* 移除 GUI Guider 创建的默认 item */
        lv_obj_t *list = guider_ui.bluetooth_bt_list_devices;
        if (guider_ui.bluetooth_bt_list_devices_item0) {
            lv_obj_del(guider_ui.bluetooth_bt_list_devices_item0);
            guider_ui.bluetooth_bt_list_devices_item0 = NULL;
        }
        /* 添加事件回调 */
        lv_obj_add_event_cb(guider_ui.bluetooth_bt_sw_enable, on_sw_enable, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(guider_ui.bluetooth_bt_btn_scan, on_btn_scan, LV_EVENT_CLICKED, NULL);
    }

    /* 开关状态同步 */
    if (s_bt_on) {
        lv_obj_add_state(guider_ui.bluetooth_bt_sw_enable, LV_STATE_CHECKED);
        set_status_label("Scanning...");
    } else {
        lv_obj_clear_state(guider_ui.bluetooth_bt_sw_enable, LV_STATE_CHECKED);
        set_status_label("Bluetooth OFF");
    }
}

void bluetooth_exit(void)
{
    ESP_LOGI(TAG, "=== EXIT ===");
    s_active = false;
    s_was_connected = false;
    scan_stop();
}

void bluetooth_update(void)
{
    if (!s_active) return;

    /* 延迟启动扫描：等 NimBLE sync 后再扫 */
    if (s_bt_on) {
        s_enter_tick++;
        if (s_enter_tick == 30) {  /* ~480ms */
            ESP_LOGI(TAG, "delayed scan start");
            scan_start();
            s_last_count = -1;
        }

        /* 设备数量变化时刷新列表 */
        int count = scan_get_count();
        if (count != s_last_count) {
            ESP_LOGI(TAG, "count changed: %d -> %d", s_last_count, count);
            refresh_list();
            s_last_count = count;
            if (count > 0) {
                set_status_label("Devices found");
            }
        }

        /* 连接状态变化时刷新列表（已连接设备标绿色） */
        bool now_connected = ble_is_connected();
        if (now_connected != s_was_connected) {
            s_was_connected = now_connected;
            refresh_list();
            ESP_LOGI(TAG, "conn state: %d", now_connected);
        }
        if (now_connected) {
            set_status_label("Connected");
        }
    }
}

static void set_btn_style(lv_obj_t *btn)
{
    lv_obj_set_style_text_color(btn, lv_color_hex(0x0D3055), LV_PART_MAIN);
    lv_obj_set_style_text_font(btn, &lv_font_montserratMedium_16, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 3, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
}

/* 按 RSSI 降序排序（信号强的在前） */
static int cmp_idx_rssi_desc(const void *a, const void *b)
{
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    const scan_device_t *da = scan_get_device(ia);
    const scan_device_t *db = scan_get_device(ib);
    if (!da || !db) return 0;
    return db->rssi - da->rssi;  /* 降序：信号强的在前 */
}

/* 刷新设备列表（按信号强度排序，不显示 RSSI 数值） */
static void refresh_list(void)
{
    lv_obj_t *list = guider_ui.bluetooth_bt_list_devices;
    if (!list) return;

    lv_obj_clean(list);

    int count = scan_get_count();
    if (count == 0) {
        lv_obj_t *btn = lv_list_add_button(list, LV_SYMBOL_BLUETOOTH, "No devices found");
        set_btn_style(btn);
        return;
    }

    /* 构建索引数组并按 RSSI 排序 */
    int order[16];
    int n = count < 16 ? count : 16;
    for (int i = 0; i < n; i++) order[i] = i;
    qsort(order, n, sizeof(order[0]), cmp_idx_rssi_desc);

    for (int i = 0; i < n; i++) {
        int idx = order[i];
        const scan_device_t *d = scan_get_device(idx);
        if (!d) continue;

        char buf[64];
        if (strlen(d->name) > 0) {
            snprintf(buf, sizeof(buf), "%s", d->name);
        } else {
            snprintf(buf, sizeof(buf),
                "%02X:%02X:%02X:%02X:%02X:%02X",
                d->addr[0], d->addr[1], d->addr[2],
                d->addr[3], d->addr[4], d->addr[5]);
        }

        lv_obj_t *btn = lv_list_add_button(list, LV_SYMBOL_BLUETOOTH, buf);
        lv_obj_add_event_cb(btn, on_list_item, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
        set_btn_style(btn);

        /* 已连接设备标绿色 */
        const uint8_t *conn_addr = conn_get_connected_addr();
        if (conn_addr && ble_is_connected() && memcmp(d->addr, conn_addr, 6) == 0) {
            lv_obj_set_style_text_color(btn, lv_color_hex(0x00E676), LV_PART_MAIN);
        }
    }

    ESP_LOGI(TAG, "list refreshed: %d items", n);
}

/* 开关回调：开启/关闭蓝牙 */
static void on_sw_enable(lv_event_t *e)
{
    (void)e;
    lv_obj_t *sw = guider_ui.bluetooth_bt_sw_enable;
    bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);

    ESP_LOGI(TAG, "switch: %d", checked);

    if (checked) {
        s_bt_on = true;
        ble_enable();
        set_status_label("Scanning...");
        s_enter_tick = 25; /* 加快启动扫描 */
        s_last_count = -1;
    } else {
        s_bt_on = false;
        ble_disable();
        set_status_label("Bluetooth OFF");
        /* 清空列表 */
        lv_obj_clean(guider_ui.bluetooth_bt_list_devices);
        lv_obj_t *btn = lv_list_add_button(guider_ui.bluetooth_bt_list_devices, LV_SYMBOL_BLUETOOTH, "Bluetooth OFF");
        set_btn_style(btn);
    }
}

/* 扫描按钮回调 */
static void on_btn_scan(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "scan button clicked");

    if (!s_bt_on) {
        set_status_label("Enable Bluetooth first");
        return;
    }

    if (scan_is_running()) {
        scan_stop();
    }

    /* 重置并启动扫描 */
    s_last_count = -1;
    scan_start();
    set_status_label("Scanning...");

    /* 清空列表 */
    lv_obj_clean(guider_ui.bluetooth_bt_list_devices);
    lv_obj_t *btn = lv_list_add_button(guider_ui.bluetooth_bt_list_devices, LV_SYMBOL_BLUETOOTH, "Scanning...");
    set_btn_style(btn);
}

/* 列表项点击回调：连接设备 */
static void on_list_item(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_event_get_user_data(e);

    const scan_device_t *d = scan_get_device(idx);
    if (!d) return;

    ESP_LOGI(TAG, "connect to [%d] %s", idx, d->name[0] ? d->name : "unknown");
    set_status_label("Connecting...");

    /* 停止扫描 */
    scan_stop();

    /* 发起连接 */
    conn_connect(d->addr, true);
}

/* 设置状态标签文本 */
static void set_status_label(const char *text)
{
    if (guider_ui.bluetooth_bt_label_title) {
        /* 标题保持 "Bluetooth"，状态可以显示在子标题位置 */
        /* 这里我们保持标题不变，只在日志中记录 */
    }
    ESP_LOGI(TAG, "status: %s", text);
}