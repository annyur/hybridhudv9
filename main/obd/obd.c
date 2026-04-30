/* obd.c — OBD-II PID 轮询（参考 Car Scanner 日志优化） */
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

/* 状态机 */
typedef enum {
    ST_IDLE = 0,
    ST_INIT_ATZ,       /* 复位 ELM327（Car Scanner 先 ATZ） */
    ST_INIT_ATE0,
    ST_INIT_ATL1,
    ST_INIT_ATS1,
    ST_INIT_ATH1,
    ST_INIT_ATSP6,     /* CAN 11bit 500k */
    ST_INIT_ATSH7E0,   /* HPCM 头部 */
    ST_INIT_ATS0,      /* 关闭空格 */
    /* PID 循环 */
    ST_PID_RPM, ST_PID_SPEED, ST_PID_COOLANT, ST_PID_VOLTAGE,
    ST_PID_THROTTLE, ST_PID_ODOMETER, ST_PID_SOC, ST_PID_DIST,
    ST_COUNT
} st_t;

static st_t s_st = ST_IDLE;
static uint32_t s_wait = 0;
static int s_retry = 0;
static int s_tx_fail = 0;
static bool s_atz_ready = false;   /* 收到 > 提示符 */

/* 平均功耗累计 */
static float    s_power_integral = 0.0f;  /* 功率对时间的积分 (kW·ms) */
static uint32_t s_energy_start_ms = 0;    /* OBD 启动时间戳 */
static uint32_t s_energy_last_ms = 0;     /* 上次累加时间戳 */

/* 收到响应后是否立即推进状态 */
static bool s_rx_trigger = false;

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
    char buf[32];
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

static void parse_std(const char *resp)
{
    const char *p = resp;

    /* 跳过紧凑 CAN 头部：7E8xx / 7E0xx / 7E4xx（3字符 ID + 2字符 DLC） */
    if ((strncmp(p, "7E8", 3) == 0 || strncmp(p, "7E0", 3) == 0 ||
         strncmp(p, "7E4", 3) == 0) && strlen(p) >= 5) {
        p += 5; /* 跳过 ID + DLC */
    }

    uint8_t d[24];
    int n = parse_hex(p, d, sizeof(d));
    if (n < 2) return;

    uint8_t mode = d[0];
    uint8_t pid  = d[1];
    if (mode != 0x41) return;

    switch (pid) {
    case 0x0C:  if (n >= 4) { s_data.rpm = ((d[2]*256)+d[3])/4; } break;
    case 0x0D:  if (n >= 3) { s_data.speed = (int)(d[2] * 1.075f + 0.5f); } break;
    case 0x05:  if (n >= 3) { s_data.coolant = d[2]-40; } break;
    case 0x42:  if (n >= 4) { s_data.batt_v = ((d[2]*256)+d[3])*0.001f; } break;
    case 0x11:  if (n >= 3) { s_data.throttle = d[2]*100/255; } break;
    case 0xA6:  if (n >= 6) { s_data.odometer = ((d[2]*16777216.0f)+(d[3]*65536.0f)+(d[4]*256.0f)+d[5])/10.0f; } break;
    case 0x5B:  if (n >= 3) { s_data.epa_soc = d[2]*100.0f/255.0f; } break;
    case 0x31:  if (n >= 4) { s_data.dist = (d[2]*256)+d[3]; } break;
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
        /* 设备进入 STOPPED 状态，需要 ATZ 重新初始化 */
        ESP_LOGW(TAG, "device STOPPED, reset to ATZ");
        s_st = ST_INIT_ATZ;
        s_wait = 0;
        s_atz_ready = false;
        return;
    }
    if (strstr(resp, "NO DATA") || strstr(resp, "ERROR") || strstr(resp, "UNABLE") || strstr(resp, "?")) {
        s_rx_trigger = true;  /* 错误也推进，避免卡死 */
        return;
    }
    if (is_prompt(resp)) {
        s_atz_ready = true;
        s_rx_trigger = true;  /* > 提示符 = 设备就绪，推进 */
        return;
    }
    if (is_ok(resp)) {
        /* 只有初始化命令靠 OK 推进；PID 阶段等 > 或数据 */
        if (s_st < ST_PID_RPM) {
            s_rx_trigger = true;
        }
        /* PID 阶段收到 OK 不推进 */
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
        return;  /* 搜索中，不推进 */
    }
    /* PID 数据：独立解析，不推进状态机（状态机靠 > 推进） */
    if (s_st >= ST_PID_RPM) {
        parse_std(resp);
    }
}

static void next(void) { s_st++; if (s_st >= ST_COUNT) s_st = ST_PID_RPM; }

static const char *cmd_of(st_t st) {
    switch (st) {
    case ST_INIT_ATZ:      return "ATZ";
    case ST_INIT_ATE0:     return "ATE0";
    case ST_INIT_ATL1:     return "ATL1";
    case ST_INIT_ATS1:     return "ATS1";
    case ST_INIT_ATH1:     return "ATH1";
    case ST_INIT_ATSP6:    return "ATSP6";
    case ST_INIT_ATSH7E0:  return "ATSH7E0";
    case ST_INIT_ATS0:     return "ATS0";
    case ST_PID_RPM:       return "010C";
    case ST_PID_SPEED:     return "010D";
    case ST_PID_COOLANT:   return "0105";
    case ST_PID_VOLTAGE:   return "0142";
    case ST_PID_THROTTLE:  return "0111";
    case ST_PID_ODOMETER:  return "01A6";
    case ST_PID_SOC:       return "015B";
    case ST_PID_DIST:      return "0131";
    default: return "";
    }
}

static int tof(st_t st)
{
    if (st == ST_INIT_ATZ) return 2000;      /* ATZ 复位 */
    if (st <= ST_INIT_ATSH7E0) return 500;  /* 初始化命令 */
    if (st == ST_PID_SPEED) return 100;      /* 车速高频 100ms */
    return 1000;                              /* 其他 PID 1 秒 */
}

/* ========== 公共接口 ========== */
void obd_init(void)  { memset(&s_data,0,sizeof(s_data)); s_running=false; s_st=ST_IDLE; s_atz_ready=false; s_rx_trigger=false; elm_init(); }
void obd_start(void) { if(s_running)return; s_running=true; s_st=ST_INIT_ATZ; s_retry=0; s_wait=0; s_tx_fail=0; s_atz_ready=false; s_rx_trigger=false; s_power_integral=0.0f; s_energy_start_ms=lv_tick_get(); s_energy_last_ms=s_energy_start_ms; ESP_LOGI(TAG,"start"); }
void obd_stop(void)  { s_running=false; s_st=ST_IDLE; ESP_LOGI(TAG,"stop"); }
bool obd_is_running(void) { return s_running; }

void obd_update(void)
{
    if (!s_running) return;

    elm_poll(0);
    while (elm_rx_ready()) {
        handle_rx(elm_rx_buf());
    }

    uint32_t now = lv_tick_get();

    /* 累加功率对时间的积分，计算自启动后的平均功耗 */
    if (s_energy_start_ms > 0 && s_energy_last_ms > 0) {
        uint32_t dt = now - s_energy_last_ms;
        if (dt > 0 && dt < 5000) {  /* 过滤异常跳变 */
            s_power_integral += s_data.power_kw * dt;  /* kW·ms */
            uint32_t total_ms = now - s_energy_start_ms;
            if (total_ms > 0) {
                s_data.avg_kw = s_power_integral / (float)total_ms;  /* kW·ms / ms = kW */
            }
        }
        s_energy_last_ms = now;
    }

    /* 收到任何响应后立即推进状态（不等超时），但每轮只推进一次 */
    if (s_rx_trigger && s_wait > 0) {
        s_rx_trigger = false;
        s_wait = 0;
        next();
    }

    /* 超时兜底 */
    if (s_wait > 0 && (now - s_wait) > (uint32_t)tof(s_st)) {
        s_wait = 0;
        if (s_st <= ST_INIT_ATSH7E0) {
            if (++s_retry > 5) { s_running = false; ESP_LOGW(TAG, "init timeout, stop"); return; }
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
        const char *c = cmd_of(s_st);
        if (c[0]) {
            if (tx(c)) s_wait = now;
            else s_wait = now;
        } else {
            next();
        }
    }
}

const obd_data_t *obd_get_data(void) { return &s_data; }