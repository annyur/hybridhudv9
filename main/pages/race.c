/* race.c — Race 界面业务 + OBD 数据绑定 */
#include "race.h"
#include "obd.h"
#include "imu.h"
#include "ble.h"
#include "rtc.h"
#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gui_guider.h"

extern lv_ui guider_ui;

static bool s_active = false;
static bool s_boot_done = false;

/* G-force */
#define G_SCALE   60.0f
#define G_MAX     2.0f
#define CENTER_X  233.0f
#define CENTER_Y  233.0f
#define MAP_SCREEN_X(ay)   (-(ay))    /* 左右：取反 */
#define MAP_SCREEN_Y(az)   (az)       /* 上下：取反 */

/* 颜色 */
#define COLOR_GREEN  lv_color_hex(0x00ff24)
#define COLOR_ORANGE lv_color_hex(0xff6500)
#define COLOR_RED    lv_color_hex(0xff0000)

static lv_obj_t ** const s_arcs[4] = {
    &guider_ui.race_arc_4, &guider_ui.race_arc_5,
    &guider_ui.race_arc_6, &guider_ui.race_arc_7,
};

static int   s_last_speed = -1;
static float s_last_kw = -999.0f;
static int   s_last_time_mm = -1;
static float s_last_color_kw = -999.0f;
static int   s_last_coolant = -999;

/* 动画状态 */
static uint32_t s_boot_start = 0;
static int      s_breath_tick = 0;

static void set_arcs_color(lv_color_t c)
{
    for (int i = 0; i < 4; i++)
        lv_obj_set_style_arc_color(*s_arcs[i], c, LV_PART_INDICATOR);
}

static void set_arcs_value(int v)
{
    for (int i = 0; i < 4; i++)
        lv_arc_set_value(*s_arcs[i], v);
}

static void update_arc_colors(float kw)
{
    if (fabsf(kw - s_last_color_kw) < 1.0f) return;
    s_last_color_kw = kw;
    lv_color_t c = (kw < 0) ? COLOR_GREEN : (kw > 50) ? COLOR_RED : COLOR_ORANGE;
    set_arcs_color(c);
}

/* ========== G 点呼吸动画（手动驱动，24步/480ms） ========== */
static void gpoint_breath_step(int tick)
{
    /* tick: 0~24，周期 480ms @ 20ms */
    int phase = tick % 24;
    int opa;
    if (phase < 8) {
        /* 0~8 (0~160ms): 淡入 0→255 */
        opa = (phase * 255) / 8;
    } else if (phase < 16) {
        /* 8~16 (160~320ms): 淡出 255→0 */
        opa = 255 - ((phase - 8) * 255) / 8;
    } else {
        /* 16~24 (320~480ms): 透明等待 */
        opa = 0;
    }
    lv_obj_set_style_opa(guider_ui.race_label_G_piont, opa, LV_PART_MAIN | LV_STATE_DEFAULT);
}

/* 开机动画标记 */
void race_boot_animation(void)
{
    if (s_boot_done) return;
    s_boot_done = true;
}

void race_enter(void)
{
    s_active = true;
    s_boot_done = true;   /* 跳过开机动画，直接允许 arc 和 G-force 更新 */
    /* 重置所有动画状态，从零开始 */
    s_boot_start = 0;
    s_breath_tick = 0;
    hud_imu_calibrate();
}

void race_exit(void)
{
    s_active = false;
    /* 切走时重置所有动画状态，切回来时从零开始 */
    /* s_boot_start 和 s_breath_tick 由 race_enter 重置 */
}

void race_update(void)
{
    if (!s_active) return;

    const obd_data_t *d = obd_get_data();
    char buf[32];

    /* ===================== 开机动画（只播放一次，G 点呼吸同时进行） ===================== */
    if (s_boot_done && s_boot_start == 0) {
        s_boot_start = lv_tick_get();
    }
    if (s_boot_start > 0 && s_boot_start != 0xFFFFFFFF) {
        uint32_t elapsed = lv_tick_get() - s_boot_start;
        /* G 点呼吸（始终进行） */
        s_breath_tick++;
        gpoint_breath_step(s_breath_tick);

        if (elapsed < 500) {
            /* 0~500ms: 只呼吸 */
        } else if (elapsed < 1500) {
            /* 500~1500ms: arc sweep up */
            int v = ((elapsed - 500) * 200) / 1000;
            if (v > 200) v = 200;
            set_arcs_value(v);
        } else if (elapsed < 2500) {
            /* 1500~2500ms: arc sweep down */
            int v = 200 - ((elapsed - 1500) * 200) / 1000;
            if (v < 0) v = 0;
            set_arcs_value(v);
        } else {
            /* 动画结束 */
            set_arcs_value(0);
            s_boot_start = 0xFFFFFFFF;
        }
        /* boot 期间也更新 label */
        goto update_label;
    }

    /* ===================== 蓝牙未连接：arc=0，G点呼吸 ===================== */
    if (!ble_is_connected()) {
        s_breath_tick++;
        gpoint_breath_step(s_breath_tick);

update_label:
        /* label 仍然更新 */
        if (d) {
            if (d->speed != s_last_speed) {
                s_last_speed = d->speed;
                snprintf(buf, sizeof(buf), "%02d", d->speed);
                lv_label_set_text(guider_ui.race_label_speed_number, buf);
            }
            if (fabsf(d->power_kw - s_last_kw) > 0.1f) {
                s_last_kw = d->power_kw;
                snprintf(buf, sizeof(buf), "%.1f", d->power_kw);
                lv_label_set_text(guider_ui.race_label_energy_number, buf);
            }
            update_arc_colors(d->power_kw);
        }
        /* 跳过 G-force IMU 和 arc 更新 */
        goto update_time;
    }

    /* ===================== 蓝牙已连接：恢复正常 ===================== */
    /* 恢复 G 点不透明 */
    lv_obj_set_style_opa(guider_ui.race_label_G_piont, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* ---------- G-force（IMU） ---------- */
    {
        float ax, ay, az;
        if (hud_imu_get_accel(&ax, &ay, &az)) {
            float g_x = MAP_SCREEN_X(ay) / 9.8f;
            float g_y = MAP_SCREEN_Y(az) / 9.8f;
            if (g_x > G_MAX)  g_x = G_MAX;
            if (g_x < -G_MAX) g_x = -G_MAX;
            if (g_y > G_MAX)  g_y = G_MAX;
            if (g_y < -G_MAX) g_y = -G_MAX;

            float px = CENTER_X + g_x * G_SCALE - 10.0f;
            float py = CENTER_Y + g_y * G_SCALE - 10.0f;
            if (px < 83.0f) px = 83.0f;
            if (px > 383.0f) px = 383.0f;
            if (py < 83.0f) py = 83.0f;
            if (py > 383.0f) py = 383.0f;

            lv_obj_set_pos(guider_ui.race_label_G_piont, (lv_coord_t)px, (lv_coord_t)py);

            snprintf(buf, sizeof(buf), "%.2f", g_y < 0 ? -g_y : 0.0f);
            lv_label_set_text(guider_ui.race_label_top, buf);
            snprintf(buf, sizeof(buf), "%.2f", g_y > 0 ? g_y : 0.0f);
            lv_label_set_text(guider_ui.race_label_down, buf);
            snprintf(buf, sizeof(buf), "%.2f", g_x < 0 ? -g_x : 0.0f);
            lv_label_set_text(guider_ui.race_label_left, buf);
            snprintf(buf, sizeof(buf), "%.2f", g_x > 0 ? g_x : 0.0f);
            lv_label_set_text(guider_ui.race_label_right, buf);
        }
    }

    /* ---------- OBD 数据 ---------- */
    if (!d) goto update_time;

    update_arc_colors(d->power_kw);

    if (d->speed != s_last_speed) {
        s_last_speed = d->speed;
        set_arcs_value(d->speed);
        snprintf(buf, sizeof(buf), "%02d", d->speed);
        lv_label_set_text(guider_ui.race_label_speed_number, buf);
    }
    if (fabsf(d->power_kw - s_last_kw) > 0.1f) {
        s_last_kw = d->power_kw;
        snprintf(buf, sizeof(buf), "%.1f", d->power_kw);
        lv_label_set_text(guider_ui.race_label_energy_number, buf);
    }

    /* ========== 冷却液温度 → label_1 ========== */
    if (d->coolant != s_last_coolant) {
        s_last_coolant = d->coolant;
        snprintf(buf, sizeof(buf), "%d°C", d->coolant);
        lv_label_set_text(guider_ui.race_label_1, buf);
    }

update_time:
    /* ---------- RTC 时间 ---------- */
    {
        struct tm ti;
        hud_rtc_get_time(&ti);
        int now_mm = ti.tm_hour * 60 + ti.tm_min;
        if (now_mm != s_last_time_mm) {
            s_last_time_mm = now_mm;
            snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
            lv_label_set_text(guider_ui.race_label_time, buf);
        }
    }

    static bool s_labels_inited = false;
    if (!s_labels_inited) {
        s_labels_inited = true;
        lv_label_set_text(guider_ui.race_label_kw, "kw");
    }
}