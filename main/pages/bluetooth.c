/* bluetooth.c — Bluetooth 界面业务 */
#include "bluetooth.h"
#include "ble.h"
#include "scan.h"
#include "conn.h"
#include "lvgl.h"
#include "gui_guider.h"
#include <string.h>

extern lv_ui guider_ui;

static bool s_active = false;

void bluetooth_enter(void)
{
    s_active = true;
    ble_enable();
    scan_start();
}

void bluetooth_exit(void)
{
    s_active = false;
    scan_stop();
}

void bluetooth_update(void)
{
    if (!s_active) return;

    /* 后续：把 scan_get_count() / scan_get_device() 的结果刷到 list 上 */
    int count = scan_get_count();
    bool connected = ble_is_connected();
    (void)count;
    (void)connected;
}