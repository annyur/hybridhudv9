/* general.h */
#ifndef GENERAL_H
#define GENERAL_H

#ifdef __cplusplus
extern "C" {
#endif

void general_enter(void);
void general_exit(void);
void general_update(void);

/* 启动开机动画（由 main.c 在初始化完成后调用一次） */
void general_boot_animation(void);

#ifdef __cplusplus
}
#endif

#endif