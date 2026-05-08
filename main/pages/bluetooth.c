/* bluetooth.c — Bluetooth 界面业务（修复：事件注册移出切换路径，防止卡死） */
#include "bluetooth.h"
#include "ble.h"
#include "scan.h"
#include "conn.h"
#include "host/ble_gap.h"
#include "screen.h"
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
#define MAX_RECONNECT_ATTEMPTS 3  /* 最大重连次数 */

/* 上次连接设备 */
static uint8_t s_last_addr[6] = {0};
static bool    s_has_last = false;

/* 状态 */
static bool s_active = false;
static int  s_last_count = -1;
static int  s_enter_tick = 0;
static bool s_ui_inited = false;
static bool s_was_connected = false;
static int  s_conn_idx = -1;

/* 重连状态（模块级，可被重置函数访问）*/
static int  s_reconnect_state = 0;   /* 0=等待 1=已尝试 2=成功 3=放弃 */
static int  s_retry = 0;            /* 重试计数器 */
static uint32_t s_connect_start_ms = 0;
static uint32_t s_fail_cooldown_ms = 0;  /* 连接失败后的冷却时间 */

/* 函数前向声明 */
static void refresh_list(void);
static void on_btn_scan(lv_event_t *e);
static void on_btn_back(lv_event_t *e);
static void on_list_item(lv_event_t *e);
static void set_title_label(const char *text);
static void set_btn_style(lv_obj_t *btn);

/* ========== NVS：上次连接地址 ========== */
static void nvs_save_last_addr(const uint8_t *addr)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_BT, NVS_READWRITE, &h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s (NVS partition may be corrupted)", esp_err_to_name(ret));
        return;
    }
    nvs_set_blob(h, NVS_KEY_LAST, addr, 6);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "last addr saved");
}

static void nvs_load_last_addr(void)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NVS_NS_BT, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s (NVS partition may be corrupted)", esp_err_to_name(ret));
        return;
    }

    size_t len = 6;
    ret = nvs_get_blob(h, NVS_KEY_LAST, s_last_addr, &len);
    nvs_close(h);

    s_has_last = (ret == ESP_OK && len == 6);
    if (s_has_last) {
        ESP_LOGI(TAG, "last addr loaded: %02X:%02X:%02X:%02X:%02X:%02X",
                 s_last_addr[0], s_last_addr[1], s_last_addr[2],
                 s_last_addr[3], s_last_addr[4], s_last_addr[5]);
    }
}

/* ========== UI 初始化（screen_init 时调用一次，绝不在 screen_switch 路径中） ========== */
void bluetooth_ui_init(void)
{
    if (s_ui_inited) return;
    s_ui_inited = true;

    ESP_LOGI(TAG, "ui_init start");

    /* 移除 GUI Guider 创建的默认 item */
    if (guider_ui.bluetooth_bt_list_devices_item0) {
        lv_obj_del(guider_ui.bluetooth_bt_list_devices_item0);
        guider_ui.bluetooth_bt_list_devices_item0 = NULL;
    }

    /* 注册事件回调 — 必须在 LVGL 锁内，但必须在非切换路径中调用 */
    if (guider_ui.bluetooth_bt_btn_scan) {
        lv_obj_add_event_cb(guider_ui.bluetooth_bt_btn_scan, on_btn_scan, LV_EVENT_CLICKED, NULL);
    }
    if (guider_ui.bluetooth_btn_back) {
        lv_obj_add_event_cb(guider_ui.bluetooth_btn_back, on_btn_back, LV_EVENT_CLICKED, NULL);
    }

    ESP_LOGI(TAG, "ui_init done");
}

/* ========== 后台自动回连（main.c 循环调用，无需进入界面） ========== */
void bluetooth_auto_reconnect(void)
{
    static int s_tick = 0;

    /* 成功或放弃后不再尝试 */
    if (s_reconnect_state == 2 || s_reconnect_state == 3) return;

    /* 检查冷却期（固定5秒，给 NimBLE 更多清理时间） */
    uint32_t now = lv_tick_get();
    if (s_fail_cooldown_ms > 0) {
        if (now - s_fail_cooldown_ms < 5000) {
            return;  /* 冷却期内不尝试连接 */
        }
        s_fail_cooldown_ms = 0;
    }

    s_tick++;
    if (s_tick < 3) return;      /* ~300ms 快速等待 NimBLE 就绪 */

    if (!s_has_last) {
        nvs_load_last_addr();      /* 提前加载地址 */
    }

    if (!s_has_last) {
        ESP_LOGW(TAG, "no last addr, skip auto reconnect");
        s_reconnect_state = 3;
        return;
    }

    /* 检查蓝牙是否已连接 */
    if (ble_is_connected()) {
        if (s_reconnect_state != 2) {
            ESP_LOGI(TAG, "bluetooth connected, reset retry counter");
            s_reconnect_state = 2;
            s_retry = 0;
        }
        return;
    }

    /* 蓝牙断开后的重连逻辑 */
    if (s_reconnect_state == 2) {
        /* 之前已连接过，现在断开了，开始重连流程 */
        ESP_LOGI(TAG, "bluetooth disconnected, reconnect attempt %d/%d", s_retry + 1, MAX_RECONNECT_ATTEMPTS);
        s_reconnect_state = 0;  /* 进入等待连接状态 */
        s_fail_cooldown_ms = now;  /* 立即设置冷却期，确保有时间间隔 */
        s_retry++;
    }

    /* 检查重试次数是否超过限制（断开后最多尝试3次） */
    if (s_retry > MAX_RECONNECT_ATTEMPTS) {
        ESP_LOGW(TAG, "reconnect failed %d times, stop retrying", MAX_RECONNECT_ATTEMPTS);
        s_reconnect_state = 3;  /* 放弃 */
        s_retry = 0;
        return;
    }

    /* 连接超时检测：如果发起连接后1秒仍未成功，重试 */
    if (s_reconnect_state == 1) {
        if (now - s_connect_start_ms > 1000) {
            ESP_LOGW(TAG, "connect timeout, retry=%d", s_retry);
            /* 连接失败后先断开，确保 NimBLE 栈状态正确 */
            conn_disconnect();
            s_reconnect_state = 0;  /* 重置状态 */
            s_fail_cooldown_ms = now;  /* 设置冷却期 */
            s_retry++;  /* 增加重试计数 */
        }
        return;
    }

    /* 发起新连接 */
    if (s_reconnect_state == 0) {
        /* 检查是否已达到最大重试次数 */
        if (s_retry >= MAX_RECONNECT_ATTEMPTS) {
            ESP_LOGW(TAG, "reconnect failed %d times, stop retrying", MAX_RECONNECT_ATTEMPTS);
            s_reconnect_state = 3;  /* 放弃 */
            s_retry = 0;
            return;
        }
        ESP_LOGI(TAG, "auto reconnect last device... (attempt %d/%d)", s_retry + 1, MAX_RECONNECT_ATTEMPTS);
        conn_connect(s_last_addr, BLE_ADDR_PUBLIC);
        s_reconnect_state = 1;  /* 已发起连接 */
        s_connect_start_ms = now;
    }
}

/* ========== 公共接口 ========== */
void bluetooth_reset_reconnect_state(void)
{
    /* 重置自动重连状态，允许重新开始连接流程 */
    s_reconnect_state = 0;
    s_retry = 0;
    s_fail_cooldown_ms = 0;
    ESP_LOGI(TAG, "reconnect state reset, ready to retry");
}

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

    /* 蓝牙默认启用，进入界面自动开始扫描 */
    if (!ble_is_enabled()) {
        ble_enable();
    }
    set_title_label("Scanning...");
}

void bluetooth_exit(void)
{
    ESP_LOGI(TAG, "=== EXIT ===");
    s_active = false;
    s_was_connected = false;
    s_conn_idx = -1;
    /* 退出界面时只停止扫描，不停止 BLE 和 OBD（后台继续工作） */
    scan_stop();
}

void bluetooth_update(void)
{
    if (!s_active) return;

    /* 延迟启动扫描 */
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
            set_title_label("Devices found");
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
            set_title_label("Connected");
            ESP_LOGI(TAG, "connected, idx=%d", s_conn_idx);
        } else {
            s_conn_idx = -1;
            ESP_LOGI(TAG, "disconnected");
            set_title_label("Scanning...");
        }
        refresh_list();
    }
}

/* ========== 辅助函数 ========== */
static void set_title_label(const char *text)
{
    if (guider_ui.bluetooth_bt_label_title) {
        lv_label_set_text(guider_ui.bluetooth_bt_label_title, text);
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

    if (ia == s_conn_idx) return -1;
    if (ib == s_conn_idx) return 1;

    const scan_device_t *da = scan_get_device(ia);
    const scan_device_t *db = scan_get_device(ib);
    if (!da || !db) return 0;

    if (s_has_last) {
        bool ma = (memcmp(da->addr, s_last_addr, 6) == 0);
        bool mb = (memcmp(db->addr, s_last_addr, 6) == 0);
        if (ma && !mb) return -1;
        if (!ma && mb) return 1;
    }
    return db->rssi - da->rssi;
}

static void refresh_list(void)
{
    lv_obj_t *list = guider_ui.bluetooth_bt_list_devices;
    if (!list) return;

    /* 清理旧列表项 - lv_obj_clean 会递归删除所有子对象并释放资源 */
    lv_obj_clean(list);

    int count = scan_get_count();

    bool last_in_list = false;
    if (s_has_last && count > 0) {
        for (int i = 0; i < count; i++) {
            const scan_device_t *d = scan_get_device(i);
            if (d && memcmp(d->addr, s_last_addr, 6) == 0) { last_in_list = true; break; }
        }
    }

    int total = count + (s_has_last && !last_in_list ? 1 : 0);
    if (total == 0) return;

    int *order = (int *)malloc(sizeof(int) * total);
    if (!order) {
        ESP_LOGE(TAG, "refresh_list: malloc failed");
        return;
    }

    for (int i = 0; i < count; i++) order[i] = i;
    if (s_has_last && !last_in_list) order[count] = -1;

    qsort(order, total, sizeof(int), cmp_idx_conn_rssi);

    for (int i = 0; i < total; i++) {
        int idx = order[i];
        char label[64];

        if (idx == -1) {
            /* 上次连接的设备不在扫描列表中 */
            snprintf(label, sizeof(label), "Last: %02X:%02X:%02X:%02X:%02X:%02X",
                     s_last_addr[0], s_last_addr[1], s_last_addr[2],
                     s_last_addr[3], s_last_addr[4], s_last_addr[5]);
            lv_obj_t *btn = lv_list_add_button(list, NULL, label);
            if (btn) {
                set_btn_style(btn);
                lv_obj_set_style_bg_color(btn, lv_color_hex(0xdddddd), LV_PART_MAIN);
                lv_obj_add_event_cb(btn, on_list_item, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
            }
            continue;
        }

        const scan_device_t *d = scan_get_device(idx);
        if (!d) continue;

        bool is_connected = (idx == s_conn_idx);
        bool is_last = (memcmp(d->addr, s_last_addr, 6) == 0);

        snprintf(label, sizeof(label), "%s%s",
                 is_connected ? "[C] " : (is_last ? "[L] " : ""),
                 d->name[0] ? d->name : "Unknown");

        lv_obj_t *btn = lv_list_add_button(list, NULL, label);
        if (btn) {
            set_btn_style(btn);
            if (is_connected) {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x00ff24), LV_PART_MAIN);
            } else if (is_last) {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0xdddddd), LV_PART_MAIN);
            }
            lv_obj_add_event_cb(btn, on_list_item, LV_EVENT_CLICKED, (void *)(intptr_t)idx);
        }
    }

    free(order);
}

static void on_btn_scan(lv_event_t *e)
{
    (void)e;
    if (!scan_is_running()) {
        scan_start();
        set_title_label("Scanning...");
    } else {
        scan_stop();
        set_title_label("Scan stopped");
    }
}

static void on_btn_back(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "back btn → switch to RACE");
    screen_switch(SCREEN_RACE);
}

static void on_list_item(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);

    if (idx == -1) {
        /* 点击上次连接的设备 */
        if (s_has_last) {
            ESP_LOGI(TAG, "connect to last");
            conn_connect(s_last_addr, BLE_ADDR_PUBLIC);
        }
        return;
    }

    const scan_device_t *d = scan_get_device(idx);
    if (!d) return;

    ESP_LOGI(TAG, "connect to [%d] %s", idx, d->name);
    set_title_label("Connecting...");
    conn_connect(d->addr, d->addr_type);
}