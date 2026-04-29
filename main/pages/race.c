/* race.c — Race 界面业务（G-force + 蓝牙闪烁） */
#include "race.h"
#include "imu.h"
#include "ble.h"
#include <lvgl.h>
#include <math.h>

extern gui_obj_t gui;

static bool s_active = false;

void race_enter(void)
{
    s_active = true;
    imu_calibrate();
}

void race_exit(void)
{
    s_active = false;
}

void race_update(void)
{
    if (!s_active) return;

    /* G-force 计算与 label_G_point 位置更新 */
    /* 你的 G-force 逻辑写在这里 */
}
