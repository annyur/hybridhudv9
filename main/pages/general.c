/* general.c — General 界面业务 + OBD 数据绑定 + LVGL 动画 */
#include "general.h"
#include "obd.h"
#include "ble.h"
#include "imu.h"
#include "rtc.h"
#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gui_guider.h"

extern lv_ui guider_ui;

static bool s_active = false;
static bool s_boot_done = false;   /* 开机动画只启动一次 */

/* ===== Slider 呼吸：手动驱动，24步/480ms，和 race G 点一致 ===== */
static void slider_breath_step(int tick)
{
    /* tick: 0~24，周期 480ms @ 20ms */
    int phase = tick % 24;
    if (phase < 8) {
        /* 0~8 (0~160ms): 从中间向两端扩展 */
        int ext = (phase * 50) / 8;
        lv_slider_set_left_value(guider_ui.general_slider_energy, 50 - ext, LV_ANIM_OFF);
        lv_slider_set_value(guider_ui.general_slider_energy, 50 + ext, LV_ANIM_OFF);
        lv_obj_set_style_opa(guider_ui.general_slider_energy, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (phase < 16) {
        /* 8~16 (160~320ms): 渐隐 */
        int opa = 255 - ((phase - 8) * 255) / 8;
        lv_obj_set_style_opa(guider_ui.general_slider_energy, opa, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        /* 16~24 (320~480ms): 透明等待 */
        lv_obj_set_style_opa(guider_ui.general_slider_energy, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_slider_set_left_value(guider_ui.general_slider_energy, 50, LV_ANIM_OFF);
        lv_slider_set_value(guider_ui.general_slider_energy, 50, LV_ANIM_OFF);
    }
}

/* ===== 开机动画：启动 LVGL 内置 sweep ===== */
void general_boot_animation(void)
{
    if (s_boot_done) return;
    s_boot_done = true;

    /* RPM arc：延迟1秒 → 0 → 8000 → 0，1000ms up + 1000ms down */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, guider_ui.general_arc_rpm);
    lv_anim_set_values(&a, 0, 8000);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_arc_set_value);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_playback_time(&a, 1000);
    lv_anim_set_playback_delay(&a, 0);
    lv_anim_set_delay(&a, 750);   /* 延迟0.75秒 */
    lv_anim_start(&a);

    /* Speed arc：延迟0.5秒 → 0 → 200 → 0，1000ms up + 1000ms down */
    lv_anim_init(&a);
    lv_anim_set_var(&a, guider_ui.general_arc_speed);
    lv_anim_set_values(&a, 0, 200);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_arc_set_value);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_playback_time(&a, 1000);
    lv_anim_set_playback_delay(&a, 0);
    lv_anim_set_delay(&a, 500);    /* 延迟0.5秒 */
    lv_anim_start(&a);
}

/* 上次的值，用于减少不必要的刷新 */
static int   s_last_rpm     = -1;
static int   s_display_speed = -1;
static float s_speed_frac    = 0.0f;  /* 浮点累积器：0→100 极限 7 秒 */
static int   s_last_speed   = -1;
static int   s_last_coolant = -1;
static int   s_last_oil     = -1;
static int   s_last_soc     = -1;
static float s_last_kw      = -999.0f;
static int   s_last_dist    = -1;
static float s_last_batt_v  = -1.0f;
static float s_last_odom    = -1.0f;
static int   s_last_time_mm = -1;
static float s_last_slider_kw = -999.0f;

void general_enter(void)
{
    s_active = true;
    s_boot_done = true;

    /* 删除 GUI Guider 可能启动的 arc 默认动画，防止覆盖 OBD 实时数据 */
    lv_anim_del(guider_ui.general_arc_rpm,   NULL);
    lv_anim_del(guider_ui.general_arc_speed,  NULL);
    lv_anim_del(guider_ui.general_arc_oil,    NULL);
    lv_anim_del(guider_ui.general_arc_energy, NULL);

    /* 立即同步到当前 OBD 数据，避免 arc 停留在动画残留值 */
    const obd_data_t *d = obd_get_data();
    if (d) {
        lv_arc_set_value(guider_ui.general_arc_rpm,   d->rpm);
        lv_arc_set_value(guider_ui.general_arc_speed,  d->speed);
        lv_arc_set_value(guider_ui.general_arc_oil,    d->oil > 100 ? 100 : (d->oil < 0 ? 0 : d->oil));
        lv_arc_set_value(guider_ui.general_arc_energy, (int)(d->epa_soc));
    }

    /* 重置 OBD 刷新标记，确保切回来后立即刷新 */
    s_last_rpm = s_last_speed = s_last_coolant = s_last_oil = s_last_soc = -1;
    s_display_speed = -1;
    s_speed_frac = 0.0f;
    s_last_kw = s_last_slider_kw = -999.0f;
    s_last_dist = -1;
    s_last_batt_v = -1.0f;
    s_last_odom = -1.0f;
    s_last_time_mm = -1;
}

void general_exit(void)
{
    s_active = false;
}

void general_update(void)
{
    if (!s_active) return;

    const obd_data_t *d = obd_get_data();
    if (!d) return;

    char buf[32];

    /* ========== 转速表 ========== */
    if (d->rpm != s_last_rpm) {
        s_last_rpm = d->rpm;
        lv_arc_set_value(guider_ui.general_arc_rpm, d->rpm);
        if (d->rpm == 0) {
            lv_label_set_text(guider_ui.general_label_rpm_number, "");
        } else {
            snprintf(buf, sizeof(buf), "%d", d->rpm);
            lv_label_set_text(guider_ui.general_label_rpm_number, buf);
        }
    }

    /* ========== 车速表（固定速率动画：0→100 极限 7 秒 ≈ 0.433/帧） ========== */
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
        lv_arc_set_value(guider_ui.general_arc_speed, s_display_speed);
        snprintf(buf, sizeof(buf), "%02d", s_display_speed);
        lv_label_set_text(guider_ui.general_label_speed_number, buf);
    }

    /* ========== 油量/电量表 (0-100%) ========== */
    int oil_pct = d->oil > 100 ? 100 : (d->oil < 0 ? 0 : d->oil);
    if (oil_pct != s_last_oil) {
        s_last_oil = oil_pct;
        lv_arc_set_value(guider_ui.general_arc_oil, oil_pct);
        snprintf(buf, sizeof(buf), "%d", d->oil);
        lv_label_set_text(guider_ui.general_label_oil_number, buf);
    }

    /* ========== 能量/续航表 (0-100%) ========== */
    int soc = (int)(d->epa_soc);
    if (soc > 100) soc = 100;
    if (soc < 0)   soc = 0;
    if (soc != s_last_soc) {
        s_last_soc = soc;
        lv_arc_set_value(guider_ui.general_arc_energy, soc);
    }

    /* ========== 冷却液温度 ========== */
    if (d->coolant != s_last_coolant) {
        s_last_coolant = d->coolant;
        snprintf(buf, sizeof(buf), "%d", d->coolant);
        lv_label_set_text(guider_ui.general_label_temp, buf);
    }

    /* ========== 功率 (kw) — 符号反向：正数=耗电显示"-"，负数=回收显示"+" ========== */
    if (fabsf(d->power_kw - s_last_kw) > 0.1f) {
        s_last_kw = d->power_kw;
        if (d->power_kw > 0) {
            snprintf(buf, sizeof(buf), "-%.1f", d->power_kw);
        } else if (d->power_kw < 0) {
            snprintf(buf, sizeof(buf), "+%.1f", -d->power_kw);
        } else {
            snprintf(buf, sizeof(buf), "0.0");
        }
        lv_label_set_text(guider_ui.general_label_energy_number, buf);
    }

    /* ========== 功率 Slider（双向：中间为0） ========== */
    if (!ble_is_connected()) {
        /* 未连接蓝牙：手动呼吸动画 */
        static int s_slider_tick = 0;
        s_slider_tick++;
        slider_breath_step(s_slider_tick);
    } else {
        /* 已连接蓝牙：恢复不透明，显示实际功率 */
        lv_obj_set_style_opa(guider_ui.general_slider_energy, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

#define MAX_KW 100.0f
        if (fabsf(d->power_kw - s_last_slider_kw) > 0.5f) {
            s_last_slider_kw = d->power_kw;
            if (d->power_kw >= 0) {
                int right = 50 + (int)((d->power_kw / MAX_KW) * 50.0f);
                if (right > 100) right = 100;
                lv_slider_set_left_value(guider_ui.general_slider_energy, 50, LV_ANIM_OFF);
                lv_slider_set_value(guider_ui.general_slider_energy, right, LV_ANIM_OFF);
            } else {
                int left = 50 - (int)((-d->power_kw / MAX_KW) * 50.0f);
                if (left < 0) left = 0;
                lv_slider_set_left_value(guider_ui.general_slider_energy, left, LV_ANIM_OFF);
                lv_slider_set_value(guider_ui.general_slider_energy, 50, LV_ANIM_OFF);
            }
        }
    }

    /* ========== 剩余可用能量 (kWh) ========== */
    if (d->bms_remain_wh != s_last_dist) {
        s_last_dist = d->bms_remain_wh;
        snprintf(buf, sizeof(buf), "%.1f", d->bms_remain_wh / 1000.0f);
        lv_label_set_text(guider_ui.general_label_energy_number_2, buf);
    }

    /* ========== 总里程 (odometer) ========== */
    if (fabsf(d->odometer - s_last_odom) > 0.5f) {
        s_last_odom = d->odometer;
        snprintf(buf, sizeof(buf), "%.0f", d->odometer);
        lv_label_set_text(guider_ui.general_label_1, buf);
    }

    /* ========== 平均功耗（自设备启动后） ========== */
    if (fabsf(d->avg_kw - s_last_batt_v) > 0.1f) {
        s_last_batt_v = d->avg_kw;
        snprintf(buf, sizeof(buf), "%.1f", d->avg_kw);
        lv_label_set_text(guider_ui.general_label_2, buf);
    }

    /* ========== 能耗 (kw/H 或 L/100km) ========== */
    if (d->speed > 0) {
        float consum = d->power_kw / (d->speed / 100.0f);
        snprintf(buf, sizeof(buf), "%.1f", consum);
        lv_label_set_text(guider_ui.general_label_3, buf);
    }

    /* ========== RTC 时间 ========== */
    {
        struct tm ti;
        hud_rtc_get_time(&ti);
        int now_mm = ti.tm_hour * 60 + ti.tm_min;
        if (now_mm != s_last_time_mm) {
            s_last_time_mm = now_mm;
            snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
            lv_label_set_text(guider_ui.general_label_time, buf);
        }
    }

    /* ========== 单位标签（固定文本，首次更新即可） ========== */
    static bool s_labels_inited = false;
    if (!s_labels_inited) {
        s_labels_inited = true;
        lv_label_set_text(guider_ui.general_label_4, "kw/H");
        lv_label_set_text(guider_ui.general_label_5, "L/km");
        lv_label_set_text(guider_ui.general_label_6, "km");
        lv_label_set_text(guider_ui.general_label_unit_kw, "kw");
        lv_label_set_text(guider_ui.general_label_trip, "trip");
    }
}