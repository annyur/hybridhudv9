/* board.h — 板级初始化封装 */
#ifndef BOARD_H
#define BOARD_H

#include "bsp/esp-bsp.h"   /* 必须在 __cplusplus 外面！ */

#ifdef __cplusplus
extern "C" {
#endif

void board_init(void);

#ifdef __cplusplus
}
#endif

#endif