/* bluetooth.h */
#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#ifdef __cplusplus
extern "C" {
#endif

void bluetooth_enter(void);
void bluetooth_exit(void);
void bluetooth_update(void);

/* 开机动画 */
void general_boot_animation(void);

/* 后台自动回连（由 main.c 循环调用，无需进入界面） */
void bluetooth_auto_reconnect(void);

#ifdef __cplusplus
}
#endif

#endif
