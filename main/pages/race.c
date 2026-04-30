/* race.c — Race 界面业务 */
#include "race.h"
#include "imu.h"
#include "ble.h"
#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include "gui_guider.h"

extern lv_ui guider_ui;

static bool s_active = false;

/* G-force 显示参数 */
#define G_SCALE   60.0f   /* 1g 对应的像素偏移 */
#define G_MAX     2.0f    /* 最大显示 G 值 */
#define CENTER_X  233.0f  /* race_img_1 中心 X (73 + 160) */
#define CENTER_Y  233.0f  /* race_img_1 中心 Y (73 + 160) */

void race_enter(void)
{
    s_active = true;
    hud_imu_calibrate();
}

void race_exit(void)
{
    s_active = false;
}

void race_update(void)
{
    if (!s_active) return;

    float ax, ay, az;
    if (!hud_imu_get_accel(&ax, &ay, &az)) return;

    /* 计算净 G 值（假设校准后水平放置时 az ≈ 1g，此处简化处理） */
    /* 若 IMU 安装方向不同，请调整 ax/ay/az 的符号和映射 */
    float g_lat  = ax;          /* 横向 G（左右）*/
    float g_long = ay;          /* 纵向 G（前后）*/
    float g_total = sqrtf(ax * ax + ay * ay + az * az);

    /* 限制最大 G 值 */
    if (g_lat > G_MAX)  g_lat = G_MAX;
    if (g_lat < -G_MAX) g_lat = -G_MAX;
    if (g_long > G_MAX) g_long = G_MAX;
    if (g_long < -G_MAX) g_long = -G_MAX;

    /* ========== 更新 G 点位置 ========== */
    /* race_label_G_piont 尺寸 20x20，圆角 20 */
    /* 中心位置 = CENTER + g * G_SCALE */
    /* 注意：屏幕 Y 向下为正，若向前加速 G 点应向下（后方）移动，请根据实际安装调整符号 */
    float px = CENTER_X + g_lat * G_SCALE - 10.0f;   /* -10 是半宽补偿 */
    float py = CENTER_Y + g_long * G_SCALE - 10.0f;  /* 纵向：ay 正 = 向前加速 = G 点后移 */

    /* 限制在背景图范围内（留 10px 边距） */
    if (px < 83.0f)  px = 83.0f;   /* 73 + 10 */
    if (px > 383.0f) px = 383.0f;  /* 73 + 320 - 10 */
    if (py < 83.0f)  py = 83.0f;
    if (py > 383.0f) py = 383.0f;

    lv_obj_set_pos(guider_ui.race_label_G_piont, (lv_coord_t)px, (lv_coord_t)py);

    /* ========== 更新四周 Label ========== */
    char buf[16];

    /* Top — 正向纵向（加速） */
    snprintf(buf, sizeof(buf), "%.2f", g_long > 0 ? g_long : 0.0f);
    lv_label_set_text(guider_ui.race_label_top, buf);

    /* Down — 负向纵向（刹车） */
    snprintf(buf, sizeof(buf), "%.2f", g_long < 0 ? -g_long : 0.0f);
    lv_label_set_text(guider_ui.race_label_down, buf);

    /* Left — 负向横向（左转） */
    snprintf(buf, sizeof(buf), "%.2f", g_lat < 0 ? -g_lat : 0.0f);
    lv_label_set_text(guider_ui.race_label_left, buf);

    /* Right — 正向横向（右转） */
    snprintf(buf, sizeof(buf), "%.2f", g_lat > 0 ? g_lat : 0.0f);
    lv_label_set_text(guider_ui.race_label_right, buf);
}