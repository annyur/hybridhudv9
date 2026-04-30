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
    lv_anim_set_delay(&a, 500);   /* 延迟0.75秒 */
    lv_anim_start(&a);

    /* Speed arc：延迟0.5秒 → 0 → 200 → 0，1000ms up + 1000ms down */
    lv_anim_init(&a);
    lv_anim_set_var(&a, guider_ui.general_arc_speed);
    lv_anim_set_values(&a, 0, 200);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_arc_set_value);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_playback_time(&a, 1000);
    lv_anim_set_playback_delay(&a, 0);
    lv_anim_set_delay(&a, 750);    /* 延迟0.5秒 */
    lv_anim_start(&a);
}

/* 上次的值，用于减少不必要的刷新 */
static int   s_last_rpm     = -1;
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
    /* 重置 OBD 刷新标记，确保切回来后立即刷新 */
    s_last_rpm = s_last_speed = s_last_coolant = s_last_oil = s_last_soc = -1;
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
        /* 动画播放期间不覆盖 arc，动画结束后自然归 0 */
        if (s_boot_done) {
            lv_arc_set_value(guider_ui.general_arc_rpm, d->rpm);
        }
        if (d->rpm == 0) {
            lv_label_set_text(guider_ui.general_label_rpm_number, "");
        } else {
            snprintf(buf, sizeof(buf), "%d", d->rpm);
            lv_label_set_text(guider_ui.general_label_rpm_number, buf);
        }
    }

    /* ========== 车速表 ========== */
    if (d->speed != s_last_speed) {
        s_last_speed = d->speed;
        if (s_boot_done) {
            lv_arc_set_value(guider_ui.general_arc_speed, d->speed);
        }
        snprintf(buf, sizeof(buf), "%02d", d->speed);
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

    /* ========== 功率 (kw) ========== */
    if (fabsf(d->power_kw - s_last_kw) > 0.1f) {
        s_last_kw = d->power_kw;
        snprintf(buf, sizeof(buf), "%.1f", d->power_kw);
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

    /* ========== 续航里程 (km) ========== */
    if (d->dist != s_last_dist) {
        s_last_dist = d->dist;
        snprintf(buf, sizeof(buf), "%d", d->dist);
        lv_label_set_text(guider_ui.general_label_energy_number_2, buf);
    }

    /* ========== 总里程 (odometer) ========== */
    if (fabsf(d->odometer - s_last_odom) > 0.5f) {
        s_last_odom = d->odometer;
        snprintf(buf, sizeof(buf), "%.0f", d->odometer);
        lv_label_set_text(guider_ui.general_label_1, buf);
    }

    /* ========== 电池电压 ========== */
    if (fabsf(d->batt_v - s_last_batt_v) > 0.1f) {
        s_last_batt_v = d->batt_v;
        snprintf(buf, sizeof(buf), "%.1f", d->batt_v);
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