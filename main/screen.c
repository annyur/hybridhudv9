/* screen.c — 界面路由 + 坐标手势导航 */
#include "screen.h"
#include "lvgl.h"
#include "gui_guider.h"
#include "general.h"
#include "race.h"
#include "setting.h"
#include "bluetooth.h"
#include "bsp/esp-bsp.h"

/* GUI Guider 全局 UI 实例定义 */
lv_ui guider_ui;

/* 页面操作表 */
typedef struct {
    void (*enter)(void);
    void (*exit)(void);
    void (*update)(void);
} screen_ops_t;

static const screen_ops_t s_screens[] = {
    [SCREEN_GENERAL]   = { general_enter,   general_exit,   general_update },
    [SCREEN_RACE]      = { race_enter,      race_exit,      race_update },
    [SCREEN_SETTING]   = { setting_enter,   setting_exit,   setting_update },
    [SCREEN_BLUETOOTH] = { bluetooth_enter, bluetooth_exit, bluetooth_update },
};

static screen_id_t s_current = SCREEN_GENERAL;
static screen_id_t s_main    = SCREEN_GENERAL;
static bool s_inited = false;

/* 手势跟踪 */
static lv_coord_t s_touch_start_y = 0;
static bool       s_touch_tracking = false;
static uint32_t   s_last_switch_ms = 0;

/* ========== 内部辅助：递归禁用 Arc/Slider/Switch/Button/Img 的点击 ========== */
static void disable_interactive_children(lv_obj_t *parent)
{
    if (!parent) return;
    uint32_t cnt = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        if (!child) continue;

        const lv_obj_class_t *cls = lv_obj_get_class(child);
        if (cls == &lv_arc_class ||
            cls == &lv_slider_class ||
            cls == &lv_switch_class ||
            cls == &lv_button_class ||
            cls == &lv_image_class) {
            lv_obj_clear_flag(child, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(child, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        }
        /* 递归处理容器 */
        disable_interactive_children(child);
    }
}

/* 执行切屏（带 enter/exit 回调） */
static void apply_screen(screen_id_t id)
{
    if (id >= SCREEN_COUNT || id == s_current) return;

    /* 防抖 */
    uint32_t now = lv_tick_get();
    if (now - s_last_switch_ms < 200) return;
    s_last_switch_ms = now;

    /* 退出旧界面 */
    if (s_screens[s_current].exit) {
        s_screens[s_current].exit();
    }

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

    /* 进入新界面 */
    if (s_screens[id].enter) {
        s_screens[id].enter();
    }
}

/* Theme 按钮：general ↔ race 切换 */
static void on_btn_theme(lv_event_t *e)
{
    (void)e;
    s_main = (s_main == SCREEN_GENERAL) ? SCREEN_RACE : SCREEN_GENERAL;
    apply_screen(s_main);
}

/* Bluetooth 按钮：setting → bluetooth */
static void on_btn_bluetooth(lv_event_t *e)
{
    (void)e;
    apply_screen(SCREEN_BLUETOOTH);
}

/* Back 按钮：bluetooth → setting */
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

    /* ===== 递归禁用 general / race 所有 Arc/Slider/Switch/Button/Img 的点击 ===== */
    disable_interactive_children(guider_ui.general);
    disable_interactive_children(guider_ui.race);
    /* ========================================================================== */

    /* 按钮事件绑定 */
    lv_obj_add_event_cb(guider_ui.setting_btn_2, on_btn_theme,     LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(guider_ui.setting_btn_3, on_btn_bluetooth, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(guider_ui.bluetooth_btn_back, on_btn_back, LV_EVENT_CLICKED, NULL);

    s_main = SCREEN_GENERAL;
    s_current = SCREEN_GENERAL;

    /* 触发 general 的 enter */
    if (s_screens[SCREEN_GENERAL].enter) {
        s_screens[SCREEN_GENERAL].enter();
    }
}

void screen_switch(screen_id_t id)
{
    apply_screen(id);
}

void screen_update(void)
{
    gesture_poll();

    /* 调用当前界面的 update */
    if (s_current < SCREEN_COUNT && s_screens[s_current].update) {
        s_screens[s_current].update();
    }
}

screen_id_t screen_current(void)
{
    return s_current;
}