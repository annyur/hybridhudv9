/* obd.c — OBD-II PID 轮询 + 数据解析 */
#include "obd.h"
#include "elm.h"
#include "ble.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "OBD";

static obd_data_t s_data = {0};
static bool s_running = false;

/* ===== 状态机 ===== */
typedef enum {
    OBD_IDLE = 0,
    OBD_INIT_ATZ,       /* ATZ 复位 */
    OBD_INIT_ATE0,      /* ATE0 关闭回显 */
    OBD_INIT_ATL1,      /* ATL1 长消息 */
    OBD_INIT_ATSP0,     /* ATSP0 自动协议 */
    OBD_REQ_RPM,        /* 010C 转速 */
    OBD_REQ_SPEED,      /* 010D 车速 */
    OBD_REQ_COOLANT,    /* 0105 冷却液温度 */
    OBD_REQ_OIL,        /* 015C 机油温度 */
    OBD_REQ_SOC,        /* 015B 电池 SOC */
    OBD_REQ_VOLTAGE,    /* 0142 控制模块电压 */
    OBD_REQ_THROTTLE,   /* 0111 节气门位置 */
    OBD_REQ_POWER,      /* 通过电流×电压估算 */
    OBD_COUNT
} obd_state_t;

static obd_state_t s_state = OBD_IDLE;
static uint32_t s_last_ms = 0;
static uint32_t s_rx_wait = 0;
static int s_init_retry = 0;

/* 各 PID 的命令和响应超时(ms) */
#define CMD_TIMEOUT     500
#define INIT_TIMEOUT    1000
#define POLL_INTERVAL   200   /* 每个 PID 间隔 200ms */

/* ===== 工具：发送命令（带\r） ===== */
static bool send_cmd(const char *cmd)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%s\r", cmd);
    ESP_LOGI(TAG, "TX: %s", cmd);
    return elm_tx(buf);
}

/* ===== 工具：解析十六进制字节 ===== */
static int hex_byte(const char *p)
{
    char tmp[3] = {p[0], p[1], 0};
    return (int)strtol(tmp, NULL, 16);
}

/* ===== 工具：从响应字符串提取十六进制数据 ===== */
static int parse_response(const char *resp, uint8_t *out, int max_len)
{
    int cnt = 0;
    const char *p = resp;
    while (*p && cnt < max_len) {
        /* 跳过空格、>、回车等非十六进制字符 */
        if ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f')) {
            if (p[1] && ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))) {
                out[cnt++] = (uint8_t)hex_byte(p);
                p += 2;
                continue;
            }
        }
        p++;
    }
    return cnt;
}

/* ===== 工具：检查响应是否包含 "NO DATA" / "ERROR" ===== */
static bool is_error_response(const char *resp)
{
    return (strstr(resp, "NO DATA") || strstr(resp, "ERROR") ||
            strstr(resp, "UNABLE") || strstr(resp, "?"));
}

/* ===== 解析各 PID ===== */
static void parse_pid_rpm(const uint8_t *d, int len)
{
    if (len >= 2 && d[0] == 0x41 && d[1] == 0x0C) {
        s_data.rpm = ((d[2] * 256) + d[3]) / 4;
        ESP_LOGI(TAG, "RPM=%d", s_data.rpm);
    }
}

static void parse_pid_speed(const uint8_t *d, int len)
{
    if (len >= 2 && d[0] == 0x41 && d[1] == 0x0D) {
        s_data.speed = d[2];
        ESP_LOGI(TAG, "Speed=%d", s_data.speed);
    }
}

static void parse_pid_coolant(const uint8_t *d, int len)
{
    if (len >= 2 && d[0] == 0x41 && d[1] == 0x05) {
        s_data.coolant = d[2] - 40;
        ESP_LOGI(TAG, "Coolant=%d", s_data.coolant);
    }
}

static void parse_pid_oil(const uint8_t *d, int len)
{
    if (len >= 2 && d[0] == 0x41 && d[1] == 0x5C) {
        s_data.oil = d[2] - 40;
        ESP_LOGI(TAG, "Oil=%d", s_data.oil);
    }
}

static void parse_pid_soc(const uint8_t *d, int len)
{
    if (len >= 2 && d[0] == 0x41 && d[1] == 0x5B) {
        s_data.epa_soc = d[2] / 2.55f;  /* 0~100% */
        ESP_LOGI(TAG, "SOC=%.1f", s_data.epa_soc);
    }
}

static void parse_pid_voltage(const uint8_t *d, int len)
{
    if (len >= 2 && d[0] == 0x41 && d[1] == 0x42) {
        s_data.batt_v = d[2] / 10.0f;
        ESP_LOGI(TAG, "Voltage=%.1f", s_data.batt_v);
    }
}

static void parse_pid_throttle(const uint8_t *d, int len)
{
    if (len >= 2 && d[0] == 0x41 && d[1] == 0x11) {
        s_data.throttle = d[2] * 100 / 255;
        ESP_LOGI(TAG, "Throttle=%d", s_data.throttle);
    }
}

/* ===== 状态处理 ===== */
static void handle_rx(const char *resp)
{
    ESP_LOGI(TAG, "RX: %s", resp);

    uint8_t data[16];
    int dlen = parse_response(resp, data, sizeof(data));

    if (is_error_response(resp)) {
        ESP_LOGW(TAG, "PID error, skip");
        return;
    }

    switch (s_state) {
    case OBD_INIT_ATZ:
    case OBD_INIT_ATE0:
    case OBD_INIT_ATL1:
    case OBD_INIT_ATSP0:
        /* 初始化命令只需确认收到响应 */
        break;
    case OBD_REQ_RPM:      parse_pid_rpm(data, dlen);      break;
    case OBD_REQ_SPEED:    parse_pid_speed(data, dlen);    break;
    case OBD_REQ_COOLANT:  parse_pid_coolant(data, dlen);  break;
    case OBD_REQ_OIL:      parse_pid_oil(data, dlen);      break;
    case OBD_REQ_SOC:      parse_pid_soc(data, dlen);      break;
    case OBD_REQ_VOLTAGE:  parse_pid_voltage(data, dlen);  break;
    case OBD_REQ_THROTTLE: parse_pid_throttle(data, dlen); break;
    default: break;
    }
}

static void next_state(void)
{
    s_state++;
    if (s_state >= OBD_COUNT) {
        s_state = OBD_REQ_RPM;  /* 循环 */
    }
}

static const char *state_cmd(obd_state_t st)
{
    switch (st) {
    case OBD_INIT_ATZ:     return "ATZ";
    case OBD_INIT_ATE0:    return "ATE0";
    case OBD_INIT_ATL1:    return "ATL1";
    case OBD_INIT_ATSP0:   return "ATSP0";
    case OBD_REQ_RPM:      return "010C";
    case OBD_REQ_SPEED:    return "010D";
    case OBD_REQ_COOLANT:  return "0105";
    case OBD_REQ_OIL:      return "015C";
    case OBD_REQ_SOC:      return "015B";
    case OBD_REQ_VOLTAGE:  return "0142";
    case OBD_REQ_THROTTLE: return "0111";
    default: return "";
    }
}

static int state_timeout(obd_state_t st)
{
    return (st <= OBD_INIT_ATSP0) ? INIT_TIMEOUT : CMD_TIMEOUT;
}

/* ========== 公共接口 ========== */

void obd_init(void)
{
    memset(&s_data, 0, sizeof(s_data));
    s_running = false;
    s_state = OBD_IDLE;
    elm_init();
    ESP_LOGI(TAG, "initialized");
}

void obd_start(void)
{
    if (s_running) return;
    s_running = true;
    s_state = OBD_INIT_ATZ;
    s_init_retry = 0;
    s_last_ms = 0;
    ESP_LOGI(TAG, "start");
}

void obd_stop(void)
{
    s_running = false;
    s_state = OBD_IDLE;
    ESP_LOGI(TAG, "stop");
}

void obd_update(void)
{
    if (!s_running) return;

    /* 先处理 ELM 接收 */
    elm_poll(0);
    if (elm_rx_ready()) {
        const char *resp = elm_rx_buf();
        handle_rx(resp);
        elm_rx_clear();
        s_rx_wait = 0;
        /* 收到响应，进入下一个 PID */
        next_state();
    }

    uint32_t now = lv_tick_get();

    /* 超时检测 */
    if (s_rx_wait > 0 && (now - s_rx_wait) > (uint32_t)state_timeout(s_state)) {
        ESP_LOGW(TAG, "timeout @ state %d", s_state);
        s_rx_wait = 0;
        if (s_state <= OBD_INIT_ATSP0) {
            /* 初始化超时，重试 */
            s_init_retry++;
            if (s_init_retry > 3) {
                ESP_LOGE(TAG, "init failed after 3 retries");
                s_running = false;
                return;
            }
            s_state = OBD_INIT_ATZ;
        } else {
            /* PID 超时，跳过 */
            next_state();
        }
    }

    /* 发送当前状态对应的命令 */
    if (s_rx_wait == 0 && (s_last_ms == 0 || (now - s_last_ms) >= POLL_INTERVAL)) {
        const char *cmd = state_cmd(s_state);
        if (cmd[0]) {
            if (send_cmd(cmd)) {
                s_rx_wait = now;
                s_last_ms = now;
            } else {
                ESP_LOGE(TAG, "send failed");
            }
        }
    }
}

const obd_data_t *obd_get_data(void)
{
    return &s_data;
}

/* 可选：通过 IMU 的油门估算功率（简易方案） */
void obd_set_power_from_accel(float kw)
{
    s_data.power_kw = kw;
}