/* key.h — 物理按键接口
 * 硬件: ESP32-S3-Touch-AMOLED-1.75
 *   BOOT — GPIO0 (直连)
 *   PWR  — TCA9554 EXIO4 (I2C 扩展器)
 *
 * 功能:
 *   PWR  键 — 任意界面短按 → 进入 Setting
 *   BOOT 键 — Race 界面有效  → G-force 归零校准
 */
#ifndef KEY_H
#define KEY_H

#ifdef __cplusplus
extern "C" {
#endif

void key_init(void);
void key_poll(void);

#ifdef __cplusplus
}
#endif

#endif