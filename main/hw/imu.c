/* imu.c — QMI8658 IMU */
#include "imu.h"
#include "board.h"
#include <qmi8658.h>
#include <esp_log.h>

static const char *TAG = "IMU";
static qmi8658_dev_t s_imu = {0};
static bool s_imu_inited = false;

void imu_init(void)
{
    i2c_master_bus_handle_t i2c_bus = bsp_i2c_get_handle();
    esp_err_t ret = qmi8658_init(&s_imu, i2c_bus, QMI8658_ADDRESS_HIGH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "QMI8658 init failed: %d", ret);
        return;
    }

    qmi8658_set_accel_range(&s_imu, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(&s_imu, QMI8658_ACCEL_ODR_1000HZ);
    qmi8658_set_gyro_range(&s_imu, QMI8658_GYRO_RANGE_2048DPS);
    qmi8658_set_gyro_odr(&s_imu, QMI8658_GYRO_ODR_1000HZ);
    qmi8658_enable_sensors(&s_imu, QMI8658_ENABLE_ACCEL | QMI8658_ENABLE_GYRO);

    s_imu_inited = true;
    ESP_LOGI(TAG, "QMI8658 initialized");
}

void imu_update(void) { }

void imu_calibrate(void)
{
    if (!s_imu_inited) return;
}

bool imu_get_accel(float *ax, float *ay, float *az)
{
    if (!s_imu_inited) return false;
    return qmi8658_read_accel_mps2(&s_imu, ax, ay, az) == ESP_OK;
}

bool imu_get_gyro(float *gx, float *gy, float *gz)
{
    if (!s_imu_inited) return false;
    return qmi8658_read_gyro_rads(&s_imu, gx, gy, gz) == ESP_OK;
}

float imu_get_temp(void)
{
    if (!s_imu_inited) return 0.0f;
    float t = 0.0f;
    qmi8658_read_temp(&s_imu, &t);
    return t;
}