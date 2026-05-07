/* ============================================================
 * main/pages/race.c  --  Race 页面（精简版）
 * 显示: 功率(kW)、冷却液温度、电池温度、G-force
 * arc5/arc6: 双向扩散功率显示
 * arc7: 蓝牙未连接时绿色整圈呼吸
 *
 * 优化: 
 *   1. 脏区域/条件刷新，避免全屏重绘
 *   2. 非阻塞数据同步，通过消息队列接收 OBD 数据
 * ============================================================ */

#include "../generated/gui_guider.h"
#include "../obd/obd.h"
#include "../ble/ble.h"
#include <math.h>
#include <stdio.h>
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

/* G-Force EMA 滤波器封装（可重入）*/
typedef struct {
    float ema_x;
    float ema_y;
    float prev_x;
    float prev_y;
} gforce_filter_t;

static void gforce_filter_reset(gforce_filter_t *f)
{
    f->ema_x = 0.0f;
    f->ema_y = 0.0f;
    f->prev_x = 0.0f;
    f->prev_y = 0.0f;
}

/* G-Force 滤波器实例（替代原来的静态变量）*/
static gforce_filter_t s_gforce = {0};

/* 功率动画 */
static float s_display_kw = 0.0f;
static float s_last_display_kw = 0.0f;

/* OBD 数据本地缓存（通过消息队列更新）*/
static obd_data_t s_obd_cache = {0};
static bool s_obd_cache_valid = false;

/* 缓存 */
static int   s_last_coolant = -999;
static int   s_last_batt_temp = -999;

/* arc5/6 颜色缓存 — style 修改开销大，只在功率变化时更新 */
static float s_last_power_kw = -999.0f;
static int   s_last_arc_spread = -1;
static int   s_display_spread = 0;     /* 当前显示的 arc 扩散值（用于步进动画）*/

/* BLE 状态缓存 — 每 100ms 查一次 */
static uint32_t s_last_ble_check_ms = 0;
static bool     s_ble_conn_cached = false;

/* arc7 闪烁反馈: G-force 归零时快速闪2下 */
static int s_arc7_flash_cnt = 0;    /* 剩余闪烁次数 */
static int s_arc7_flash_phase = 0;  /* 0=亮 1=暗 */
static int s_arc7_flash_tick = 0;

/* arc7 呼吸动画 — 使用 LVGL 内置动画 */
static lv_anim_t s_breath_anim;
static bool s_breath_running = false;

/* 功率数字动画值 */
static float s_animated_kw = 0.0f;  /* 当前动画值 */

static void arc7_breath_start(void);
static void arc7_breath_stop(void);

/* G-force 位置节流 — 只有移动超过阈值才更新 LVGL */
static float s_last_g_px = 0.0f, s_last_g_py = 0.0f;
#define G_POS_THRESH  2.0f

/* G-force 数值标签缓存 - 避免重复设置相同文本 */
static float s_last_g_val_top = 999.0f;
static float s_last_g_val_bottom = 999.0f;
static float s_last_g_val_left = 999.0f;
static float s_last_g_val_right = 999.0f;

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
 * 角度每帧都更新(丝滑)，颜色只在功率变化时更新(减少style开销)
 * 支持步进动画，差值越大变化越快 */
static void set_power_arcs(float target_power_kw)
{
    float target_abs_kw = target_power_kw >= 0 ? target_power_kw : -target_power_kw;
    if (target_abs_kw > 50.0f) target_abs_kw = 50.0f;
    
    /* 目标扩散值 - 50kW填满整圈 */
    int target_spread = (int)(target_abs_kw + 0.5f);
    
    /* 计算显示值到目标值的差值 */
    float kw_diff = target_power_kw - s_display_kw;
    int spread_diff = target_spread - s_display_spread;
    
    /* 动态步进：差值越大变化越快 */
    float kw_step = fabsf(kw_diff) * 0.6f;  /* 60% 跟随因子 */
    int spread_step = abs(spread_diff) * 5 / 10;  /* 50% 跟随因子 */
    
    /* 最小步长限制 - 使用更小的步长实现丝滑过渡 */
    if (kw_step < 0.08f) kw_step = 0.08f;  /* 更小的步长 */
    if (spread_step < 1) spread_step = 1;
    
    /* 更新显示值 - 直接跟随目标，减少延迟 */
    if (fabsf(kw_diff) > 0.02f) {
        s_display_kw += (kw_diff > 0 ? kw_step : -kw_step);
        /* 防止过冲 */
        if ((kw_diff > 0 && s_display_kw > target_power_kw) ||
            (kw_diff < 0 && s_display_kw < target_power_kw)) {
            s_display_kw = target_power_kw;
        }
    }
    
    /* 更新扩散值 */
    if (abs(spread_diff) > 0) {
        s_display_spread += (spread_diff > 0 ? spread_step : -spread_step);
        /* 防止过冲 */
        if ((spread_diff > 0 && s_display_spread > target_spread) ||
            (spread_diff < 0 && s_display_spread < target_spread)) {
            s_display_spread = target_spread;
        }
    }
    
    /* 更新 arc 角度 */
    if (s_display_spread != s_last_arc_spread) {
        s_last_arc_spread = s_display_spread;
        int start5 = (s_display_spread <= 0) ? 0 : (360 - s_display_spread);
        lv_arc_set_start_angle(guider_ui.race_arc_5, start5);
        lv_arc_set_end_angle(guider_ui.race_arc_5, s_display_spread);

        lv_arc_set_start_angle(guider_ui.race_arc_6, 180 - s_display_spread);
        lv_arc_set_end_angle(guider_ui.race_arc_6, 180 + s_display_spread);
    }
    
    /* 颜色只在功率变化超过 0.5kW 或 回收/放电切换时更新 */
    bool color_changed = (fabsf(s_display_kw - s_last_power_kw) > 0.5f) ||
                         ((s_display_kw < 0) != (s_last_power_kw < 0)) ||
                         (s_last_power_kw < -500.0f);
    if (color_changed) {
        s_last_power_kw = s_display_kw;
        lv_color_t c = power_color(s_display_kw);
        lv_obj_set_style_arc_color(guider_ui.race_arc_5, c, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(guider_ui.race_arc_6, c, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
    
    /* 更新功率数字显示（丝滑动画）- 使用更高的跟随速度减少延迟 */
    if (fabsf(s_display_kw - s_animated_kw) > 0.02f) {
        /* 计算动画步长 - 使用更高的跟随速度 */
        float anim_step = (s_display_kw - s_animated_kw) * 0.35f;  /* 35% 插值，加快跟随 */
        if (fabsf(anim_step) < 0.1f) {
            anim_step = anim_step > 0 ? 0.1f : -0.1f;  /* 最小步长 */
        }
        s_animated_kw += anim_step;
        
        /* 防止过冲 */
        float diff = s_display_kw - s_animated_kw;
        if (fabsf(diff) < 0.05f) {
            s_animated_kw = s_display_kw;
        }
        
        /* 更新数字显示 */
        char buf[16];
        if (s_animated_kw > 0.1f)       snprintf(buf, sizeof(buf), "%.1f", s_animated_kw);
        else if (s_animated_kw < -0.1f) snprintf(buf, sizeof(buf), "+%.1f", -s_animated_kw);
        else                           snprintf(buf, sizeof(buf), "0.0");
        lv_label_set_text(guider_ui.race_label_energy_number, buf);
    }
    
    /* 确保指示器可见 */
    lv_obj_set_style_arc_opa(guider_ui.race_arc_5, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(guider_ui.race_arc_6, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}

/* ---------- arc7 绿色呼吸 ----------
 * 注意: 初始化时 value=0，indicator 弧长为 0，必须设为 100 才能显示整圈 */
/* arc7 呼吸动画 - 执行回调 */
static void arc7_breath_exec(void * obj, int32_t value)
{
    lv_obj_set_style_arc_opa(obj, (lv_opa_t)value, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}

/* arc7 呼吸动画 - 启动 */
static void arc7_breath_start(void)
{
    if (s_breath_running) return;

    /* 确保 indicator 弧长为满圈（整圈呼吸可见）- UI 设计范围是 0-1000 */
    lv_arc_set_value(guider_ui.race_arc_7, 1000);
    lv_obj_set_style_arc_color(guider_ui.race_arc_7, lv_color_hex(0x00ff24),
                               LV_PART_INDICATOR | LV_STATE_DEFAULT);

    /* 使用 LVGL 内置动画 - LVGL v9 API */
    lv_anim_init(&s_breath_anim);
    lv_anim_set_var(&s_breath_anim, guider_ui.race_arc_7);
    lv_anim_set_values(&s_breath_anim, 0, 255);
    lv_anim_set_time(&s_breath_anim, 1000);  /* 1秒周期 */
    lv_anim_set_exec_cb(&s_breath_anim, arc7_breath_exec);
    lv_anim_set_repeat_count(&s_breath_anim, LV_ANIM_REPEAT_INFINITE);  /* 无限循环 */
    lv_anim_set_playback_time(&s_breath_anim, 1000);  /* 回退时间相同 */
    lv_anim_start(&s_breath_anim);

    s_breath_running = true;
}

/* arc7 呼吸动画 - 停止 */
static void arc7_breath_stop(void)
{
    if (!s_breath_running) return;

    lv_anim_del(guider_ui.race_arc_7, arc7_breath_exec);
    lv_obj_set_style_arc_opa(guider_ui.race_arc_7, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    s_breath_running = false;
}

/* race_update_non_obd: 处理不需要 OBD 数据的 UI 更新（非阻塞模式下调用）*/
static void race_update_non_obd(void)
{
    char buf[32];

    /* ---------- G-force ---------- */
    {
        float ax, ay, az;
        if (hud_imu_get_accel(&ax, &ay, &az)) {
            float raw_x = (-ay) / 9.80665f;
            float raw_y = (az)  / 9.80665f;
            float jerk_x = raw_x - s_gforce.prev_x;
            float jerk_y = raw_y - s_gforce.prev_y;
            float jerk = (jerk_x < 0 ? -jerk_x : jerk_x) + (jerk_y < 0 ? -jerk_y : jerk_y);
            s_gforce.prev_x = raw_x; s_gforce.prev_y = raw_y;
            s_gforce.ema_x = ema_filter(s_gforce.ema_x, shake_comp(raw_x, jerk));
            s_gforce.ema_y = ema_filter(s_gforce.ema_y, shake_comp(raw_y, jerk));
            float off_x = g_to_pixel(s_gforce.ema_x);
            float off_y = g_to_pixel(s_gforce.ema_y);
            float px = G_CENTER_X + off_x - 10.0f;
            float py = G_CENTER_Y + off_y - 10.0f;
            if (px < G_CENTER_X - G_OUTER_R) px = G_CENTER_X - G_OUTER_R;
            if (px > G_CENTER_X + G_OUTER_R - 20.0f) px = G_CENTER_X + G_OUTER_R - 20.0f;
            if (py < G_CENTER_Y - G_OUTER_R) py = G_CENTER_Y - G_OUTER_R;
            if (py > G_CENTER_Y + G_OUTER_R - 20.0f) py = G_CENTER_Y + G_OUTER_R - 20.0f;

            /* 节流：只有位置变化超过阈值才更新 LVGL */
            if (fabsf(px - s_last_g_px) > G_POS_THRESH || fabsf(py - s_last_g_py) > G_POS_THRESH) {
                lv_obj_set_pos(guider_ui.race_label_G_piont, (lv_coord_t)px, (lv_coord_t)py);
                s_last_g_px = px; s_last_g_py = py;
            }

            /* G-force 数值标签 - 只在值变化超过阈值时更新 */
            float val_top = s_gforce.ema_y < 0 ? -s_gforce.ema_y : 0.0f;
            float val_bottom = s_gforce.ema_y > 0 ? s_gforce.ema_y : 0.0f;
            float val_left = s_gforce.ema_x < 0 ? -s_gforce.ema_x : 0.0f;
            float val_right = s_gforce.ema_x > 0 ? s_gforce.ema_x : 0.0f;

            if (fabsf(val_top - s_last_g_val_top) > 0.01f) {
                snprintf(buf, sizeof(buf), "%.2f", val_top);
                lv_label_set_text(guider_ui.race_label_top, buf);
                s_last_g_val_top = val_top;
            }
            if (fabsf(val_bottom - s_last_g_val_bottom) > 0.01f) {
                snprintf(buf, sizeof(buf), "%.2f", val_bottom);
                lv_label_set_text(guider_ui.race_label_down, buf);
                s_last_g_val_bottom = val_bottom;
            }
            if (fabsf(val_left - s_last_g_val_left) > 0.01f) {
                snprintf(buf, sizeof(buf), "%.2f", val_left);
                lv_label_set_text(guider_ui.race_label_left, buf);
                s_last_g_val_left = val_left;
            }
            if (fabsf(val_right - s_last_g_val_right) > 0.01f) {
                snprintf(buf, sizeof(buf), "%.2f", val_right);
                lv_label_set_text(guider_ui.race_label_right, buf);
                s_last_g_val_right = val_right;
            }
        }
    }

    /* ---------- BLE 状态检查（每 100ms 一次）---------- */
    uint32_t now = lv_tick_get();
    if (now - s_last_ble_check_ms >= 100) {
        s_last_ble_check_ms = now;
        bool ble_conn = ble_is_connected();
        if (ble_conn != s_ble_conn_cached) {
            s_ble_conn_cached = ble_conn;
            if (ble_conn) {
                /* 蓝牙连接成功，停止呼吸动画并隐藏 arc7 */
                arc7_breath_stop();
                lv_obj_set_style_arc_opa(guider_ui.race_arc_7, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            } else {
                /* 蓝牙断开，启动呼吸动画（绿色）*/
                arc7_breath_start();
            }
        }
    }

    /* arc7 闪烁反馈处理 */
    if (s_arc7_flash_cnt > 0) {
        /* 进入闪烁时立即设置蓝色 */
        if (s_arc7_flash_tick == 0 && s_arc7_flash_phase == 0) {
            lv_arc_set_value(guider_ui.race_arc_7, 1000);
            lv_obj_set_style_arc_color(guider_ui.race_arc_7, lv_color_hex(0x0000ff),
                                       LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_opa(guider_ui.race_arc_7, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
        
        s_arc7_flash_tick++;
        if (s_arc7_flash_tick >= 2) {
            s_arc7_flash_tick = 0;
            s_arc7_flash_phase ^= 1;
            if (s_arc7_flash_phase == 0) {
                lv_arc_set_value(guider_ui.race_arc_7, 1000);
                lv_obj_set_style_arc_color(guider_ui.race_arc_7, lv_color_hex(0x0000ff),
                                           LV_PART_INDICATOR | LV_STATE_DEFAULT);
                lv_obj_set_style_arc_opa(guider_ui.race_arc_7, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            } else {
                lv_obj_set_style_arc_opa(guider_ui.race_arc_7, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
            }
            if (s_arc7_flash_phase == 1) {
                s_arc7_flash_cnt--;
                if (s_arc7_flash_cnt == 0) {
                    /* 闪烁结束 */
                    if (ble_is_connected()) {
                        /* 蓝牙已连接，隐藏 arc7 */
                        lv_obj_set_style_arc_opa(guider_ui.race_arc_7, 0,
                                                 LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    } else {
                        /* 蓝牙未连接，恢复绿色并启动呼吸动画 */
                        lv_obj_set_style_arc_color(guider_ui.race_arc_7, lv_color_hex(0x00ff24),
                                                   LV_PART_INDICATOR | LV_STATE_DEFAULT);
                        lv_obj_set_style_arc_opa(guider_ui.race_arc_7, 255,
                                                 LV_PART_INDICATOR | LV_STATE_DEFAULT);
                        arc7_breath_start();
                    }
                }
            }
        }
    }
}

/* ============================================================ */
void race_update(void)
{
    /* 非阻塞接收 OBD 数据，队列为空时立即返回，不阻塞 UI 渲染 */
    obd_data_t new_data;
    bool data_updated = obd_queue_receive(&new_data);
    
    /* 如果收到新数据，更新本地缓存 */
    if (data_updated) {
        s_obd_cache = new_data;
        s_obd_cache_valid = true;
    }
    
    /* 如果没有有效的 OBD 数据缓存，跳过 OBD 相关更新 */
    if (!s_obd_cache_valid) {
        /* 仍然可以处理不需要 OBD 数据的更新（如 G-force）*/
        race_update_non_obd();
        return;
    }
    
    /* 使用本地缓存的数据进行 UI 更新 */
    const obd_data_t *d = &s_obd_cache;
    char buf[32];

    /* ---------- G-force ---------- */
    {
        float ax, ay, az;
        if (hud_imu_get_accel(&ax, &ay, &az)) {
            float raw_x = (-ay) / 9.80665f;
            float raw_y = (az)  / 9.80665f;
            float jerk_x = raw_x - s_gforce.prev_x;
            float jerk_y = raw_y - s_gforce.prev_y;
            float jerk = (jerk_x < 0 ? -jerk_x : jerk_x) + (jerk_y < 0 ? -jerk_y : jerk_y);
            s_gforce.prev_x = raw_x; s_gforce.prev_y = raw_y;
            s_gforce.ema_x = ema_filter(s_gforce.ema_x, shake_comp(raw_x, jerk));
            s_gforce.ema_y = ema_filter(s_gforce.ema_y, shake_comp(raw_y, jerk));
            float off_x = g_to_pixel(s_gforce.ema_x);
            float off_y = g_to_pixel(s_gforce.ema_y);
            float px = G_CENTER_X + off_x - 10.0f;
            float py = G_CENTER_Y + off_y - 10.0f;
            if (px < G_CENTER_X - G_OUTER_R) px = G_CENTER_X - G_OUTER_R;
            if (px > G_CENTER_X + G_OUTER_R - 20.0f) px = G_CENTER_X + G_OUTER_R - 20.0f;
            if (py < G_CENTER_Y - G_OUTER_R) py = G_CENTER_Y - G_OUTER_R;
            if (py > G_CENTER_Y + G_OUTER_R - 20.0f) py = G_CENTER_Y + G_OUTER_R - 20.0f;

            /* 节流：只有位置变化超过阈值才更新 LVGL */
            if (fabsf(px - s_last_g_px) > G_POS_THRESH || fabsf(py - s_last_g_py) > G_POS_THRESH) {
                lv_obj_set_pos(guider_ui.race_label_G_piont, (lv_coord_t)px, (lv_coord_t)py);
                s_last_g_px = px; s_last_g_py = py;
            }

            /* G-force 数值标签 - 只在值变化超过阈值时更新，避免重复设置文本 */
            float val_top = s_gforce.ema_y < 0 ? -s_gforce.ema_y : 0.0f;
            float val_bottom = s_gforce.ema_y > 0 ? s_gforce.ema_y : 0.0f;
            float val_left = s_gforce.ema_x < 0 ? -s_gforce.ema_x : 0.0f;
            float val_right = s_gforce.ema_x > 0 ? s_gforce.ema_x : 0.0f;

            if (fabsf(val_top - s_last_g_val_top) > 0.01f) {
                snprintf(buf, sizeof(buf), "%.2f", val_top);
                lv_label_set_text(guider_ui.race_label_top, buf);
                s_last_g_val_top = val_top;
            }
            if (fabsf(val_bottom - s_last_g_val_bottom) > 0.01f) {
                snprintf(buf, sizeof(buf), "%.2f", val_bottom);
                lv_label_set_text(guider_ui.race_label_down, buf);
                s_last_g_val_bottom = val_bottom;
            }
            if (fabsf(val_left - s_last_g_val_left) > 0.01f) {
                snprintf(buf, sizeof(buf), "%.2f", val_left);
                lv_label_set_text(guider_ui.race_label_left, buf);
                s_last_g_val_left = val_left;
            }
            if (fabsf(val_right - s_last_g_val_right) > 0.01f) {
                snprintf(buf, sizeof(buf), "%.2f", val_right);
                lv_label_set_text(guider_ui.race_label_right, buf);
                s_last_g_val_right = val_right;
            }
        }
    }

    /* ---------- arc5/arc6: 双向扩散功率显示（与蓝牙状态无关）---------- */
    /* 先更新 arc 和内部显示值，功率数字已在此函数内更新 */
    set_power_arcs(d->power_kw);

    /* 冷却液温度 - 脏区域优化 */
    if (d->coolant != s_last_coolant) {
        s_last_coolant = d->coolant;
        snprintf(buf, sizeof(buf), "%d", d->coolant);
        lv_label_set_text(guider_ui.race_label_1, buf);
    }

    /* 电池温度平均 - 脏区域优化 */
    {
        int bt = (int)((d->bms_max_temp + d->bms_min_temp) / 2.0f);
        if (bt != s_last_batt_temp) {
            s_last_batt_temp = bt;
            snprintf(buf, sizeof(buf), "%d", bt);
            lv_label_set_text(guider_ui.race_label_2, buf);
        }
    }

    /* ---------- arc7: 闪烁反馈 或 蓝牙状态指示 ---------- */
    if (s_arc7_flash_cnt > 0) {
        /* G-force 归零反馈: 快速闪烁2下(60ms周期，加快速度) */
        /* 闪烁期间停止呼吸动画 */
        arc7_breath_stop();

        s_arc7_flash_tick++;
        if (s_arc7_flash_tick >= 3) {  /* 3×20ms = 60ms，加快闪烁速度 */
            s_arc7_flash_tick = 0;
            s_arc7_flash_phase = !s_arc7_flash_phase;
            s_arc7_flash_cnt--;

            /* 闪烁完成后，根据蓝牙状态处理 */
            if (s_arc7_flash_cnt == 0) {
                if (s_ble_conn_cached) {
                    /* 蓝牙已连接，隐藏 arc7 */
                    lv_obj_set_style_arc_opa(guider_ui.race_arc_7, 0,
                                             LV_PART_INDICATOR | LV_STATE_DEFAULT);
                } else {
                    /* 蓝牙未连接，恢复绿色并启动呼吸动画 */
                    lv_obj_set_style_arc_color(guider_ui.race_arc_7, lv_color_hex(0x00ff24),
                                               LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_arc_opa(guider_ui.race_arc_7, 255,
                                             LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    arc7_breath_start();
                }
            } else if (s_arc7_flash_cnt > 0) {
                /* 只有在闪烁过程中才设置蓝色和透明度 */
                lv_arc_set_value(guider_ui.race_arc_7, 1000);  /* UI 设计范围是 0-1000 */
                lv_obj_set_style_arc_color(guider_ui.race_arc_7, lv_color_hex(0x0066FF),  /* 蓝色闪烁 */
                                           LV_PART_INDICATOR | LV_STATE_DEFAULT);
                lv_obj_set_style_arc_opa(guider_ui.race_arc_7,
                                         s_arc7_flash_phase ? 0 : 255,
                                         LV_PART_INDICATOR | LV_STATE_DEFAULT);
            }
        } else {
            /* 闪烁 tick 未达到，继续闪烁 */
            lv_arc_set_value(guider_ui.race_arc_7, 1000);
            lv_obj_set_style_arc_color(guider_ui.race_arc_7, lv_color_hex(0x0066FF),
                                       LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_opa(guider_ui.race_arc_7,
                                     s_arc7_flash_phase ? 0 : 255,
                                     LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
    } else {
        /* ble 状态每 100ms 查一次 */
        uint32_t now = lv_tick_get();
        if ((now - s_last_ble_check_ms) >= 100) {
            bool prev_conn = s_ble_conn_cached;
            s_ble_conn_cached = ble_is_connected();
            s_last_ble_check_ms = now;

            /* 状态变化时更新动画 */
            if (!s_ble_conn_cached && prev_conn) {
                /* 蓝牙断开: 启动呼吸动画 */
                arc7_breath_start();
            } else if (s_ble_conn_cached && !prev_conn) {
                /* 蓝牙连接: 停止呼吸动画 */
                arc7_breath_stop();
            }
        }
        /* 蓝牙未连接且不在闪烁状态时，呼吸动画由 LVGL 自动管理 */
    }
}

/* ============================================================ */
void race_enter(void)
{
    gforce_filter_reset(&s_gforce);
    s_display_kw = -999.0f;
    s_last_display_kw = -999.0f;
    s_last_power_kw = -999.0f;
    s_last_ble_check_ms = 0;
    s_arc7_flash_cnt = 0;
    s_arc7_flash_tick = 0;
    s_last_coolant = -999;
    s_last_batt_temp = -999;
    s_last_g_px = 0.0f;
    s_last_g_py = 0.0f;
    s_last_arc_spread = -1;
    s_breath_running = false;
    s_last_g_val_top = 999.0f;
    s_last_g_val_bottom = 999.0f;
    s_last_g_val_left = 999.0f;
    s_last_g_val_right = 999.0f;

    /* 初始化蓝牙状态，如果未连接则启动呼吸动画 */
    s_ble_conn_cached = ble_is_connected();
    if (s_ble_conn_cached) {
        /* 蓝牙已连接，确保呼吸动画停止 */
        arc7_breath_stop();
        lv_obj_set_style_arc_opa(guider_ui.race_arc_7, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    } else {
        arc7_breath_start();
    }

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

void race_exit(void) 
{ 
    /* 停止呼吸动画，释放 GPU 和定时器资源 */
    arc7_breath_stop();
    /* 停止闪烁状态 */
    s_arc7_flash_cnt = 0;
    s_arc7_flash_tick = 0;
}
void race_boot_animation(void) { }
void race_G_zero(void)
{
    ESP_LOGI("RACE", "G_zero called");
    hud_imu_calibrate();
    gforce_filter_reset(&s_gforce);

    /* 启动 arc7 快速闪烁2下(120ms亮→120ms暗→120ms亮→120ms暗) */
    s_arc7_flash_cnt = 4;
    s_arc7_flash_phase = 0;
    s_arc7_flash_tick = 0;
}
