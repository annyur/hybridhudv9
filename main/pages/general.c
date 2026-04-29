/* general.c — General 界面业务 */
#include "general.h"
#include "obd.h"
#include "ble.h"
#include "imu.h"
#include "rtc.h"
#include <lvgl.h>

/* 外部引用 GUI Guider 生成的 UI 对象 */
extern gui_obj_t gui;

static bool s_active = false;

void general_enter(void)
{
    s_active = true;
    /* 禁用 Arc / Slider 触摸 */
    /* lv_obj_clear_flag(gui.general_arc_rpm, LV_OBJ_FLAG_CLICKABLE); */
}

void general_exit(void)
{
    s_active = false;
}

void general_update(void)
{
    if (!s_active) return;

    const obd_data_t *d = obd_get_data();

    /* 更新 Arc / Label（变化检测 + 整屏 invalidate） */
    /* 你的数据绑定逻辑写在这里 */
    (void)d;
}
