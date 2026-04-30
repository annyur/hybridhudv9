/* obd.c — OBD-II PID 轮询（Car Scanner 多 PID 复合请求模式） */
#include "obd.h"
#include "elm.h"
#include "ecu.h"
#include "ble.h"
#include "esp_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static const char *TAG = "OBD";
static obd_data_t s_data = {0};
static bool s_running = false;

/* ========== 状态机定义 ========== */
typedef enum {
    ST_IDLE = 0,
    ST_INIT_ATZ,
    ST_INIT_ATE0,
    ST_INIT_ATL1,
    ST_INIT_ATS1,
    ST_INIT_ATH1,
    ST_INIT_ATSP6,
    ST_INIT_ATSH7DF,   /* 功能地址（所有 ECU 同时响应） */
    ST_INIT_ATS0,
    ST_INIT_ATST64,    /* 设置超时 64×4ms=256ms */
    ST_PID_BATCH,      /* 多 PID 复合请求 */
    ST_COUNT
} st_t;

static st_t s_st = ST_IDLE;
static uint32_t s_wait = 0;
static int s_retry = 0;
static int s_tx_fail = 0;
static bool s_atz_ready = false;

/* 平均功耗累计 */
static float    s_power_integral = 0.0f;
static uint32_t s_energy_start_ms = 0;
static uint32_t s_energy_last_ms = 0;

/* 收到响应后是否立即推进 */
static bool s_rx_trigger = false;

/* ========== 多 PID 复合请求配置 ========== */
/* Car Scanner 格式：01<pid1><pid2><pid3>... 一次请求最多 6 个 PID */
#define COMPOSITE_PID_COUNT 6
static const uint8_t s_composite_pids[COMPOSITE_PID_COUNT] = {
    0x0C,   /* RPM */
    0x0D,   /* Speed */
    0x05,   /* Coolant */
    0x42,   /* Voltage */
    0x11,   /* Throttle */
    0x0E,   /* 占位：点火提前角（未实装但凑够 6 个 PID 提高 CAN 帧利用率） */
};

/* 解析 HEX */
static int parse_hex(const char *resp, uint8_t *out, int max)
{
    int n = 0;
    const char *p = resp;
    while (*p && n < max) {
        if ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f')) {
            if (p[1] && ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))) {
                char tmp[3] = {p[0], p[1], 0};
                out[n++] = (uint8_t)strtol(tmp, NULL, 16);
                p += 2; continue;
            }
        }
        p++;
    }
    return n;
}

static bool tx(const char *cmd)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s\r", cmd);
    bool ok = elm_tx(buf);
    if (ok) {
        ESP_LOGI(TAG, "TX: %s", cmd);
        s_tx_fail = 0;
    } else {
        s_tx_fail++;
        if (s_tx_fail <= 3) ESP_LOGW(TAG, "TX fail %d: %s", s_tx_fail, cmd);
    }
    return ok;
}

/* ========== 多 PID 解析（单行可能包含多个 PID） ========== */
static void parse_multi_pid(const char *resp)
{
    const char *p = resp;

    /* 跳过 CAN 头部：7E8xx / 7E0xx / 7EFxx / 7ECxx（3 字符 ID + 2 字符 DLC） */
    if ((strncmp(p, "7E8", 3) == 0 || strncmp(p, "7E0", 3) == 0 ||
         strncmp(p, "7EF", 3) == 0 || strncmp(p, "7EC", 3) == 0 ||
         strncmp(p, "7EE", 3) == 0 || strncmp(p, "7ED", 3) == 0 ||
         strncmp(p, "7EB", 3) == 0 || strncmp(p, "788", 3) == 0 ||
         strncmp(p, "7A9", 3) == 0) && strlen(p) >= 5) {
        p += 5;
    }

    uint8_t d[32];
    int n = parse_hex(p, d, sizeof(d));
    if (n < 3) return;

    /* 在多帧数据中提取所有 0x41 响应 */
    for (int i = 0; i < n - 1; i++) {
        if (d[i] == 0x41 && d[i+1] <= 0xA6) {
            uint8_t pid = d[i+1];
            int data_len = 0;
            /* 根据 PID 确定数据长度 */
            switch (pid) {
            case 0x0C: data_len = 2; break;
            case 0x0D: data_len = 1; break;
            case 0x05: data_len = 1; break;
            case 0x42: data_len = 2; break;
            case 0x11: data_len = 1; break;
            case 0x0E: data_len = 1; break;
            default:   data_len = 1; break;
            }

            if (i + 2 + data_len > n) continue;

            switch (pid) {
            case 0x0C: {
                int raw = (d[i+2] << 8) | d[i+3];
                s_data.rpm = raw / 4;
                break;
            }
            case 0x0D: {
                s_data.speed = (int)(d[i+2] * 1.075f + 0.5f);
                break;
            }
            case 0x05: {
                s_data.coolant = d[i+2] - 40;
                break;
            }
            case 0x42: {
                int raw = (d[i+2] << 8) | d[i+3];
                s_data.batt_v = raw * 0.001f;
                break;
            }
            case 0x11: {
                s_data.throttle = d[i+2] * 100 / 255;
                break;
            }
            }
            /* 跳到下一个 PID */
            i += 1 + data_len;
        }
    }
}

static bool is_prompt(const char *resp)
{
    return (resp[0] == '>' && resp[1] == '\0');
}

static bool is_ok(const char *resp)
{
    return (strcasecmp(resp, "OK") == 0);
}

static void handle_rx(const char *resp)
{
    ESP_LOGI(TAG, "RX[%d]: '%s'", s_st, resp);

    if (strstr(resp, "STOPPED")) {
        ESP_LOGW(TAG, "device STOPPED, reset to ATZ");
        s_st = ST_INIT_ATZ;
        s_wait = 0;
        s_atz_ready = false;
        return;
    }

    if (strstr(resp, "NO DATA") || strstr(resp, "ERROR") || strstr(resp, "UNABLE") || strstr(resp, "?")) {
        s_rx_trigger = true;
        return;
    }

    if (is_prompt(resp)) {
        s_atz_ready = true;
        s_rx_trigger = true;
        return;
    }

    if (is_ok(resp)) {
        if (s_st < ST_PID_BATCH) {
            s_rx_trigger = true;
        }
        return;
    }

    if (strstr(resp, "ELM327")) {
        if (s_st == ST_INIT_ATZ) {
            s_atz_ready = true;
        }
        s_rx_trigger = true;
        return;
    }

    if (strstr(resp, "SEARCHING")) {
        return;
    }

    /* PID 数据：解析单行中可能包含的多个 PID */
    if (s_st >= ST_PID_BATCH) {
        parse_multi_pid(resp);
    }
}

static void next(void)
{
    s_st++;
    if (s_st >= ST_COUNT) {
        s_st = ST_PID_BATCH;
    }
}

static const char *cmd_of(st_t st)
{
    switch (st) {
    case ST_INIT_ATZ:      return "ATZ";
    case ST_INIT_ATE0:     return "ATE0";
    case ST_INIT_ATL1:     return "ATL1";
    case ST_INIT_ATS1:     return "ATS1";
    case ST_INIT_ATH1:     return "ATH1";
    case ST_INIT_ATSP6:    return "ATSP6";
    case ST_INIT_ATSH7DF:  return "ATSH7DF";
    case ST_INIT_ATS0:     return "ATS0";
    case ST_INIT_ATST64:   return "ATST64";
    default: return "";
    }
}

/* ========== 构造多 PID 复合请求 ========== */
static bool tx_composite(void)
{
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "01");
    for (int i = 0; i < COMPOSITE_PID_COUNT; i++) {
        len += snprintf(buf + len, sizeof(buf) - len, "%02X", s_composite_pids[i]);
    }
    /* 添加 \r */
    snprintf(buf + len, sizeof(buf) - len, "\r");

    bool ok = elm_tx(buf);
    if (ok) {
        ESP_LOGI(TAG, "TX COMPOSITE: %s", buf);
        s_tx_fail = 0;
    } else {
        s_tx_fail++;
        if (s_tx_fail <= 3) ESP_LOGW(TAG, "TX composite fail %d", s_tx_fail);
    }
    return ok;
}

static int tof(st_t st)
{
    if (st == ST_INIT_ATZ) return 2000;
    if (st <= ST_INIT_ATST64) return 500;
    /* 复合请求超时：等待所有 ECU 响应 */
    return 800;
}

/* ========== 公共接口 ========== */
void obd_init(void)
{
    memset(&s_data, 0, sizeof(s_data));
    s_running = false;
    s_st = ST_IDLE;
    s_atz_ready = false;
    s_rx_trigger = false;
    elm_init();
}

void obd_start(void)
{
    if (s_running) return;
    s_running = true;
    s_st = ST_INIT_ATZ;
    s_retry = 0;
    s_wait = 0;
    s_tx_fail = 0;
    s_atz_ready = false;
    s_rx_trigger = false;
    s_power_integral = 0.0f;
    s_energy_start_ms = lv_tick_get();
    s_energy_last_ms = s_energy_start_ms;
    ESP_LOGI(TAG, "start (Car Scanner composite mode)");
}

void obd_stop(void)
{
    s_running = false;
    s_st = ST_IDLE;
    ESP_LOGI(TAG, "stop");
}

bool obd_is_running(void) { return s_running; }

void obd_update(void)
{
    if (!s_running) return;

    elm_poll(0);
    while (elm_rx_ready()) {
        handle_rx(elm_rx_buf());
    }

    uint32_t now = lv_tick_get();

    /* 累加功率对时间的积分 */
    if (s_energy_start_ms > 0 && s_energy_last_ms > 0) {
        uint32_t dt = now - s_energy_last_ms;
        if (dt > 0 && dt < 5000) {
            s_power_integral += s_data.power_kw * dt;
            uint32_t total_ms = now - s_energy_start_ms;
            if (total_ms > 0) {
                s_data.avg_kw = s_power_integral / (float)total_ms;
            }
        }
        s_energy_last_ms = now;
    }

    /* 收到 > 提示符后推进 */
    if (s_rx_trigger && s_wait > 0) {
        s_rx_trigger = false;
        s_wait = 0;
        next();
    }

    /* 超时兜底 */
    if (s_wait > 0 && (now - s_wait) > (uint32_t)tof(s_st)) {
        s_wait = 0;
        if (s_st <= ST_INIT_ATST64) {
            if (++s_retry > 5) {
                s_running = false;
                ESP_LOGW(TAG, "init timeout, stop");
                return;
            }
            ESP_LOGW(TAG, "timeout retry %d, reset to ATZ", s_retry);
            s_st = ST_INIT_ATZ;
            s_atz_ready = false;
        } else {
            next();
        }
    }

    /* 写失败暂停 */
    if (s_tx_fail >= 5 && (now - s_wait) < 500) return;
    if (s_tx_fail >= 5) s_tx_fail = 0;

    if (s_wait == 0) {
        if (s_st == ST_PID_BATCH) {
            if (tx_composite()) s_wait = now;
            else s_wait = now;
        } else {
            const char *c = cmd_of(s_st);
            if (c[0]) {
                if (tx(c)) s_wait = now;
                else s_wait = now;
            } else {
                next();
            }
        }
    }
}

const obd_data_t *obd_get_data(void) { return &s_data; }