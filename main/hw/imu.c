/* imu.c — QMI8658 IMU */
#include "imu.h"
#include "board.h"
#include "qmi8658.h"
#include "esp_log.h"

static const char *TAG = "IMU";
static qmi8658_dev_t s_imu;
static bool s_imu_inited = false;

void imu_init(void)
{
    i2c_master_bus_handle_t bus_handle = bsp_i2c_get_handle();
    esp_err_t ret = qmi8658_init(&s_imu, bus_handle, QMI8658_ADDRESS_HIGH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "QMI8658 init failed: %d", ret);
        return;
    }

    qmi8658_set_accel_range(&s_imu, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(&s_imu, QMI8658_ACCEL_ODR_1000HZ);
    qmi8658_set_accel_unit_mps2(&s_imu, true);
    qmi8658_write_register(&s_imu, QMI8658_CTRL5, 0x03);  /* 启用 accel + gyro */

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

    bool ready = false;
    if (qmi8658_is_data_ready(&s_imu, &ready) != ESP_OK || !ready) return false;

    qmi8658_data_t data;
    if (qmi8658_read_sensor_data(&s_imu, &data) != ESP_OK) return false;

    *ax = data.accelX;
    *ay = data.accelY;
    *az = data.accelZ;
    return true;
}

bool imu_get_gyro(float *gx, float *gy, float *gz)
{
    if (!s_imu_inited) return false;

    bool ready = false;
    if (qmi8658_is_data_ready(&s_imu, &ready) != ESP_OK || !ready) return false;

    qmi8658_data_t data;
    if (qmi8658_read_sensor_data(&s_imu, &data) != ESP_OK) return false;

    *gx = data.gyroX;
    *gy = data.gyroY;
    *gz = data.gyroZ;
    return true;
}

float imu_get_temp(void)
{
    if (!s_imu_inited) return 0.0f;

    bool ready = false;
    if (qmi8658_is_data_ready(&s_imu, &ready) != ESP_OK || !ready) return 0.0f;

    qmi8658_data_t data;
    if (qmi8658_read_sensor_data(&s_imu, &data) != ESP_OK) return 0.0f;

    return data.temperature;
}