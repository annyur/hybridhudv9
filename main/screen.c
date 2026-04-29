/* screen.c — 界面路由 + 坐标手势 + 滑动切屏 */
#include "screen.h"
#include "lvgl.h"
#include "gui_guider.h"
#include "bsp/esp-bsp.h"

/* GUI Guider 全局 UI 实例定义 */
lv_ui guider_ui;

static screen_id_t s_current = SCREEN_GENERAL;
static screen_id_t s_main    = SCREEN_GENERAL;
static bool s_inited = false;

/* 触摸跟踪 */
static lv_coord_t s_touch_start_y = 0;
static bool       s_touch_tracking = false;
static uint32_t   s_last_switch_ms = 0;

/* 执行切屏（带滑动动画） */
static void apply_screen(screen_id_t id)
{
    if (id >= SCREEN_COUNT || id == s_current) return;

    /* 防抖：300ms 内禁止重复切屏 */
    uint32_t now = lv_tick_get();
    if (now - s_last_switch_ms < 300) return;
    s_last_switch_ms = now;
    s_current = id;

    lv_obj_t *target = NULL;
    lv_screen_load_anim_t anim = LV_SCREEN_LOAD_ANIM_NONE;
    switch (id) {
        case SCREEN_GENERAL:
            target = guider_ui.general;
            /* 从 setting 底部上划回来：新屏幕从底部向上推入 */
            anim = LV_SCREEN_LOAD_ANIM_MOVE_BOTTOM;
            break;
        case SCREEN_RACE:
            target = guider_ui.race;
            anim = LV_SCREEN_LOAD_ANIM_MOVE_BOTTOM;
            break;
        case SCREEN_SETTING:
            target = guider_ui.setting;
            /* 从主屏顶部下滑：新屏幕从顶部向下推入 */
            anim = LV_SCREEN_LOAD_ANIM_MOVE_TOP;
            break;
        case SCREEN_BLUETOOTH:
            target = guider_ui.bluetooth;
            /* setting → bluetooth：向右推入 */
            anim = LV_SCREEN_LOAD_ANIM_MOVE_LEFT;
            break;
        default: return;
    }

    if (target) {
        bsp_display_lock(0);
        /* 200ms 滑动，auto_del=false（所有 screen 预创建，不能删） */
        lv_screen_load_anim(target, anim, 200, 0, false);
        bsp_display_unlock();
    }
}

/* Theme 按钮：general ↔ race 切换 */
static void on_btn_theme(lv_event_t *e)
{
    (void)e;
    s_main = (s_main == SCREEN_GENERAL) ? SCREEN_RACE : SCREEN_GENERAL;
    apply_screen(s_main);
}

/* Bluetooth 按钮 */
static void on_btn_bluetooth(lv_event_t *e)
{
    (void)e;
    apply_screen(SCREEN_BLUETOOTH);
}

/* Back 按钮 */
static void on_btn_back(lv_event_t *e)
{
    (void)e;
    apply_screen(SCREEN_SETTING);
}

/* 底层手势轮询（不依赖 LVGL 事件系统） */
static void gesture_poll(void)
{
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (!indev) return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_indev_state_t state = lv_indev_get_state(indev);

    if (state == LV_INDEV_STATE_PRESSED && !s_touch_tracking) {
        s_touch_tracking = true;
        s_touch_start_y = p.y;
    }
    else if (state == LV_INDEV_STATE_RELEASED && s_touch_tracking) {
        s_touch_tracking = false;
        lv_coord_t dy = p.y - s_touch_start_y;

        if (s_current == SCREEN_GENERAL || s_current == SCREEN_RACE) {
            /* 顶部下滑：起点在顶部 80px，向下划超 60px */
            if (s_touch_start_y < 80 && dy > 60) {
                apply_screen(SCREEN_SETTING);
            }
        }
        else if (s_current == SCREEN_SETTING) {
            /* 底部上划：起点在底部 80px（386+），向上划超 60px */
            if (s_touch_start_y > 386 && dy < -60) {
                apply_screen(s_main);
            }
        }
    }
}

void screen_init(void)
{
    if (s_inited) return;
    s_inited = true;

    setup_ui(&guider_ui);
    setup_scr_race(&guider_ui);
    setup_scr_setting(&guider_ui);
    setup_scr_bluetooth(&guider_ui);

    /* 禁用 Arc / Slider 触摸 */
    lv_obj_clear_flag(guider_ui.general_arc_rpm,       LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.general_arc_speed,     LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.general_arc_oil,        LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.general_arc_energy,     LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.general_slider_energy, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_clear_flag(guider_ui.race_arc_4, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.race_arc_5, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.race_arc_6, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.race_arc_7, LV_OBJ_FLAG_CLICKABLE);

    /* 按钮事件绑定 */
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
    gesture_poll();
}

screen_id_t screen_current(void)
{
    return s_current;
}
