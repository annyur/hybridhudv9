/* imu.h — QMI8658 IMU */
#ifndef IMU_H
#define IMU_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void hud_imu_init(void);
void hud_imu_update(void);
void hud_imu_calibrate(void);
bool hud_imu_get_accel(float *ax, float *ay, float *az);
bool hud_imu_get_gyro(float *gx, float *gy, float *gz);
float hud_imu_get_temp(void);

#ifdef __cplusplus
}
#endif

#endif