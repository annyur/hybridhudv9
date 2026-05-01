/* obd.c — OBD-II PID 轮询 + ISO-TP 多帧拼接器 + 复合请求 */
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
    ST_INIT_ATH0,
    ST_INIT_ATSP6,
    ST_INIT_ATSH7E0,
    ST_INIT_ATS0,
    ST_INIT_ATSTFF,
    ST_PID_RUN,       /* 复合请求：010C010D（转速+车速） */
    ST_PID_COOLANT,   /* 0105 */
    ST_PID_VOLTAGE,   /* 0142 */
    ST_PID_ODOMETER,  /* 01A6 */
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

/* PID 阶段是否已收到有效数据响应 */
static bool s_got_data = false;

/* PID 阶段最小命令间隔（ms）：等 > 推进 */
#define PID_MIN_INTERVAL 200
/* init 阶段最小命令间隔（ms），防止命令堆积到设备缓冲 */
#define INIT_MIN_INTERVAL 150
static uint32_t s_last_tx_ms = 0;

/* 双速轮询：慢轮计数器 */
static int s_slow_tick = 0;
#define SLOW_EVERY 4

/* ========== ISO-TP 多帧拼接器 ========== */
#define TP_MAX_SLOTS 8
#define TP_MAX_DATA  32
#define TP_TIMEOUT_MS 2000

typedef struct {
    uint16_t can_id;
    bool     active;
    uint8_t  buf[TP_MAX_DATA];
    uint16_t total;       /* 期望总长度 */
    uint16_t received;    /* 已接收字节数 */
    uint8_t  next_seq;    /* 期望的下一个 CF 序号 */
    uint32_t start_ms;    /* FF 接收时间 */
} tp_slot_t;

static tp_slot_t s_tp_slots[TP_MAX_SLOTS];

static uint8_t hex_byte(const char *p)
{
    if (!p || !p[0] || !p[1]) return 0;
    char tmp[3] = {p[0], p[1], '\0'};
    return (uint8_t)strtol(tmp, NULL, 16);
}

static uint16_t extract_can_id(const char *line)
{
    if (strlen(line) < 7) return 0;
    if (line[0] != '7') return 0;
    if (!isxdigit((unsigned char)line[1]) || !isxdigit((unsigned char)line[2])) return 0;
    if (!isxdigit((unsigned char)line[3])) return 0;
    char id_str[5] = {line[0], line[1], line[2], '\0'};
    return (uint16_t)strtol(id_str, NULL, 16);
}

static int tp_find_slot(uint16_t can_id)
{
    int empty = -1, oldest = 0;
    for (int i = 0; i < TP_MAX_SLOTS; i++) {
        if (s_tp_slots[i].active) {
            if (s_tp_slots[i].can_id == can_id) return i;
            if (s_tp_slots[i].start_ms < s_tp_slots[oldest].start_ms) oldest = i;
        } else if (empty < 0) {
            empty = i;
        }
    }
    if (empty >= 0) {
        s_tp_slots[empty].can_id = can_id;
        return empty;
    }
    /* 驱逐最老的 */
    s_tp_slots[oldest].active = false;
    s_tp_slots[oldest].can_id = can_id;
    return oldest;
}

/* forward declaration: parse_std is defined later */
static void parse_std(const char *resp);

static void tp_parse_complete(tp_slot_t *slot)
{
    if (slot->received == 0) return;
    char hex_str[TP_MAX_DATA * 2 + 1];
    for (int i = 0; i < slot->received; i++) {
        snprintf(hex_str + i * 2, 3, "%02X", slot->buf[i]);
    }
    hex_str[slot->received * 2] = '\0';
    ESP_LOGI(TAG, "TP asm CAN=0x%03X len=%d data=%s", slot->can_id, slot->received, hex_str);
    /* 复用 parse_std 解析组装后的纯数据 */
    parse_std(hex_str);
}

static void iso_tp_feed(const char *resp, uint16_t can_id)
{
    int slot = tp_find_slot(can_id);
    const char *p = resp + 3; /* 跳过 CAN ID */
    uint8_t pci = hex_byte(p);
    uint8_t type = pci & 0xF0;

    if (type == 0x00) { /* Single Frame */
        uint8_t len = pci & 0x0F;
        if (len > 0 && len <= 7 && strlen(p) >= 2 + len * 2) {
            for (int i = 0; i < len; i++) {
                s_tp_slots[slot].buf[i] = hex_byte(p + 2 + i * 2);
            }
            s_tp_slots[slot].received = len;
            s_tp_slots[slot].total = len;
            s_tp_slots[slot].active = false;
            tp_parse_complete(&s_tp_slots[slot]);
        } else {
            s_tp_slots[slot].active = false;
        }
    } else if (type == 0x10) { /* First Frame */
        uint8_t len_low = hex_byte(p + 2);
        uint16_t total = ((pci & 0x0F) << 8) | len_low;
        if (total > 0 && total <= TP_MAX_DATA && strlen(p) >= 4 + 6 * 2) {
            s_tp_slots[slot].can_id = can_id;
            s_tp_slots[slot].total = total;
            s_tp_slots[slot].received = 6; /* FF 含 6 字节数据 */
            s_tp_slots[slot].next_seq = 1;
            s_tp_slots[slot].active = true;
            s_tp_slots[slot].start_ms = lv_tick_get();
            for (int i = 0; i < 6; i++) {
                s_tp_slots[slot].buf[i] = hex_byte(p + 4 + i * 2);
            }
        } else {
            s_tp_slots[slot].active = false;
        }
    } else if (type == 0x20) { /* Consecutive Frame */
        if (!s_tp_slots[slot].active) return;
        uint8_t seq = pci & 0x0F;
        if (seq != s_tp_slots[slot].next_seq) {
            ESP_LOGW(TAG, "TP seq err CAN=0x%03X want=%d got=%d", can_id,
                     s_tp_slots[slot].next_seq, seq);
            s_tp_slots[slot].active = false;
            return;
        }
        uint8_t remain = s_tp_slots[slot].total - s_tp_slots[slot].received;
        uint8_t copy = (remain < 7) ? remain : 7;
        if (strlen(p) >= 2 + copy * 2) {
            for (int i = 0; i < copy; i++) {
                s_tp_slots[slot].buf[s_tp_slots[slot].received + i] = hex_byte(p + 2 + i * 2);
            }
            s_tp_slots[slot].received += copy;
            s_tp_slots[slot].next_seq = (seq + 1) & 0x0F;
            if (s_tp_slots[slot].received >= s_tp_slots[slot].total) {
                tp_parse_complete(&s_tp_slots[slot]);
                s_tp_slots[slot].active = false;
            }
        }
    }
}

/* 清理超时的拼接槽 */
static void tp_timeout_check(uint32_t now)
{
    for (int i = 0; i < TP_MAX_SLOTS; i++) {
        if (s_tp_slots[i].active && (now - s_tp_slots[i].start_ms) > TP_TIMEOUT_MS) {
            ESP_LOGW(TAG, "TP timeout CAN=0x%03X", s_tp_slots[i].can_id);
            s_tp_slots[i].active = false;
        }
    }
}

/* 解析 HEX 字符串为字节数组 */
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
        s_last_tx_ms = lv_tick_get();
    } else {
        s_tx_fail++;
        if (s_tx_fail <= 3) ESP_LOGW(TAG, "TX fail %d: %s", s_tx_fail, cmd);
    }
    return ok;
}

/* ========== 核心增强：循环解析一行中的多个 PID ========== */
static void parse_std(const char *resp)
{
    const char *p = resp;

    /* 跳过紧凑 CAN 头部：7E8xx / 7E0xx / 7E4xx（3字符 ID + 2字符 DLC） */
    if ((strncmp(p, "7E8", 3) == 0 || strncmp(p, "7E0", 3) == 0 ||
         strncmp(p, "7E4", 3) == 0) && strlen(p) >= 5) {
        p += 5;
    }

    /* 解析整行所有 hex 字节 */
    uint8_t d[32];
    int n = parse_hex(p, d, sizeof(d));
    if (n < 3) return;

    /* 循环扫描所有 0x41 开头的 PID 响应段 */
    for (int i = 0; i < n - 2; i++) {
        if (d[i] != 0x41) continue;
        uint8_t pid = d[i + 1];
        int avail = n - i - 2; /* 该 PID 后面的可用数据字节数 */
        switch (pid) {
        case 0x0C:
            if (avail >= 2) { s_data.rpm = ((d[i + 2] * 256) + d[i + 3]) / 4; }
            break;
        case 0x0D:
            if (avail >= 1) {
                int raw = d[i + 2];
                /* 动态补偿：0km/h +9.5%，每10km/h线性减少0.5% */
                float comp = 1.095f - raw * 0.0005f;
                if (comp < 1.02f) comp = 1.02f;  /* 最低保底 +2% */
                s_data.speed = (int)(raw * comp + 0.5f);
            }
            break;
        case 0x05:
            if (avail >= 1) { s_data.coolant = d[i + 2] - 40; }
            break;
        case 0x42:
            if (avail >= 2) { s_data.batt_v = ((d[i + 2] * 256) + d[i + 3]) * 0.001f; }
            break;
        case 0x11:
            if (avail >= 1) { s_data.throttle = d[i + 2] * 100 / 255; }
            break;
        case 0xA6:
            if (avail >= 4) {
                s_data.odometer = ((d[i + 2] * 16777216.0f) + (d[i + 3] * 65536.0f)
                                   + (d[i + 4] * 256.0f) + d[i + 5]) / 10.0f;
            }
            break;
        case 0x5B:
            if (avail >= 1) { s_data.epa_soc = d[i + 2] * 100.0f / 255.0f; }
            break;
        case 0x31:
            if (avail >= 2) { s_data.dist = (d[i + 2] * 256) + d[i + 3]; }
            break;
        }
    }
}

/* ========== 宽松解析：不依赖 0x41 前缀，直接扫描 PID 码 ==========
   用于处理 ELM327 复合请求的多帧格式（如 1:07E5E50D000000 中 0D 无 41 前缀） */
static void parse_loose(const char *resp)
{
    uint8_t d[32];
    int n = parse_hex(resp, d, sizeof(d));
    if (n < 2) return;

    for (int i = 0; i < n - 1; i++) {
        switch (d[i]) {
        case 0x0C:
            if (i + 2 < n) s_data.rpm = ((d[i + 1] * 256) + d[i + 2]) / 4;
            break;
        case 0x0D:
            if (i + 1 < n) {
                int raw = d[i + 1];
                /* 动态补偿：0km/h +9.5%，每10km/h线性减少0.5% */
                float comp = 1.095f - raw * 0.0005f;
                if (comp < 1.02f) comp = 1.02f;  /* 最低保底 +2% */
                s_data.speed = (int)(raw * comp + 0.5f);
            }
            break;
        case 0x05:
            if (i + 1 < n) s_data.coolant = d[i + 1] - 40;
            break;
        case 0x42:
            if (i + 2 < n) s_data.batt_v = ((d[i + 1] * 256) + d[i + 2]) * 0.001f;
            break;
        case 0x11:
            if (i + 1 < n) s_data.throttle = d[i + 1] * 100 / 255;
            break;
        case 0xA6:
            if (i + 4 < n) {
                s_data.odometer = ((d[i + 1] * 16777216.0f) + (d[i + 2] * 65536.0f)
                                   + (d[i + 3] * 256.0f) + d[i + 4]) / 10.0f;
            }
            break;
        case 0x5B:
            if (i + 1 < n) s_data.epa_soc = d[i + 1] * 100.0f / 255.0f;
            break;
        case 0x31:
            if (i + 2 < n) s_data.dist = (d[i + 1] * 256) + d[i + 2];
            break;
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

/* 判断是否为当前命令的 echo */
static bool is_echo(const char *resp)
{
    switch (s_st) {
    case ST_PID_RUN:      return (strcmp(resp, "010C010D") == 0);
    case ST_PID_COOLANT:  return (strcmp(resp, "0105") == 0);
    case ST_PID_VOLTAGE:  return (strcmp(resp, "0142") == 0);
    case ST_PID_ODOMETER: return (strcmp(resp, "01A6") == 0);
    default: return false;
    }
}

static void handle_rx(const char *resp)
{
    /* 1. 纯数据响应（ATH0 模式，无 CAN 头部，如 410C1B50 / 410D00 / 410C1B50410D00） */
    if (resp[0] == '4' && (resp[1] == '1' || resp[1] == '2')) {
        parse_std(resp);
        s_got_data = true;
        /* 不设置 s_rx_trigger：等 > 到来再统一推进，防止设备堆积 */
        return;
    }

    /* 2. ELM327 复合请求多帧格式：00B / 0:410C0000 / 1:07E5E50D000000 */
    if (resp[0] == '0' && resp[1] == ':') {
        const char *payload = resp + 2;
        parse_std(payload);   /* 标准解析（处理 41xx） */
        parse_loose(payload);   /* 宽松解析（处理无 41 前缀的 PID） */
        s_got_data = true;
        return;
    }
    if (resp[0] == '1' && resp[1] == ':') {
        const char *payload = resp + 2;
        parse_loose(payload);   /* 第二帧通常无 41 前缀，只用宽松解析 */
        s_got_data = true;
        return;
    }
    /* 响应头如 "00B"（长度指示）直接忽略 */
    if (resp[0] == '0' && isxdigit((unsigned char)resp[1]) && isxdigit((unsigned char)resp[2]) && resp[3] == '\0') {
        return;
    }

    /* 3. ISO-TP CAN 帧检测与拼接（ATH1 模式） */
    uint16_t can_id = extract_can_id(resp);
    if (can_id != 0) {
        iso_tp_feed(resp, can_id);
        return;
    }

    ESP_LOGI(TAG, "RX[%d]: '%s'", s_st, resp);

    if (strstr(resp, "STOPPED")) {
        ESP_LOGW(TAG, "device STOPPED, reset to ATZ");
        s_st = ST_INIT_ATZ;
        s_wait = 0;
        s_atz_ready = false;
        s_got_data = false;
        return;
    }

    if (strstr(resp, "NO DATA") || strstr(resp, "ERROR") || strstr(resp, "UNABLE") || strstr(resp, "?")) {
        s_rx_trigger = true;
        return;
    }

    if (is_prompt(resp)) {
        s_atz_ready = true;
        if (s_st < ST_PID_RUN) {
            /* 初始化阶段：> 推进，但要求确实发过命令（防止堆积的延迟 > 空转） */
            if (s_wait > 0) {
                s_rx_trigger = true;
            }
        } else {
            /* PID 阶段：> 只在已收到数据后才推进；
               若没收到数据就收到 >，说明是之前堆积的，忽略 */
            if (s_got_data && s_wait > 0) {
                s_rx_trigger = true;
            }
        }
        return;
    }

    if (is_echo(resp)) {
        /* PID 命令的 echo，忽略，不推进状态机 */
        return;
    }

    if (is_ok(resp)) {
        /* 初始化阶段 OK 推进；PID 阶段不推进 */
        if (s_st < ST_PID_RUN) {
            s_rx_trigger = true;
        }
        return;
    }

    if (strstr(resp, "ELM327")) {
        if (s_st == ST_INIT_ATZ) {
            s_atz_ready = true;
        }
        /* 不推进：等 > 到来时再推进，防止 ATZ 后 30ms 就发下一个命令 */
        return;
    }

    if (strstr(resp, "SEARCHING")) {
        return;
    }
}

static void next(void)
{
    st_t old = s_st;
    s_st++;
    if (s_st >= ST_COUNT) {
        s_st = ST_PID_RUN;
    }
    /* init→PID 切换时清空 elm 缓冲区，丢弃堆积的残留响应 */
    if (old <= ST_INIT_ATSTFF && s_st >= ST_PID_RUN) {
        elm_rx_clear();
        s_got_data = false;
        s_rx_trigger = false;
        s_slow_tick = 0;
        ESP_LOGI(TAG, "enter PID, clear backlog");
    }
    /* 双速轮询：从 RUN 推进时，如果不是慢速轮次，跳回 RUN（只轮复合命令） */
    if (old == ST_PID_RUN && s_st == ST_PID_COOLANT) {
        s_slow_tick++;
        if (s_slow_tick < SLOW_EVERY) {
            s_st = ST_PID_RUN;
            ESP_LOGI(TAG, "fast round %d/%d, only RUN", s_slow_tick, SLOW_EVERY);
        } else {
            s_slow_tick = 0;
            ESP_LOGI(TAG, "slow round, all PIDs");
        }
    }
}

static const char *cmd_of(st_t st)
{
    switch (st) {
    case ST_INIT_ATZ:      return "ATZ";
    case ST_INIT_ATE0:     return "ATE0";
    case ST_INIT_ATL1:     return "ATL1";
    case ST_INIT_ATS1:     return "ATS1";
    case ST_INIT_ATH0:     return "ATH0";
    case ST_INIT_ATSP6:    return "ATSP6";
    case ST_INIT_ATSH7E0:  return "ATSH7E0";
    case ST_INIT_ATS0:     return "ATS0";
    case ST_INIT_ATSTFF:   return "ATST64";
    case ST_PID_RUN:       return "010C010D";  /* 复合请求：转速+车速 */
    case ST_PID_COOLANT:   return "0105";
    case ST_PID_VOLTAGE:   return "0142";
    case ST_PID_ODOMETER:  return "01A6";
    default: return "";
    }
}

static int tof(st_t st)
{
    if (st == ST_INIT_ATZ) return 3000;  /* ATZ 复位需要时间 */
    if (st <= ST_INIT_ATSTFF) return 500;
    return 1500;  /* PID 阶段：等 > 到来 */
}

/* ========== 公共接口 ========== */
void obd_init(void)
{
    memset(&s_data, 0, sizeof(s_data));
    s_running = false;
    s_st = ST_IDLE;
    s_atz_ready = false;
    s_rx_trigger = false;
    s_got_data = false;
    s_slow_tick = 0;
    memset(s_tp_slots, 0, sizeof(s_tp_slots));
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
    s_got_data = false;
    s_slow_tick = 0;
    memset(s_tp_slots, 0, sizeof(s_tp_slots));
    s_power_integral = 0.0f;
    s_energy_start_ms = lv_tick_get();
    s_energy_last_ms = s_energy_start_ms;
    ESP_LOGI(TAG, "start (composite mode)");
}

void obd_stop(void)
{
    s_running = false;
    s_st = ST_IDLE;
    s_got_data = false;
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

    /* ISO-TP 超时清理 */
    tp_timeout_check(now);

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

    /* 收到响应触发后推进状态机 */
    if (s_rx_trigger && s_wait > 0) {
        s_rx_trigger = false;
        s_wait = 0;
        s_got_data = false;
        next();
    }

    /* 超时兜底 */
    if (s_wait > 0 && (now - s_wait) > (uint32_t)tof(s_st)) {
        s_wait = 0;
        if (s_st <= ST_INIT_ATSTFF) {
            if (++s_retry > 5) {
                s_running = false;
                ESP_LOGW(TAG, "init timeout, stop");
                return;
            }
            ESP_LOGW(TAG, "timeout retry %d, reset to ATZ", s_retry);
            s_st = ST_INIT_ATZ;
            s_atz_ready = false;
            s_got_data = false;
        } else {
            s_got_data = false;
            next();
        }
    }

    /* 写失败暂停 */
    if (s_tx_fail >= 5 && (now - s_wait) < 500) return;
    if (s_tx_fail >= 5) s_tx_fail = 0;

    if (s_wait == 0) {
        /* init 阶段：检查最小命令间隔 */
        if (s_st > ST_IDLE && s_st <= ST_INIT_ATSTFF && (now - s_last_tx_ms) < INIT_MIN_INTERVAL) {
            return;
        }
        /* PID 阶段：检查最小命令间隔，防止设备堆积崩溃 */
        if (s_st >= ST_PID_RUN && (now - s_last_tx_ms) < PID_MIN_INTERVAL) {
            return;
        }
        const char *c = cmd_of(s_st);
        if (c[0]) {
            if (tx(c)) s_wait = now;
            else s_wait = now;
        } else {
            next();
        }
    }
}

const obd_data_t *obd_get_data(void)
{
    return &s_data;
}