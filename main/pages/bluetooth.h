/* bluetooth.h — Bluetooth 界面业务 */
#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#ifdef __cplusplus
extern "C" {
#endif

void bluetooth_enter(void);
void bluetooth_exit(void);
void bluetooth_update(void);

/* 事件回调（由 screen.c 在 screen_init 中绑定） */
void on_sw_changed(lv_event_t *e);
void on_btn_scan(lv_event_t *e);

#ifdef __cplusplus
}
#endif

#endif