/* bluetooth.c — Bluetooth 界面业务 */
#include "bluetooth.h"
#include "ble.h"
#include "scan.h"
#include "conn.h"
#include "host/ble_gap.h"
#include "gui_guider.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "BTUI";

/* 外部 GUI 实例 */
extern lv_ui guider_ui;

/* NVS 键 */
#define NVS_NS_BT   "btns"
#define NVS_KEY_LAST "last_addr"

/* 上次连接设备 */
static uint8_t s_last_addr[6] = {0};
static bool    s_has_last = false;

/* 状态 */
static bool s_active = false;
static bool s_bt_on = true;
static int  s_last_count = -1;
static int  s_enter_tick = 0;
static bool s_list_inited = false;
static bool s_was_connected = false;
static int  s_conn_idx = -1;

/* 函数前向声明 */
static void refresh_list(void);
static void on_sw_enable(lv_event_t *e);
static void on_btn_scan(lv_event_t *e);
static void on_list_item(lv_event_t *e);
static void set_status_label(const char *text);

/* ========== NVS：上次连接地址 ========== */
static void nvs_save_last_addr(const uint8_t *addr)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS_BT, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, NVS_KEY_LAST, addr, 6);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "last addr saved");
}

static void nvs_load_last_addr(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS_BT, NVS_READONLY, &h) != ESP_OK) return;

    size_t len = 6;
    esp_err_t ret = nvs_get_blob(h, NVS_KEY_LAST, s_last_addr, &len);
    nvs_close(h);

    s_has_last = (ret == ESP_OK && len == 6);
    if (s_has_last) {
        ESP_LOGI(TAG, "last addr loaded: %02X:%02X:%02X:%02X:%02X:%02X",
                 s_last_addr[0], s_last_addr[1], s_last_addr[2],
                 s_last_addr[3], s_last_addr[4], s_last_addr[5]);
    }
}

/* ========== 公共接口 ========== */

void bluetooth_enter(void)
{
    ESP_LOGI(TAG, "=== ENTER ===");
    s_active = true;
    s_enter_tick = 0;
    s_last_count = -1;

    /* 加载上次连接地址 */
    if (!s_has_last) {
        nvs_load_last_addr();
    }

    /* 确保 NimBLE 已初始化后启用蓝牙 */
    if (!ble_is_enabled()) {
        ble_enable();
    }

    /* 初始化列表（只清一次） */
    if (!s_list_inited) {
        s_list_inited = true;
        /* 移除 GUI Guider 创建的默认 item */
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
    s_conn_idx = -1;
    scan_stop();
}

void bluetooth_update(void)
{
    if (!s_active) return;

    /* 延迟启动扫描 */
    if (s_bt_on) {
        s_enter_tick++;
        if (s_enter_tick == 30) {
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

        /* 连接状态变化 */
        bool now_connected = ble_is_connected();
        if (now_connected != s_was_connected) {
            s_was_connected = now_connected;
            if (now_connected) {
                /* 找到已连接设备的索引 */
                const uint8_t *ca = conn_get_connected_addr();
                s_conn_idx = -1;
                if (ca) {
                    /* 保存到 NVS */
                    nvs_save_last_addr(ca);
                    memcpy(s_last_addr, ca, 6);
                    s_has_last = true;

                    for (int i = 0; i < scan_get_count(); i++) {
                        const scan_device_t *d = scan_get_device(i);
                        if (d && memcmp(d->addr, ca, 6) == 0) {
                            s_conn_idx = i;
                            break;
                        }
                    }
                }
                set_status_label("Connected");
                ESP_LOGI(TAG, "connected, idx=%d", s_conn_idx);
            } else {
                s_conn_idx = -1;
                ESP_LOGI(TAG, "disconnected");
                /* 断开且有上次记录 → 延迟回连 */
                if (s_has_last) {
                    ESP_LOGI(TAG, "auto reconnect in 2s...");
                }
            }
            refresh_list();
        }

        /* 断开状态且有上次记录 → 自动回连 */
        if (!now_connected && s_has_last && s_enter_tick > 150 && (s_enter_tick % 100) == 0) {
            ESP_LOGI(TAG, "auto reconnecting last device...");
            conn_connect(s_last_addr, BLE_ADDR_PUBLIC);
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

/* 排序：已连接设备置顶，有记录的排第二，其余按 RSSI 降序 */
static int cmp_idx_conn_rssi(const void *a, const void *b)
{
    int ia = *(const int *)a;
    int ib = *(const int *)b;

    /* 已连接设备排最前面 */
    if (ia == s_conn_idx) return -1;
    if (ib == s_conn_idx) return 1;

    const scan_device_t *da = scan_get_device(ia);
    const scan_device_t *db = scan_get_device(ib);
    if (!da || !db) return 0;

    /* 上次连接设备排第二 */
    if (s_has_last) {
        bool ma = (memcmp(da->addr, s_last_addr, 6) == 0);
        bool mb = (memcmp(db->addr, s_last_addr, 6) == 0);
        if (ma && !mb) return -1;
        if (!ma && mb) return 1;
    }

    /* 其余按 RSSI 降序 */
    return db->rssi - da->rssi;
}

/* 刷新设备列表 */
static void refresh_list(void)
{
    lv_obj_t *list = guider_ui.bluetooth_bt_list_devices;
    if (!list) return;

    lv_obj_clean(list);

    int count = scan_get_count();

    /* 有上次记录且不在扫描列表中 → 先显示 Last Bluetooth */
    bool last_in_list = false;
    if (s_has_last && count > 0) {
        for (int i = 0; i < count; i++) {
            const scan_device_t *d = scan_get_device(i);
            if (d && memcmp(d->addr, s_last_addr, 6) == 0) {
                last_in_list = true;
                break;
            }
        }
    }

    if (count == 0 && !s_has_last) {
        lv_obj_t *btn = lv_list_add_button(list, LV_SYMBOL_BLUETOOTH, "No devices found");
        set_btn_style(btn);
        return;
    }

    /* 构建索引数组 */
    int order[16];
    int n = count < 16 ? count : 16;
    for (int i = 0; i < n; i++) order[i] = i;
    qsort(order, n, sizeof(order[0]), cmp_idx_conn_rssi);

    /* 如果上次记录不在列表中，先插入 Last Bluetooth 项 */
    if (s_has_last && !last_in_list) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Last Bluetooth  (%02X:%02X:%02X:%02X:%02X:%02X)",
                 s_last_addr[0], s_last_addr[1], s_last_addr[2],
                 s_last_addr[3], s_last_addr[4], s_last_addr[5]);
        lv_obj_t *btn = lv_list_add_button(list, LV_SYMBOL_BLUETOOTH, buf);
        set_btn_style(btn);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
        /* 点击回连 */
        lv_obj_add_event_cb(btn, on_list_item, LV_EVENT_CLICKED, (void *)(intptr_t)(-1));
    }

    for (int i = 0; i < n; i++) {
        int idx = order[i];
        const scan_device_t *d = scan_get_device(idx);
        if (!d) continue;

        char buf[64];
        bool is_last = (s_has_last && memcmp(d->addr, s_last_addr, 6) == 0);

        if (strlen(d->name) > 0) {
            if (is_last) {
                snprintf(buf, sizeof(buf), "Last: %s", d->name);
            } else {
                snprintf(buf, sizeof(buf), "%s", d->name);
            }
        } else {
            snprintf(buf, sizeof(buf),
                "%02X:%02X:%02X:%02X:%02X:%02X",
                d->addr[0], d->addr[1], d->addr[2],
                d->addr[3], d->addr[4], d->addr[5]);
        }

        lv_obj_t *btn = lv_list_add_button(list, LV_SYMBOL_BLUETOOTH, buf);
        lv_obj_add_event_cb(btn, on_list_item, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
        set_btn_style(btn);

        /* 上次连接设备标灰色，已连接标绿色 */
        const uint8_t *conn_addr = conn_get_connected_addr();
        if (conn_addr && ble_is_connected() && memcmp(d->addr, conn_addr, 6) == 0) {
            lv_obj_set_style_text_color(btn, lv_color_hex(0x00E676), LV_PART_MAIN);
        } else if (is_last) {
            lv_obj_set_style_text_color(btn, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
        }
    }

    ESP_LOGI(TAG, "list refreshed: %d items", n);
}

/* 开关回调 */
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
        s_enter_tick = 25;
        s_last_count = -1;
    } else {
        s_bt_on = false;
        ble_disable();
        set_status_label("Bluetooth OFF");
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

    s_last_count = -1;
    scan_start();
    set_status_label("Scanning...");

    lv_obj_clean(guider_ui.bluetooth_bt_list_devices);
    lv_obj_t *btn = lv_list_add_button(guider_ui.bluetooth_bt_list_devices, LV_SYMBOL_BLUETOOTH, "Scanning...");
    set_btn_style(btn);
}

/* 列表项点击回调 */
static bool s_connecting = false;

static void on_list_item(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);

    if (s_connecting) {
        ESP_LOGW(TAG, "connection in progress, ignore click");
        return;
    }

    /* idx = -1 表示点击 Last Bluetooth（回连上次设备） */
    if (idx == -1) {
        if (!s_has_last) return;
        s_connecting = true;
        ESP_LOGI(TAG, "reconnect last device");
        set_status_label("Reconnecting...");
        scan_stop();
        conn_connect(s_last_addr, BLE_ADDR_PUBLIC);
        return;
    }

    const scan_device_t *d = scan_get_device(idx);
    if (!d) return;

    s_connecting = true;
    ESP_LOGI(TAG, "connect to [%d] %s", idx, d->name[0] ? d->name : "unknown");
    set_status_label("Connecting...");

    scan_stop();
    conn_connect(d->addr, d->addr_type);
}

/* 设置状态标签文本 */
static void set_status_label(const char *text)
{
    ESP_LOGI(TAG, "status: %s", text);
}