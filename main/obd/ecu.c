/* ecu.c -- UDS DID 数据解析器 + 双队列调度
 *
 * 功率优先策略:
 *   - F228/F229 (电压/电流): 40ms 高频周期，功率 ~25Hz 刷新，尽可能实时
 *   - F252/F253 (电池温度): 10000ms 超慢速轮询，温度 ~0.1Hz 刷新（非关键）
 *   - 温度队列优先于功率队列，防止被饿死（安全冗余）
 */
#include "ecu.h"
#include "elm.h"
#include "obd.h"
#include "ble.h"
#include "esp_log.h"
#include "lvgl.h"
#include <ctype.h>
#include <stdio.h>

static const char *TAG = "ECU";

/* ---------- 功率队列: 100ms 高频 - 尽可能实时 ---------- */
#define PWR_PERIOD_MS   100    /* 从40ms增加到100ms，降低轮询频率，减少OBD设备压力 */
#define PWR_LEN         2
static const struct {
    uint16_t did;
    const char *addr;
} s_pwr[PWR_LEN] = {
    {0xF228, "7A1"},  /* BMS 总电压 */
    {0xF229, "7A1"},  /* BMS 电池总电流 */
};
static int      s_pwr_idx = 0;
static uint32_t s_pwr_last = 0;

/* ---------- 温度队列: 2000ms 中速轮询 - 非关键但需要定期更新 ---------- */
#define TEMP_PERIOD_MS  2000   /* 从10000ms减少到2000ms，加快数据显示 */
#define TEMP_LEN        2
static const struct {
    uint16_t did;
    const char *addr;
} s_temp[TEMP_LEN] = {
    {0xF252, "7A1"},  /* 最高单体温度 */
    {0xF253, "7A1"},  /* 最低单体温度 */
};
static int      s_temp_idx = 0;
static uint32_t s_temp_last = 0;

/* ---------- 统一 pending 控制 ---------- */
static uint16_t s_pending = 0;
static uint32_t s_pending_since = 0;
#define PENDING_TIMEOUT_MS  500

void ecu_init(void)
{
    s_pwr_idx = 0;
    s_pwr_last = 0;
    s_temp_idx = 0;
    s_temp_last = 0;
    s_pending = 0;
}

void ecu_enter_pid(void)
{
    ESP_LOGI(TAG, "enter PID");
}

/* ecu_get_next_did: 双队列调度 — 温度优先防止饿死，功率紧随其后 */
bool ecu_get_next_did(uint16_t *did, const char **addr)
{
    if (!ble_is_connected()) return false;

    uint32_t now = lv_tick_get();

    /* pending 超时检查 */
    if (s_pending != 0) {
        if ((now - s_pending_since) > PENDING_TIMEOUT_MS) {
            ESP_LOGW(TAG, "pending 0x%04X timeout", s_pending);
            ecu_clear_pending();
        } else {
            return false;
        }
    }

    /* 温度队列优先: 5000ms 周期（必须先检查，防止被功率队列饿死） */
    if (s_temp_idx < TEMP_LEN && (now - s_temp_last) >= TEMP_PERIOD_MS) {
        *did  = s_temp[s_temp_idx].did;
        *addr = s_temp[s_temp_idx].addr;
        s_temp_idx++;
        if (s_temp_idx >= TEMP_LEN) {
            s_temp_idx = 0;
            s_temp_last = now;
        }
        return true;
    }

    /* 功率队列: 70ms 周期 */
    if (s_pwr_idx < PWR_LEN && (now - s_pwr_last) >= PWR_PERIOD_MS) {
        *did  = s_pwr[s_pwr_idx].did;
        *addr = s_pwr[s_pwr_idx].addr;
        s_pwr_idx++;
        if (s_pwr_idx >= PWR_LEN) {
            s_pwr_idx = 0;
            s_pwr_last = now;
        }
        return true;
    }

    return false;
}

/* ecu_peek_next_did: 偷看下一个 DID（不消耗队列） */
bool ecu_peek_next_did(uint16_t *did, const char **addr)
{
    int saved_pwr_idx = s_pwr_idx;
    uint32_t saved_pwr_last = s_pwr_last;
    int saved_temp_idx = s_temp_idx;
    uint32_t saved_temp_last = s_temp_last;

    bool ok = ecu_get_next_did(did, addr);

    s_pwr_idx = saved_pwr_idx;
    s_pwr_last = saved_pwr_last;
    s_temp_idx = saved_temp_idx;
    s_temp_last = saved_temp_last;

    return ok;
}

void ecu_set_pending(uint16_t did)
{
    s_pending = did;
    s_pending_since = lv_tick_get();
}

void ecu_clear_pending(void)
{
    s_pending = 0;
}

/* 解析 UDS 正响应（62 XX XX ...） */
void ecu_parse_uds(const char *resp)
{
    uint8_t d[20];
    int n = 0;
    const char *p = resp;

    while (*p && n < 20) {
        if (isxdigit((unsigned char)*p) && p[1] && isxdigit((unsigned char)p[1])) {
            char tmp[3] = {p[0], p[1], 0};
            d[n++] = (uint8_t)strtol(tmp, NULL, 16);
            p += 2; continue;
        }
        p++;
    }

    /* 跳过 CAN ID 头部找 62 */
    int off = 0;
    while (off < n && d[off] != 0x62) off++;
    if (off + 3 >= n) return;

    uint16_t did = ((uint16_t)d[off + 1] << 8) | d[off + 2];
    uint8_t *v = &d[off + 3];
    int vn = n - off - 3;

    obd_data_t *data = obd_get_data_rw();

    switch (did) {
    case 0xF228:
        if (vn >= 2) {
            uint16_t raw = ((uint16_t)v[0] << 8) | v[1];
            data->bms_total_v = raw / 10.0f;
            ESP_LOGI(TAG, "BMS V=%.1fV", data->bms_total_v);
        }
        break;
    case 0xF229:
        if (vn >= 2) {
            int16_t raw = (int16_t)(((uint16_t)v[0] << 8) | v[1]);
            data->bms_total_current = (raw - 6000) / 10.0f;
            ESP_LOGI(TAG, "BMS I=%.1fA raw=%d", data->bms_total_current, (int)raw);
        }
        break;
    case 0xF250:
        if (vn >= 2) {
            data->bms_max_cell = (((uint16_t)v[0] << 8) | v[1]) / 1000.0f;
        }
        break;
    case 0xF251:
        if (vn >= 2) {
            data->bms_min_cell = (((uint16_t)v[0] << 8) | v[1]) / 1000.0f;
        }
        break;
    case 0xF252:
        if (vn >= 1) {
            data->bms_max_temp = (int)v[0] - 40;  /* 文档公式: u8 - 40 = °C */
            ESP_LOGI(TAG, "BMS Tmax=%d", data->bms_max_temp);
        }
        break;
    case 0xF253:
        if (vn >= 1) {
            data->bms_min_temp = (int)v[0] - 40;  /* 文档公式: u8 - 40 = °C */
            ESP_LOGI(TAG, "BMS Tmin=%d", data->bms_min_temp);
        }
        break;
    case 0x0808:
        if (vn >= 2) data->sgcm_rpm = ((uint16_t)v[0] << 8) | v[1];
        break;
    case 0x0809:
        if (vn >= 2) {
            data->sgcm_current = (int16_t)(((uint16_t)v[0] << 8) | v[1]) / 10.0f;
            ESP_LOGI(TAG, "SGCM I=%.1fA", data->sgcm_current);
        }
        break;
    case 0x080A:
        if (vn >= 2) {
            data->sgcm_current_b = (int16_t)(((uint16_t)v[0] << 8) | v[1]) / 10.0f;
        }
        break;
    case 0x0812:
        if (vn >= 2) data->sgcm_rated = ((uint16_t)v[0] << 8) | v[1];
        break;
    }

    ecu_clear_pending();
}