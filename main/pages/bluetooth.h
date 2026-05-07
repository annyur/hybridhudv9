/* bluetooth.h */
#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#ifdef __cplusplus
extern "C" {
#endif

void bluetooth_ui_init(void);   /* screen_init 时调用一次，注册事件回调 */
void bluetooth_enter(void);
void bluetooth_exit(void);
void bluetooth_update(void);
void bluetooth_auto_reconnect(void);
void bluetooth_reconnect_init(void);  /* 初始化事件驱动的BLE重连 */
void bluetooth_on_disconnect(void);   /* BLE断开事件回调（供conn.c调用） */

#ifdef __cplusplus
}
#endif

#endif