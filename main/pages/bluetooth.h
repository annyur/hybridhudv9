/* bluetooth.h — Bluetooth 界面业务 */
#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#ifdef __cplusplus
extern "C" {
#endif

void bluetooth_enter(void);
void bluetooth_exit(void);
void bluetooth_update(void);

#ifdef __cplusplus
}
#endif

#endif