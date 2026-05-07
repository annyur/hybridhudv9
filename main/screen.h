/* screen.h -- 界面路由 */
#ifndef SCREEN_H
#define SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SCREEN_NONE = -1,
    SCREEN_RACE = 0,
    SCREEN_BLUETOOTH,
    SCREEN_COUNT
} screen_id_t;

void screen_init(void);
void screen_switch(screen_id_t id);
void screen_request_switch(screen_id_t id);
void screen_update(void);
screen_id_t screen_current(void);
void screen_set_refresh_pending(void);   /* 设置刷新标志（OBD数据到达时调用） */
void screen_clear_refresh_pending(void);  /* 清除刷新标志 */

#ifdef __cplusplus
}
#endif

#endif