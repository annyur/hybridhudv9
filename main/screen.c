/* screen.c — 界面路由 + 手势导航 */
#include "screen.h"
#include "lvgl.h"
#include "gui_guider.h"
#include "bsp/esp-bsp.h"

/* GUI Guider 全局 UI 实例定义 */
lv_ui guider_ui;

static screen_id_t s_current = SCREEN_GENERAL;
static screen_id_t s_main    = SCREEN_GENERAL;
static bool s_inited = false;

static void apply_screen(screen_id_t id)
{
    if (id >= SCREEN_COUNT || id == s_current) return;
    s_current = id;

    lv_obj_t *target = NULL;
    switch (id) {
        case SCREEN_GENERAL:   target = guider_ui.general;   break;
        case SCREEN_RACE:      target = guider_ui.race;      break;
        case SCREEN_SETTING:   target = guider_ui.setting;   break;
        case SCREEN_BLUETOOTH: target = guider_ui.bluetooth; break;
        default: return;
    }

    if (target) {
        bsp_display_lock(0);
        lv_screen_load(target);
        bsp_display_unlock();
    }
}

static void on_gesture_main(lv_event_t *e)
{
    (void)e;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_BOTTOM) {
        apply_screen(SCREEN_SETTING);
    }
}

static void on_gesture_setting(lv_event_t *e)
{
    (void)e;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_TOP) {
        apply_screen(s_main);
    }
}

static void on_btn_theme(lv_event_t *e)
{
    (void)e;
    s_main = (s_main == SCREEN_GENERAL) ? SCREEN_RACE : SCREEN_GENERAL;
    apply_screen(s_main);
}

static void on_btn_bluetooth(lv_event_t *e)
{
    (void)e;
    apply_screen(SCREEN_BLUETOOTH);
}

static void on_btn_back(lv_event_t *e)
{
    (void)e;
    apply_screen(SCREEN_SETTING);
}

void screen_init(void)
{
    if (s_inited) return;
    s_inited = true;

    setup_ui(&guider_ui);
    setup_scr_race(&guider_ui);
    setup_scr_setting(&guider_ui);
    setup_scr_bluetooth(&guider_ui);
      /* ===== 禁用 Arc / Slider 触摸，防止拦截手势 ===== */
    lv_obj_clear_flag(guider_ui.general_arc_rpm,     LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.general_arc_speed,   LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.general_arc_oil,      LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.general_arc_energy,   LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.general_slider_energy, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_clear_flag(guider_ui.race_arc_4, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.race_arc_5, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.race_arc_6, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.race_arc_7, LV_OBJ_FLAG_CLICKABLE);
    /* ================================================ */

    lv_obj_add_event_cb(guider_ui.general, on_gesture_main, LV_EVENT_GESTURE, NULL);
    lv_obj_add_flag(guider_ui.general, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(guider_ui.race, on_gesture_main, LV_EVENT_GESTURE, NULL);
    lv_obj_add_flag(guider_ui.race, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(guider_ui.setting, on_gesture_setting, LV_EVENT_GESTURE, NULL);
    lv_obj_add_flag(guider_ui.setting, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_add_event_cb(guider_ui.setting_btn_2, on_btn_theme,     LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(guider_ui.setting_btn_3, on_btn_bluetooth, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(guider_ui.bluetooth_btn_back, on_btn_back,  LV_EVENT_CLICKED, NULL);

    s_main = SCREEN_GENERAL;
    s_current = SCREEN_GENERAL;
}

void screen_switch(screen_id_t id)
{
    apply_screen(id);
}

void screen_update(void)
{
    /* 业务刷新 */
}

screen_id_t screen_current(void)
{
    return s_current;
}