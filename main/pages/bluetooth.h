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
void bluetooth_reset_reconnect_state(void);  /* 重置重连状态，允许重新连接 */

#ifdef __cplusplus
}
#endif

#endif