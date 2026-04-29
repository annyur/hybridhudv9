/* screen.c — 界面路由 */
#include "screen.h"
#include "lvgl.h"
#include "gui_guider.h"
#include "general.h"
#include "race.h"
#include "setting.h"
#include "bluetooth.h"
#include "bsp/esp-bsp.h"   /* bsp_display_lock/unlock 在这里 */

/* GUI Guider 全局 UI 实例定义 */
lv_ui guider_ui;

typedef struct {
    void (*enter)(void);
    void (*exit)(void);
    void (*update)(void);
} screen_ops_t;

static const screen_ops_t s_screens[SCREEN_COUNT] = {
    [SCREEN_GENERAL]   = { general_enter,   general_exit,   general_update },
    [SCREEN_RACE]      = { race_enter,      race_exit,      race_update },
    [SCREEN_SETTING]   = { setting_enter,   setting_exit,   setting_update },
    [SCREEN_BLUETOOTH] = { bluetooth_enter, bluetooth_exit, bluetooth_update },
};

static screen_id_t s_current = SCREEN_GENERAL;
static bool s_inited = false;

void screen_init(void)
{
    if (s_inited) return;
    s_inited = true;

    setup_ui(&guider_ui);

    s_current = SCREEN_GENERAL;
    if (s_screens[s_current].enter) {
        s_screens[s_current].enter();
    }
}

void screen_switch(screen_id_t id)
{
    if (id >= SCREEN_COUNT || id == s_current) return;

    if (s_screens[s_current].exit) {
        s_screens[s_current].exit();
    }

    lv_obj_t *target = NULL;
    switch (id) {
        case SCREEN_GENERAL:   target = guider_ui.general;   break;
        case SCREEN_RACE:      target = guider_ui.race;      break;
        case SCREEN_SETTING:   target = guider_ui.setting;   break;
        case SCREEN_BLUETOOTH: target = guider_ui.bluetooth; break;
        default: break;
    }

    if (target) {
        bsp_display_lock(0);
        lv_screen_load(target);
        bsp_display_unlock();
    }

    s_current = id;

    if (s_screens[id].enter) {
        s_screens[id].enter();
    }
}

void screen_update(void)
{
    if (s_current < SCREEN_COUNT && s_screens[s_current].update) {
        s_screens[s_current].update();
    }
}

screen_id_t screen_current(void)
{
    return s_current;
}