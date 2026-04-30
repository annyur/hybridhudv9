/* imu.c — QMI8658 IMU */
#include "imu.h"
#include "board.h"
#include "qmi8658.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "IMU";
static qmi8658_dev_t s_imu;
static bool s_imu_inited = false;

/* 零点偏置：设备启动时以当前静止状态为基准 */
static float s_ax0 = 0.0f;
static float s_ay0 = 0.0f;
static float s_az0 = 0.0f;

/* 采集零点 */
static void capture_zero(void)
{
    float sum_x = 0.0f, sum_y = 0.0f, sum_z = 0.0f;
    int valid = 0;

    for (int i = 0; i < 30; i++) {
        bool ready = false;
        if (qmi8658_is_data_ready(&s_imu, &ready) != ESP_OK || !ready) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        qmi8658_data_t data;
        if (qmi8658_read_sensor_data(&s_imu, &data) == ESP_OK) {
            sum_x += data.accelX;
            sum_y += data.accelY;
            sum_z += data.accelZ;
            valid++;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    if (valid > 0) {
        s_ax0 = sum_x / valid;
        s_ay0 = sum_y / valid;
        s_az0 = sum_z / valid;
        ESP_LOGI(TAG, "zero captured: (%.3f, %.3f, %.3f) from %d samples", s_ax0, s_ay0, s_az0, valid);
    } else {
        ESP_LOGW(TAG, "zero capture failed");
    }
}

void hud_imu_init(void)
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
    qmi8658_write_register(&s_imu, QMI8658_CTRL5, 0x03);

    s_imu_inited = true;
    ESP_LOGI(TAG, "QMI8658 initialized");

    /* 启动时采集零点：以当前状态为基准 */
    capture_zero();
}

void hud_imu_update(void) { }

void hud_imu_calibrate(void)
{
    /* 重新采集零点（比如重新安装设备后，在静止状态下调用） */
    if (!s_imu_inited) return;
    capture_zero();
}

bool hud_imu_get_accel(float *ax, float *ay, float *az)
{
    if (!s_imu_inited) return false;

    bool ready = false;
    if (qmi8658_is_data_ready(&s_imu, &ready) != ESP_OK || !ready) return false;

    qmi8658_data_t data;
    if (qmi8658_read_sensor_data(&s_imu, &data) != ESP_OK) return false;

    /* 减去零点，只保留动态变化 */
    *ax = data.accelX - s_ax0;
    *ay = data.accelY - s_ay0;
    *az = data.accelZ - s_az0;
    return true;
}

bool hud_imu_get_gyro(float *gx, float *gy, float *gz)
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

float hud_imu_get_temp(void)
{
    if (!s_imu_inited) return 0.0f;

    bool ready = false;
    if (qmi8658_is_data_ready(&s_imu, &ready) != ESP_OK || !ready) return 0.0f;

    qmi8658_data_t data;
    if (qmi8658_read_sensor_data(&s_imu, &data) != ESP_OK) return 0.0f;

    return data.temperature;
}