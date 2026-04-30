/* race.h */
#ifndef RACE_H
#define RACE_H

#ifdef __cplusplus
extern "C" {
#endif

void race_enter(void);
void race_exit(void);
void race_update(void);

/* 启动 Race 开机动画（Race 为启动界面时由 main.c 调用一次） */
void race_boot_animation(void);

#ifdef __cplusplus
}
#endif

#endif