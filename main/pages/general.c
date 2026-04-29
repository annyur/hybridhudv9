/* general.c — General 界面业务 */
#include "general.h"
#include "obd.h"
#include "ble.h"
#include "imu.h"
#include "rtc.h"
#include <lvgl.h>
#include "gui_guider.h"   /* GUI Guider 生成的 UI 结构体 */

extern lv_ui guider_ui;

static bool s_active = false;

void general_enter(void)
{
    s_active = true;
}

void general_exit(void)
{
    s_active = false;
}

void general_update(void)
{
    if (!s_active) return;

    const obd_data_t *d = obd_get_data();
    (void)d;

    /* 你的数据绑定逻辑写在这里 */
}