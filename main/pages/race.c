/* race.c — Race 界面业务 */
#include "race.h"
#include "imu.h"
#include "ble.h"
#include <lvgl.h>
#include <math.h>
#include "gui_guider.h"

extern lv_ui guider_ui;

static bool s_active = false;

void race_enter(void)
{
    s_active = true;
    hud_imu_calibrate();   /* <-- 改名 */
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