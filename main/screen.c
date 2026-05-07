/* screen.c -- simplified: race + bluetooth only */
#include "lvgl.h"
#include "gui_guider.h"
#include "events_init.h"
#include "race.h"
#include "bluetooth.h"
#include "screen.h"

/* guider_ui 全局变量定义（GUI Guider 生成代码中缺少定义） */
lv_ui guider_ui;

static screen_id_t s_current = SCREEN_RACE;
static uint32_t    s_last_switch_ms = 0;

void screen_init(void)
{
    setup_ui(&guider_ui);
    events_init(&guider_ui);

    /* setup_ui() 只创建了 race screen，手动创建 bluetooth screen */
    setup_scr_bluetooth(&guider_ui);

    /* 蓝牙界面事件注册 — 必须在 setup_ui 之后，且不在 screen_switch 路径中 */
    bluetooth_ui_init();

    s_current = SCREEN_RACE;
    lv_screen_load(guider_ui.race);
    race_enter();
}

void screen_switch(screen_id_t id)
{
    if (id == s_current) return;
    uint32_t now = lv_tick_get();
    if (now - s_last_switch_ms < 200) return;
    s_last_switch_ms = now;

    if (s_current == SCREEN_RACE)      race_exit();
    if (s_current == SCREEN_BLUETOOTH) bluetooth_exit();
    s_current = id;

    lv_obj_t *target = (id == SCREEN_RACE) ? guider_ui.race : guider_ui.bluetooth;
    if (target) lv_screen_load(target);

    if (id == SCREEN_RACE)      race_enter();
    if (id == SCREEN_BLUETOOTH) bluetooth_enter();
}

screen_id_t screen_current(void)
{
    return s_current;
}

void screen_update(void)
{
    if (s_current == SCREEN_RACE) {
        race_update();
    } else if (s_current == SCREEN_BLUETOOTH) {
        bluetooth_update();
    }
}