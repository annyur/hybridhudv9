/* imu.h — QMI8658 IMU */
#ifndef IMU_H
#define IMU_H

#ifdef __cplusplus
extern "C" {
#endif

void imu_init(void);
void imu_update(void);
void imu_calibrate(void);
bool imu_get_accel(float *ax, float *ay, float *az);
bool imu_get_gyro(float *gx, float *gy, float *gz);
float imu_get_temp(void);

#ifdef __cplusplus
}
#endif

#endif