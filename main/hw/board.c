/* board.c — 板级初始化：显示 + 触摸 + 音频 + SD */
#include "board.h"

#include <bsp/esp-bsp.h>

void board_init(void)
{
    /* I2C 总线（所有传感器共享） */
    bsp_i2c_init();

    /* 显示 + 触摸 + LVGL v9（BSP 内部已配置 DMA 双缓冲） */
    bsp_display_start();

    /* 背光 */
    bsp_display_backlight_on();

    /* 音频（如需） */
    /* bsp_audio_init(NULL); */
    /* bsp_audio_codec_speaker_init(); */
    /* bsp_audio_codec_microphone_init(); */

    /* SD 卡（如需） */
    /* bsp_sdcard_mount(); */
}
