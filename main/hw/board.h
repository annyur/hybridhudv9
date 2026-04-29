/* board.h — 板级初始化封装 */
#ifndef BOARD_H
#define BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bsp/esp-bsp.h>   /* BSP API：bsp_i2c_get_handle() 等 */

void board_init(void);

#ifdef __cplusplus
}
#endif

#endif