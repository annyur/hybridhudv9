/* ============================================================
 * main/pages/race.c  --  Race 页面（精简版）
 * 显示: 功率(kW)、冷却液温度、电池温度、G-force
 * arc5/arc6: 双向扩散功率显示
 * arc7: 蓝牙未连接时绿色整圈呼吸
 * ============================================================ */

#include "../generated/gui_guider.h"
#include "../generated/events_init.h"
#include "lvgl.h"
#include "../obd/obd.h"
#include "../obd/ecu.h"
#include "../ble/ble.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"

extern lv_ui guider_ui;
extern void hud_imu_calibrate(void);
extern bool hud_imu_get_accel(float *ax, float *ay, float *az);

/* ---------- G-force 配置 ---------- */
#define G_MAX       1.0f
#define G_CENTER_X  233.0f
#define G_CENTER_Y  233.0f
#define G_INNER_R   80.0f
#define G_OUTER_R   160.0f
#define G_THRESH    0.3f
#define G_DEAD_BASE  0.08f
#define G_DEAD_MIN   0.02f
#define G_EMA_ALPHA  0.30f

static float s_gx_ema = 0.0f, s_gy_ema = 0.0f;
static float s_gx_prev = 0.0f, s_gy_prev = 0.0f;

/* 功率动画 */
static float s_display_kw = -999.0f;
static float s_last_display_kw = -999.0f;

/* 缓存 */
static int   s_last_coolant = -999;
static int   s_last_batt_temp = -999;

/* arc5/6 颜色缓存 — style 修改开销大，只在功率变化时更新 */
static float s_last_power_kw = -999.0f;

/* arc7 呼吸 */
static int   s_breath_tick = 0;
static int   s_last_arc7_opa = -1;

/* BLE 状态缓存 — 每 100ms 查一次 */
static uint32_t s_last_ble_check_ms = 0;
static bool     s_ble_conn_cached = false;

/* arc7 闪烁反馈: G-force 归零时快速闪2下 */
static int s_arc7_flash_cnt = 0;    /* 剩余闪烁次数 */
static int s_arc7_flash_phase = 0;  /* 0=亮 1=暗 */
static int s_arc7_flash_tick = 0;   /* 帧计数(基于 20ms) */

/* ---------- G-force 辅助 ---------- */
static float shake_comp(float g, float jerk)
{
    float dead;
    if (jerk > 0.05f)       dead = G_DEAD_MIN;
    else if (jerk > 0.02f)  dead = G_DEAD_BASE * 0.5f;
    else                    dead = G_DEAD_BASE;
    if (g > dead)  return g - dead;
    if (g < -dead) return g + dead;
    return 0.0f;
}

static float ema_filter(float ema, float raw)
{
    return ema * (1.0f - G_EMA_ALPHA) + raw * G_EMA_ALPHA;
}

static float g_to_pixel(float g)
{
    float abs_g = g < 0 ? -g : g;
    float sign  = g < 0 ? -1.0f : 1.0f;
    float px;
    if (abs_g <= G_THRESH)       px = (abs_g / G_THRESH) * G_INNER_R;
    else if (abs_g <= G_MAX)     px = G_INNER_R + ((abs_g - G_THRESH) / (G_MAX - G_THRESH)) * (G_OUTER_R - G_INNER_R);
    else                         px = G_OUTER_R;
    return px * sign;
}

/* ---------- 功率颜色 ----------
 * 回收(负)=绿色, 放电0-30=橙色, 放电>30=橙→红渐变 */
static lv_color_t power_color(float power_kw)
{
    if (power_kw < 0) return lv_color_hex(0x00ff24);  /* 回收=绿色 */
    int v = (int)power_kw;
    if (v > 100) v = 100;
    if (v <= 30) return lv_color_hex(0xff6500);         /* 0~30=橙色 */
    /* 30~100: 橙→红渐变 */
    int t = ((v - 30) * 255) / 70;  /* 0~255 */
    uint8_t g = 0x65 - (uint8_t)((0x65 * t) / 255);
    return lv_color_make(0xFF, g, 0);
}

/* ---------- arc5/arc6 双向扩散 ----------
 * 以50为中心，功率越大向两边扩展越多
 * 角度每帧都更新(丝滑)，颜色只在功率变化时更新(减少style开销) */
static void set_power_arcs(float power_kw)
{
    float abs_kw = power_kw >= 0 ? power_kw : -power_kw;
    if (abs_kw > 100.0f) abs_kw = 100.0f;

    int v = (int)(abs_kw + 0.5f);
    int spread = (v * 50) / 100;

    /* 角度每帧都更新 — 让扩散动画丝滑 */
    int start5 = (spread <= 0) ? 0 : (360 - spread);
    lv_arc_set_start_angle(guider_ui.race_arc_5, start5);
    lv_arc_set_end_angle(guider_ui.race_arc_5, spread);

    lv_arc_set_start_angle(guider_ui.race_arc_6, 180 - spread);
    lv_arc_set_end_angle(guider_ui.race_arc_6, 180 + spread);

    /* 颜色只在功率变化超过 0.3kW 或 回收/放电切换时更新 */
    bool color_changed = (fabsf(power_kw - s_last_power_kw) > 0.3f) ||
                         ((power_kw < 0) != (s_last_power_kw < 0)) ||
                         (s_last_power_kw < -500.0f);
    if (color_changed) {
        s_last_power_kw = power_kw;
        lv_color_t c = power_color(power_kw);
        lv_obj_set_style_arc_color(guider_ui.race_arc_5, c, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(guider_ui.race_arc_6, c, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }

    /* 确保指示器可见 */
    lv_obj_set_style_arc_opa(guider_ui.race_arc_5, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(guider_ui.race_arc_6, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}

/* ---------- arc7 绿色呼吸 ----------
 * 注意: 初始化时 value=0，indicator 弧长为 0，必须设为 100 才能显示整圈 */
static void arc7_breath_step(int tick)
{
    int phase = tick % 24;
    int opa;
    if (phase < 8)       opa = (phase * 255) / 8;
    else if (phase < 16) opa = 255 - ((phase - 8) * 255) / 8;
    else                 opa = 0;

    /* opacity 变化时才更新 */
    if (opa == s_last_arc7_opa) return;
    s_last_arc7_opa = opa;

    /* 确保 indicator 弧长为满圈（整圈呼吸可见） */
    lv_obj_set_style_arc_color(guider_ui.race_arc_7, lv_color_hex(0x00ff24),
                               LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(guider_ui.race_arc_7, opa, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}

/* ============================================================ */
void race_update(void)
{
    /* 每 20ms 执行一次(50Hz)，与 main.c 循环同步 — 丝滑流畅 */
    const obd_data_t *d = obd_get_data();
    if (!d) return;
    char buf[32];

    /* ---------- G-force ---------- */
    {
        float ax, ay, az;
        if (hud_imu_get_accel(&ax, &ay, &az)) {
            float raw_x = (-ay) / 9.80665f;
            float raw_y = (az)  / 9.80665f;
            float jerk_x = raw_x - s_gx_prev;
            float jerk_y = raw_y - s_gy_prev;
            float jerk = (jerk_x < 0 ? -jerk_x : jerk_x) + (jerk_y < 0 ? -jerk_y : jerk_y);
            s_gx_prev = raw_x; s_gy_prev = raw_y;
            s_gx_ema = ema_filter(s_gx_ema, shake_comp(raw_x, jerk));
            s_gy_ema = ema_filter(s_gy_ema, shake_comp(raw_y, jerk));
            float off_x = g_to_pixel(s_gx_ema);
            float off_y = g_to_pixel(s_gy_ema);
            float px = G_CENTER_X + off_x - 10.0f;
            float py = G_CENTER_Y + off_y - 10.0f;
            if (px < G_CENTER_X - G_OUTER_R) px = G_CENTER_X - G_OUTER_R;
            if (px > G_CENTER_X + G_OUTER_R - 20.0f) px = G_CENTER_X + G_OUTER_R - 20.0f;
            if (py < G_CENTER_Y - G_OUTER_R) py = G_CENTER_Y - G_OUTER_R;
            if (py > G_CENTER_Y + G_OUTER_R - 20.0f) py = G_CENTER_Y + G_OUTER_R - 20.0f;
            lv_obj_set_pos(guider_ui.race_label_G_piont, (lv_coord_t)px, (lv_coord_t)py);
            snprintf(buf, sizeof(buf), "%.2f", s_gy_ema < 0 ? -s_gy_ema : 0.0f);
            lv_label_set_text(guider_ui.race_label_top,  buf);
            snprintf(buf, sizeof(buf), "%.2f", s_gy_ema > 0 ? s_gy_ema : 0.0f);
            lv_label_set_text(guider_ui.race_label_down, buf);
            snprintf(buf, sizeof(buf), "%.2f", s_gx_ema < 0 ? -s_gx_ema : 0.0f);
            lv_label_set_text(guider_ui.race_label_left, buf);
            snprintf(buf, sizeof(buf), "%.2f", s_gx_ema > 0 ? s_gx_ema : 0.0f);
            lv_label_set_text(guider_ui.race_label_right, buf);
        }
    }

    /* ---------- 功率 -- 线性步进(更快响应) ---------- */
    {
        float target = d->power_kw;
        if (s_display_kw < -500.0f) s_display_kw = target;
        float diff = target - s_display_kw;
        if (fabsf(diff) > 0.02f) {
            float step = fabsf(diff) * 0.3f;   /* 0.3 = 更快跟上实际功率 */
            if (step < 0.3f) step = 0.3f;     /* 最小步长 0.3kW/帧 */
            s_display_kw += (diff > 0 ? step : -step);
            if (fabsf(target - s_display_kw) < step) s_display_kw = target;
        }
        if (fabsf(s_display_kw - s_last_display_kw) > 0.02f) {
            s_last_display_kw = s_display_kw;
            if (s_display_kw > 0.1f)       snprintf(buf, sizeof(buf), "%.1f", s_display_kw);
            else if (s_display_kw < -0.1f) snprintf(buf, sizeof(buf), "+%.1f", -s_display_kw);
            else                           snprintf(buf, sizeof(buf), "0.0");
            lv_label_set_text(guider_ui.race_label_energy_number, buf);
        }
    }

    /* 冷却液温度 */
    if (d->coolant != s_last_coolant) {
        s_last_coolant = d->coolant;
        snprintf(buf, sizeof(buf), "%d", d->coolant);
        lv_label_set_text(guider_ui.race_label_1, buf);
    }

    /* 电池温度平均 */
    {
        int bt = (int)((d->bms_max_temp + d->bms_min_temp) / 2.0f);
        if (bt != s_last_batt_temp) {
            s_last_batt_temp = bt;
            snprintf(buf, sizeof(buf), "%d", bt);
            lv_label_set_text(guider_ui.race_label_2, buf);
        }
    }

    /* ---------- arc5/arc6: 双向扩散功率显示（与蓝牙状态无关） ---------- */
    set_power_arcs(d->power_kw);

    /* ---------- arc7: 闪烁反馈 或 蓝牙状态指示 ---------- */
    if (s_arc7_flash_cnt > 0) {
        /* G-force 归零反馈: 快速闪烁2下(120ms周期) */
        s_arc7_flash_tick++;
        if (s_arc7_flash_tick >= 6) {  /* 6×20ms = 120ms */
            s_arc7_flash_tick = 0;
            s_arc7_flash_phase = !s_arc7_flash_phase;
            s_arc7_flash_cnt--;
        }
        lv_arc_set_value(guider_ui.race_arc_7, 100);
        lv_obj_set_style_arc_color(guider_ui.race_arc_7, lv_color_hex(0xFFFFFF),
                                   LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_opa(guider_ui.race_arc_7,
                                 s_arc7_flash_phase ? 0 : 255,
                                 LV_PART_INDICATOR | LV_STATE_DEFAULT);
    } else {
        /* ble 状态每 100ms 查一次 */
        uint32_t now = lv_tick_get();
        if ((now - s_last_ble_check_ms) >= 100) {
            s_ble_conn_cached = ble_is_connected();
            s_last_ble_check_ms = now;
        }
        if (!s_ble_conn_cached) {
            /* 蓝牙断开: arc7 绿色呼吸 */
            s_breath_tick++;
            arc7_breath_step(s_breath_tick);
        } else {
            /* 蓝牙连接: arc7 隐藏 */
            lv_obj_set_style_arc_opa(guider_ui.race_arc_7, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
    }
}

/* ============================================================ */
void race_enter(void)
{
    s_gx_ema = s_gy_ema = 0.0f;
    s_gx_prev = s_gy_prev = 0.0f;
    s_display_kw = -999.0f;
    s_last_display_kw = -999.0f;
    s_last_power_kw = -999.0f;
    s_last_arc7_opa = -1;
    s_last_ble_check_ms = 0;
    s_arc7_flash_cnt = 0;  /* 切回race时清除闪烁 */
    s_arc7_flash_tick = 0;
    s_last_coolant = -999;
    s_last_batt_temp = -999;

    /* 禁用 arc 触控，避免误触和性能消耗 */
    lv_obj_remove_flag(guider_ui.race_arc_5, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(guider_ui.race_arc_6, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(guider_ui.race_arc_7, LV_OBJ_FLAG_CLICKABLE);

    const obd_data_t *d = obd_get_data();
    if (d) {
        char t[16];
        if (d->power_kw > 0.1f)       snprintf(t, sizeof(t), "%.1f", d->power_kw);
        else if (d->power_kw < -0.1f) snprintf(t, sizeof(t), "+%.1f", -d->power_kw);
        else                          snprintf(t, sizeof(t), "0.0");
        lv_label_set_text(guider_ui.race_label_energy_number, t);
        snprintf(t, sizeof(t), "%d", d->coolant);
        lv_label_set_text(guider_ui.race_label_1, t);
        int bt = (int)((d->bms_max_temp + d->bms_min_temp) / 2.0f);
        snprintf(t, sizeof(t), "%d", bt);
        lv_label_set_text(guider_ui.race_label_2, t);
    }
    hud_imu_calibrate();
}

void race_exit(void) { }
void race_boot_animation(void) { }
void race_G_zero(void)
{
    ESP_LOGI("RACE", "G_zero called");
    hud_imu_calibrate();
    s_gx_ema = s_gy_ema = 0.0f;
    s_gx_prev = s_gy_prev = 0.0f;

    /* 启动 arc7 快速闪烁2下(120ms亮→120ms暗→120ms亮→120ms暗) */
    s_arc7_flash_cnt = 4;
    s_arc7_flash_phase = 0;
    s_arc7_flash_tick = 0;
}