/* board.c — 板级初始化封装 */
#include "board.h"

void board_init(void)
{
    bsp_display_start();      /* 内部已初始化 I2C、显示、触摸 */
    bsp_display_backlight_on();
}