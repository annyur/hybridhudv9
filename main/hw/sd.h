/* sd.h — SD 卡 */
#ifndef SD_H
#define SD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

void sd_init(void);
bool sd_is_mounted(void);

#ifdef __cplusplus
}
#endif

#endif