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
#define G_MAX      1.0f    /* 最大显示加速度 1.0g */
#define G_CENTER_X 233.0f  /* 圆心 X */
#define G_CENTER_Y 233.0f  /* 圆心 Y */
#define G_INNER_R  80.0f   /* 内圆半径：0~0.3g 放大区 */
#define G_OUTER_R  160.0f  /* 外圆半径：总显示范围 */
#define G_THRESH   0.3f    /* 放大阈值 */
#define MAP_SCREEN_X(ay)   (-(ay))    /* 左右：取反 */
#define MAP_SCREEN_Y(az)   (az)       /* 上下：取反 */

/* 动态死区：基础 0.08g，根据变化率自动缩放 */
#define G_DEAD_BASE  0.08f   /* 静止/微震时死区 */
#define G_DEAD_MIN   0.02f   /* 大幅转弯时死区 */

/* EMA 低通滤波系数：0.3 = 每帧向目标移动 30%，形成惯性拖尾 */
#define G_EMA_ALPHA  0.30f

static float s_gx_ema = 0.0f;   /* 滤波后的 Gx */
static float s_gy_ema = 0.0f;   /* 滤波后的 Gy */
static float s_gx_prev = 0.0f; /* 上一帧原始值（用于计算 jerk） */
static float s_gy_prev = 0.0f;

/* 动态死区补偿：根据变化率自动调整
   - 变化率小（<0.01g/帧）→ 死区扩大到 0.08g，过滤路面微震
   - 变化率大（>0.05g/帧）→ 死区缩小到 0.02g，让真实转弯响应更快 */
static float shake_comp(float g, float jerk)
{
    float dead;
    if (jerk > 0.05f)       dead = G_DEAD_MIN;        /* 急转 */
    else if (jerk > 0.02f)  dead = G_DEAD_BASE * 0.5f; /* 中等 */
    else                    dead = G_DEAD_BASE;         /* 静止/微震 */

    if (g > dead)  return g - dead;
    if (g < -dead) return g + dead;
    return 0.0f;
}

/* EMA 低通滤波：让 G 点有惯性，微小抖动被平滑 */
static float ema_filter(float ema, float raw)
{
    return ema * (1.0f - G_EMA_ALPHA) + raw * G_EMA_ALPHA;
}
static float g_to_pixel(float g)
{
    float abs_g = g < 0 ? -g : g;
    float sign  = g < 0 ? -1.0f : 1.0f;
    float px;
    if (abs_g <= G_THRESH) {
        /* 0~0.3g 放大映射到 0~80 像素 */
        px = (abs_g / G_THRESH) * G_INNER_R;
    } else if (abs_g <= G_MAX) {
        /* 0.3g~1.0g 映射到 80~160 像素 */
        px = G_INNER_R + ((abs_g - G_THRESH) / (G_MAX - G_THRESH)) * (G_OUTER_R - G_INNER_R);
    } else {
        px = G_OUTER_R;  /* 封顶 */
    }
    return px * sign;
}

/* 颜色 */
#define COLOR_GREEN  lv_color_hex(0x00ff24)
#define COLOR_ORANGE lv_color_hex(0xff6500)
#define COLOR_RED    lv_color_hex(0xff0000)

static lv_obj_t ** const s_arcs[4] = {
    &guider_ui.race_arc_4, &guider_ui.race_arc_5,
    &guider_ui.race_arc_6, &guider_ui.race_arc_7,
};

static int   s_last_speed = -1;
static int   s_display_speed = -1;
static float s_speed_frac = 0.0f;  /* 浮点累积器：0→100 极限 7 秒 */
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
    s_boot_done = true;
    s_display_speed = -1;
    s_speed_frac = 0.0f;
    s_gx_ema = s_gy_ema = 0.0f;
    s_gx_prev = s_gy_prev = 0.0f;

    /* 删除 GUI Guider 可能启动的 arc 默认动画，防止覆盖 OBD 实时数据 */
    lv_anim_del(guider_ui.race_arc_4, NULL);
    lv_anim_del(guider_ui.race_arc_5, NULL);
    lv_anim_del(guider_ui.race_arc_6, NULL);
    lv_anim_del(guider_ui.race_arc_7, NULL);

    /* 立即恢复 G 点不透明，防止呼吸动画残留 */
    lv_obj_set_style_opa(guider_ui.race_label_G_piont, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 强制结束 boot 动画，确保切回来不再播放 sweep */
    s_boot_start = 0xFFFFFFFF;

    /* 立即同步到当前 OBD 数据，避免 arc 停留在动画残留值 */
    const obd_data_t *d = obd_get_data();
    if (d) {
        set_arcs_value(d->speed);
        s_last_speed = d->speed;
        s_display_speed = d->speed;
        s_speed_frac = 0.0f;
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d", d->speed);
        lv_label_set_text(guider_ui.race_label_speed_number, buf);
    }

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
            float raw_x = MAP_SCREEN_X(ay) / 9.8f;
            float raw_y = MAP_SCREEN_Y(az) / 9.8f;

            /* 计算变化率（jerk） */
            float jerk_x = raw_x - s_gx_prev;
            float jerk_y = raw_y - s_gy_prev;
            float jerk = (jerk_x < 0 ? -jerk_x : jerk_x) + (jerk_y < 0 ? -jerk_y : jerk_y);
            s_gx_prev = raw_x;
            s_gy_prev = raw_y;

            /* 动态死区 + EMA 低通滤波 */
            float filt_x = shake_comp(raw_x, jerk);
            float filt_y = shake_comp(raw_y, jerk);
            s_gx_ema = ema_filter(s_gx_ema, filt_x);
            s_gy_ema = ema_filter(s_gy_ema, filt_y);

            /* 分段映射：内圆(0~0.3g)放大，外环(0.3~1.0g)正常 */
            float off_x = g_to_pixel(s_gx_ema);
            float off_y = g_to_pixel(s_gy_ema);

            float px = G_CENTER_X + off_x - 10.0f;
            float py = G_CENTER_Y + off_y - 10.0f;
            /* 硬边界：G点中心不出外圆 */
            if (px < G_CENTER_X - G_OUTER_R) px = G_CENTER_X - G_OUTER_R;
            if (px > G_CENTER_X + G_OUTER_R - 20.0f) px = G_CENTER_X + G_OUTER_R - 20.0f;
            if (py < G_CENTER_Y - G_OUTER_R) py = G_CENTER_Y - G_OUTER_R;
            if (py > G_CENTER_Y + G_OUTER_R - 20.0f) py = G_CENTER_Y + G_OUTER_R - 20.0f;

            lv_obj_set_pos(guider_ui.race_label_G_piont, (lv_coord_t)px, (lv_coord_t)py);

            snprintf(buf, sizeof(buf), "%.2f", s_gy_ema < 0 ? -s_gy_ema : 0.0f);
            lv_label_set_text(guider_ui.race_label_top, buf);
            snprintf(buf, sizeof(buf), "%.2f", s_gy_ema > 0 ? s_gy_ema : 0.0f);
            lv_label_set_text(guider_ui.race_label_down, buf);
            snprintf(buf, sizeof(buf), "%.2f", s_gx_ema < 0 ? -s_gx_ema : 0.0f);
            lv_label_set_text(guider_ui.race_label_left, buf);
            snprintf(buf, sizeof(buf), "%.2f", s_gx_ema > 0 ? s_gx_ema : 0.0f);
            lv_label_set_text(guider_ui.race_label_right, buf);
        }
    }

    /* ---------- OBD 数据 ---------- */
    if (!d) goto update_time;

    update_arc_colors(d->power_kw);

    if (d->speed != s_last_speed) {
        s_last_speed = d->speed;  /* 目标值已变 */
    }
    if (s_display_speed < 0) {
        s_display_speed = s_last_speed;  /* 首次直接跳到目标值 */
        s_speed_frac = 0.0f;
    }
    if (s_display_speed != s_last_speed) {
        int diff = s_last_speed - s_display_speed;
        /* 每帧固定增加 0.356：100÷(8.5s×33帧) = 100÷280.5 ≈ 0.356 */
        float rate = 100.0f / 280.5f;
        if (diff < 0) rate = -rate;
        s_speed_frac += rate;
        if (s_speed_frac >= 1.0f || s_speed_frac <= -1.0f) {
            int step = (int)s_speed_frac;
            s_display_speed += step;
            s_speed_frac -= step;
        }
        /* 防止过冲 */
        if ((diff > 0 && s_display_speed > s_last_speed) ||
            (diff < 0 && s_display_speed < s_last_speed)) {
            s_display_speed = s_last_speed;
            s_speed_frac = 0.0f;
        }
        set_arcs_value(s_display_speed);
        snprintf(buf, sizeof(buf), "%02d", s_display_speed);
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
        snprintf(buf, sizeof(buf), "%d", d->coolant);
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