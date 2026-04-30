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

/* 低通滤波器状态：在线估计重力参考向量 */
static float s_lp_x = 0.0f;
static float s_lp_y = 0.0f;
static float s_lp_z = 0.0f;
static bool s_lp_ready = false;

/* 低通滤波系数 (alpha=0.99 @60Hz, 时间常数约1.6秒) */
#define LP_ALPHA  0.99f

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

    /* 预热低通滤波器：读取多次让重力参考收敛 */
    float x, y, z;
    for (int i = 0; i < 30; i++) {
        hud_imu_get_accel(&x, &y, &z);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    ESP_LOGI(TAG, "LP filter warmed up: ref=(%.3f, %.3f, %.3f)", s_lp_x, s_lp_y, s_lp_z);
}

void hud_imu_update(void) { }

void hud_imu_calibrate(void)
{
    if (!s_imu_inited) return;
    /* 重置低通滤波器，从当前姿态重新开始跟踪重力 */
    s_lp_ready = false;
    ESP_LOGI(TAG, "LP filter reset");
}

bool hud_imu_get_accel(float *ax, float *ay, float *az)
{
    if (!s_imu_inited) return false;

    bool ready = false;
    if (qmi8658_is_data_ready(&s_imu, &ready) != ESP_OK || !ready) return false;

    qmi8658_data_t data;
    if (qmi8658_read_sensor_data(&s_imu, &data) != ESP_OK) return false;

    float raw_ax = data.accelX;
    float raw_ay = data.accelY;
    float raw_az = data.accelZ;

    if (!s_lp_ready) {
        /* 第一次：用当前读数初始化低通滤波器 */
        s_lp_x = raw_ax;
        s_lp_y = raw_ay;
        s_lp_z = raw_az;
        s_lp_ready = true;
    } else {
        /* 低通滤波：跟踪重力（慢变分量） */
        s_lp_x = LP_ALPHA * s_lp_x + (1.0f - LP_ALPHA) * raw_ax;
        s_lp_y = LP_ALPHA * s_lp_y + (1.0f - LP_ALPHA) * raw_ay;
        s_lp_z = LP_ALPHA * s_lp_z + (1.0f - LP_ALPHA) * raw_az;
    }

    /* 动态加速度 = 原始读数 - 低通重力估计（高通滤波输出） */
    *ax = raw_ax - s_lp_x;
    *ay = raw_ay - s_lp_y;
    *az = raw_az - s_lp_z;
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