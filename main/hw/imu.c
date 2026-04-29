/* imu.c — QMI8658 IMU */
#include "imu.h"
#include "board.h"
#include <qmi8658.h>          /* waveshare/qmi8658 组件 */

static qmi8658_handle_t s_imu = NULL;

void imu_init(void)
{
    /* 复用 BSP 已初始化的 I2C 总线 */
    i2c_master_bus_handle_t i2c_bus = bsp_i2c_get_handle();
    s_imu = qmi8658_create(i2c_bus, QMI8658_I2C_ADDRESS);
    qmi8658_init(s_imu, QMI8658_ACCEL_RANGE_8G, QMI8658_GYRO_RANGE_2048DPS);
}

void imu_update(void)
{
    if (!s_imu) return;
    qmi8658_data_ready(s_imu);   /* 刷新数据 */
}

void imu_calibrate(void)
{
    if (!s_imu) return;
    /* 零偏校准逻辑 */
}

bool imu_get_accel(float *ax, float *ay, float *az)
{
    if (!s_imu) return false;
    return qmi8658_get_accelerometer(s_imu, ax, ay, az) == ESP_OK;
}

bool imu_get_gyro(float *gx, float *gy, float *gz)
{
    if (!s_imu) return false;
    return qmi8658_get_gyroscope(s_imu, gx, gy, gz) == ESP_OK;
}

float imu_get_temp(void)
{
    if (!s_imu) return 0.0f;
    float t;
    qmi8658_get_temperature(s_imu, &t);
    return t;
}
