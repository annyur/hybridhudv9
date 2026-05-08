/* obd.c — OBD-II PID 轮询 + ISO-TP 多帧拼接 + UDS DID 分发
 *
 * 核心设计（功率优先）:
 *   1. 串行状态机: ST_INIT_ATZ → ... → ST_PID_RUN → ECU DID
 *   2. 双速轮询（功率优先，20Hz）:
 *      - fast round: 不发PID，只发ECU DID（burst=4，F228/F229 50ms轮询）
 *      - slow round: 010C(转速) + 0105(环境温度) + 01A6(里程) + 012F(油量) + ECU
 *   3. ISO-TP 多帧拼接: SF/FF/CF 自动组装
 *   4. UDS DID 分发: 识别 62/7A9/7EF 响应, 回调 ecu_parse_uds()
 *
 * 非阻塞数据同步:
 *   - OBD 任务收集数据后发送到消息队列
 *   - UI 任务从队列非阻塞接收，队列为空时立即返回
 *
 * 车辆: Mazda EZ-6 / 长安深蓝 SL03 (VIN: LVRHDCEM3SN021133)
 * 协议: ISO 15765-4 CAN 11-bit 500kbps (ATSP6)
 */
#include "obd.h"
#include "elm.h"
#include "ecu.h"
#include "ble.h"
#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

static const char *TAG = "OBD";
static obd_data_t s_data = {0};
static bool s_running = false;

/* ---------- 消息队列 ---------- */
#define OBD_QUEUE_LENGTH  8   /* 队列长度（减少到8，更合理）*/
#define OBD_QUEUE_ITEM_SIZE sizeof(obd_data_t)
static QueueHandle_t s_obd_queue = NULL;
static uint32_t s_last_send_ms = 0;
static uint32_t s_queue_overflow_cnt = 0;  /* 溢出计数器 */
#define OBD_QUEUE_SEND_INTERVAL_MS 33  /* 发送间隔 - 调整到约30Hz，匹配UI刷新率 */

/* ---------- 状态机定义 ---------- */
typedef enum {
    ST_IDLE = 0,
    ST_INIT_ATZ,      /* ATZ 复位 */
    ST_INIT_ATE0,     /* ATE0 关闭回显 */
    ST_INIT_ATL1,     /* ATL1 换行 */
    ST_INIT_ATS1,     /* ATS1 空格 */
    ST_INIT_ATH0,     /* ATH0 隐藏头部 */
    ST_INIT_ATSP6,    /* ATSP6 CAN 500k */
    ST_INIT_ATSH7E0,  /* ATSH7E0 目标地址 */
    ST_INIT_ATS0,     /* ATS0 自动搜索关闭 */
    ST_INIT_ATSTFF,   /* ATST64 超时 64ms */
    ST_PID_RUN,       /* 010C 转速(slow round) */
    ST_PID_RPM,       /* 010C 转速(slow round中发) */
    ST_PID_COOLANT,   /* 0105 冷却液 */
    ST_PID_ODOMETER,  /* 01A6 里程 */
    ST_PID_FUEL,      /* 012F 燃油液位 */
    ST_ECU_ATSH,      /* ATSH7A1 切 header */
    ST_ECU_REQ,       /* 22XXXX 请求 DID */
    ST_ECU_RESTORE,   /* ATSH7E0 恢复 header */
    ST_ECU_RESTORE_THEN_ATSH,  /* RESTORE 后直接 ATSH（不同地址DID连续查询） */
    ST_COUNT
} st_t;

static st_t s_st = ST_IDLE;
static uint32_t s_wait = 0;        /* 等待响应时间戳 */
static int s_retry = 0;            /* init 重试计数 */
static int s_tx_fail = 0;          /* 发送失败计数 */
static bool s_atz_ready = false;   /* ATZ 后收到 > */

/* 平均功耗积分 */
static float    s_power_integral = 0.0f;
static uint32_t s_energy_start_ms = 0;
static uint32_t s_energy_last_ms = 0;

/* 响应处理状态 */
static bool s_rx_trigger = false;  /* 收到响应, 推进状态机 */
static bool s_got_data = false;   /* PID 阶段已收到有效数据 */

/* ECU DID 命令 buffer（由 next() 填充，cmd_of() 返回） */
static char s_ecu_header_buf[16];
static char s_ecu_req_buf[16];
static char s_ecu_current_addr[8];  /* 当前 ECU 地址（如 "7A1"），用于判断同地址连续查询 */

/* ECU burst 限制: 每个PID轮次中ECU最多连续发N个DID，防止ECU阶段过长导致车速刷新延迟 */
static int s_ecu_burst_cnt = 0;
static int s_ecu_burst_max = 1;  /* 当前轮次的burst上限，减少到1以减轻OBD设备负担 */

/* 命令间隔控制 */
#define PID_MIN_INTERVAL  150    /* PID 阶段最小间隔 ms（增加到150ms，减少OBD设备压力） */
#define INIT_MIN_INTERVAL 100    /* init 阶段最小间隔 ms（增加到100ms，提高初始化稳定性） */
static uint32_t s_last_tx_ms = 0;

/* 双速轮询 */
static int s_slow_tick = 0;
#define SLOW_EVERY 4              /* 每4轮做一次慢轮全量 */

/* 冷却液温度轮询控制 - 降低频率，非关键数据 */
static int s_coolant_tick = 0;
#define COOLANT_EVERY 8           /* 每8个slow round才轮询一次冷却液温度 */

/* 响应缓存 - 用于检测重复数据，避免重复解析 */
static char s_last_pid_resp[64];  /* 缓存上次 PID 响应 */
static char s_last_ecu_resp[64];  /* 缓存上次 ECU DID 响应 */

/* ---------- ISO-TP 多帧拼接器 ---------- */
#define TP_MAX_SLOTS 8
#define TP_MAX_DATA  32
#define TP_TIMEOUT_MS 2000

typedef struct {
    uint16_t can_id;    /* 源 CAN ID */
    bool     active;    /* 拼接中 */
    uint8_t  buf[TP_MAX_DATA];
    uint16_t total;     /* 期望总长度 */
    uint16_t received;  /* 已接收 */
    uint8_t  next_seq;  /* 期望下一个 CF 序号 */
    uint32_t start_ms;  /* FF 接收时间 */
} tp_slot_t;

static tp_slot_t s_tp_slots[TP_MAX_SLOTS];

/* hex_byte: 从 2 字符 hex 字符串解析 1 字节 */
static uint8_t hex_byte(const char *p)
{
    if (!p || !p[0] || !p[1]) return 0;
    char tmp[3] = {p[0], p[1], '\0'};
    return (uint8_t)strtol(tmp, NULL, 16);
}

/* extract_can_id: 从 "7E8xx..." 格式提取 3 位 CAN ID */
static uint16_t extract_can_id(const char *line)
{
    if (strlen(line) < 7) return 0;
    if (line[0] != '7') return 0;
    if (!isxdigit((unsigned char)line[1]) || !isxdigit((unsigned char)line[2])) return 0;
    if (!isxdigit((unsigned char)line[3])) return 0;
    char id_str[5] = {line[0], line[1], line[2], '\0'};
    return (uint16_t)strtol(id_str, NULL, 16);
}

/* tp_find_slot: 查找或分配 ISO-TP 拼接槽 */
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
    /* 槽满时驱逐最老的拼接槽 */
    s_tp_slots[oldest].active = false;
    s_tp_slots[oldest].can_id = can_id;
    return oldest;
}

/* parse_std 前向声明: 定义在下方 */
static void parse_std(const char *resp);

/* tp_parse_complete: 拼接完成后回调 parse_std */
static void tp_parse_complete(tp_slot_t *slot)
{
    if (slot->received == 0) return;
    char hex_str[TP_MAX_DATA * 2 + 1];
    for (int i = 0; i < slot->received; i++) {
        snprintf(hex_str + i * 2, 3, "%02X", slot->buf[i]);
    }
    hex_str[slot->received * 2] = '\0';
    ESP_LOGI(TAG, "TP asm CAN=0x%03X len=%d data=%s", slot->can_id, slot->received, hex_str);
    /* 组装完成后复用 parse_std 解析数据内容 */
    parse_std(hex_str);
}

/* iso_tp_feed: 处理 SF/FF/CF 帧 */
static void iso_tp_feed(const char *resp, uint16_t can_id)
{
    int slot = tp_find_slot(can_id);
    const char *p = resp + 3; /* 跳过 CAN ID 头部 */
    uint8_t pci = hex_byte(p);
    uint8_t type = pci & 0xF0;

    if (type == 0x00) { /* SF: 单帧 */
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
    } else if (type == 0x10) { /* FF: 首帧 */
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
    } else if (type == 0x20) { /* CF: 连续帧 */
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

/* tp_timeout_check: 清理 ISO-TP 超时拼接槽 */
static void tp_timeout_check(uint32_t now)
{
    for (int i = 0; i < TP_MAX_SLOTS; i++) {
        if (s_tp_slots[i].active && (now - s_tp_slots[i].start_ms) > TP_TIMEOUT_MS) {
            ESP_LOGW(TAG, "TP timeout CAN=0x%03X", s_tp_slots[i].can_id);
            s_tp_slots[i].active = false;
        }
    }
}

/* parse_hex: 将 HEX 字符串解析为字节数组 */
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

/* tx: 发送命令并记录时间 */
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

/* ---------- parse_std: 循环解析一行中的多个 PID ----------
 * 复合请求 010C010D 的响应可能包含 41 0C xx xx 41 0D xx
 * 逐段扫描 41 xx 模式, 分别提取数据 */
static void parse_std(const char *resp)
{
    const char *p = resp;

    /* 跳过紧凑 CAN 头部: 7E8xx / 7E0xx / 7E4xx (3字符 ID + 2字符 DLC) */
    if ((strncmp(p, "7E8", 3) == 0 || strncmp(p, "7E0", 3) == 0 ||
         strncmp(p, "7E4", 3) == 0) && strlen(p) >= 5) {
        p += 5;
    }

    uint8_t d[32];
    int n = parse_hex(p, d, sizeof(d));
    if (n < 3) return;

    /* 循环扫描所有 41 开头的 PID 响应段 */
    for (int i = 0; i < n - 2; i++) {
        if (d[i] != 0x41) continue;
        uint8_t pid = d[i + 1];
        int avail = n - i - 2;
        switch (pid) {
        case 0x0C:
            if (avail >= 2) { s_data.rpm = ((d[i + 2] * 256) + d[i + 3]) / 4; }
            break;
        /* 0x0D(车速)已移除: 不再轮询车速，优先级给功率DID */
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
        /* 已扫描确认支持, 解析就绪, 按需加入轮询 */
        case 0x04:
            if (avail >= 1) { s_data.engine_load = d[i + 2] * 100.0f / 255.0f; }
            break;
        case 0x06:
            if (avail >= 1) { s_data.fuel_trim_short = (d[i + 2] - 128) * 100.0f / 128.0f; }
            break;
        case 0x07:
            if (avail >= 1) { s_data.fuel_trim_long = (d[i + 2] - 128) * 100.0f / 128.0f; }
            break;
        case 0x0B:
            if (avail >= 1) { s_data.map = d[i + 2]; }
            break;
        case 0x0E:
            if (avail >= 1) { s_data.timing_advance = (d[i + 2] - 128) / 2.0f; }
            break;
        case 0x0F:
            if (avail >= 1) { s_data.intake_temp = d[i + 2] - 40; }
            break;
        case 0x14:
            if (avail >= 1) { s_data.o2s11_voltage = d[i + 2] * 0.005f; }
            break;
        case 0x15:
            if (avail >= 1) { s_data.o2s12_voltage = d[i + 2] * 0.005f; }
            break;
        case 0x1C:
            if (avail >= 1) { s_data.obd_std = d[i + 2]; }
            break;
        case 0x1F:
            if (avail >= 2) { s_data.runtime = (d[i + 2] * 256) + d[i + 3]; }
            break;
        case 0x21:
            if (avail >= 2) { s_data.dist_mil = (d[i + 2] * 256) + d[i + 3]; }
            break;
        case 0x23:
            if (avail >= 2) { s_data.fuel_rail_press = (d[i + 2] * 256 + d[i + 3]) * 10.0f; }
            break;
        case 0x2D:
            if (avail >= 1) { s_data.egr_err = (d[i + 2] - 128) * 100.0f / 128.0f; }
            break;
        case 0x2E:
            if (avail >= 1) { s_data.evap_pct = d[i + 2] * 100.0f / 255.0f; }
            break;
        case 0x2F:
            if (avail >= 1) { s_data.fuel_level = d[i + 2] * 100.0f / 255.0f; }
            break;
        case 0x33:
            if (avail >= 1) { s_data.baro = d[i + 2]; }
            break;
        case 0x46:
            if (avail >= 1) { s_data.ambient_temp = d[i + 2] - 40; }
            break;
        }
    }
}

/* ---------- parse_loose: 不依赖 41 前缀, 直接扫描 PID 码 ----------
 * 用于处理 ELM327 复合请求多帧格式:
 *   0:410C0000 / 1:07E5E50D000000 (第二帧无 41 前缀) */
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
        /* 0x0D(车速)已移除: 不再轮询车速，优先级给功率DID */
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
        /* 已扫描确认支持, 解析就绪 */
        case 0x04:
            if (i + 1 < n) s_data.engine_load = d[i + 1] * 100.0f / 255.0f;
            break;
        case 0x06:
            if (i + 1 < n) s_data.fuel_trim_short = (d[i + 1] - 128) * 100.0f / 128.0f;
            break;
        case 0x07:
            if (i + 1 < n) s_data.fuel_trim_long = (d[i + 1] - 128) * 100.0f / 128.0f;
            break;
        case 0x0B:
            if (i + 1 < n) s_data.map = d[i + 1];
            break;
        case 0x0E:
            if (i + 1 < n) s_data.timing_advance = (d[i + 1] - 128) / 2.0f;
            break;
        case 0x0F:
            if (i + 1 < n) s_data.intake_temp = d[i + 1] - 40;
            break;
        case 0x14:
            if (i + 1 < n) s_data.o2s11_voltage = d[i + 1] * 0.005f;
            break;
        case 0x15:
            if (i + 1 < n) s_data.o2s12_voltage = d[i + 1] * 0.005f;
            break;
        case 0x1C:
            if (i + 1 < n) s_data.obd_std = d[i + 1];
            break;
        case 0x1F:
            if (i + 2 < n) s_data.runtime = (d[i + 1] * 256) + d[i + 2];
            break;
        case 0x21:
            if (i + 2 < n) s_data.dist_mil = (d[i + 1] * 256) + d[i + 2];
            break;
        case 0x23:
            if (i + 2 < n) s_data.fuel_rail_press = (d[i + 1] * 256 + d[i + 2]) * 10.0f;
            break;
        case 0x2D:
            if (i + 1 < n) s_data.egr_err = (d[i + 1] - 128) * 100.0f / 128.0f;
            break;
        case 0x2E:
            if (i + 1 < n) s_data.evap_pct = d[i + 1] * 100.0f / 255.0f;
            break;
        case 0x2F:
            if (i + 1 < n) s_data.fuel_level = d[i + 1] * 100.0f / 255.0f;
            break;
        case 0x33:
            if (i + 1 < n) s_data.baro = d[i + 1];
            break;
        case 0x46:
            if (i + 1 < n) s_data.ambient_temp = d[i + 1] - 40;
            break;
        }
    }
}

/* is_prompt: 检查是否为 ELM327 提示符 > */
static bool is_prompt(const char *resp)
{
    return (resp[0] == '>' && resp[1] == '\0');
}

/* is_ok: 检查是否为 OK */
static bool is_ok(const char *resp)
{
    return (strcasecmp(resp, "OK") == 0);
}

/* is_echo: 判断是否为当前命令的回显 */
static bool is_echo(const char *resp)
{
    switch (s_st) {
    case ST_PID_RUN:      return (strcmp(resp, "010C") == 0);
    case ST_PID_RPM:      return (strcmp(resp, "010C") == 0);
    case ST_PID_COOLANT:  return (strcmp(resp, "0105") == 0);
    case ST_PID_ODOMETER: return (strcmp(resp, "01A6") == 0);
    case ST_PID_FUEL:     return (strcmp(resp, "012F") == 0);
    case ST_ECU_ATSH:
    case ST_ECU_REQ:
    case ST_ECU_RESTORE:
    case ST_ECU_RESTORE_THEN_ATSH:  return false;  /* ECU 命令不回显 */
    default: return false;
    }
}

/* ---------- handle_rx: 响应分类处理 ----------
 * 1. 41xx 标准 PID 数据
 * 2. ELM327 多帧格式 (0: / 1:)
 * 3. UDS DID 响应 (62 / 7A9 / 7EF)
 * 4. ISO-TP CAN 帧 (7E8/7E0/7E4)
 * 5. 错误/提示处理 (STOPPED / NO DATA / > / OK) */
static void handle_rx(const char *resp)
{
    /* 重复数据检测: 如果响应与上次相同，跳过解析，提升系统资源利用 */
    bool is_pid_resp = (resp[0] == '4' && (resp[1] == '1' || resp[1] == '2')) ||
                       (resp[0] == '0' && resp[1] == ':') ||
                       (resp[0] == '1' && resp[1] == ':');
    bool is_ecu_resp = ((resp[0] == '6' && (resp[1] == '2' || resp[1] == '3')) ||
                        (strncmp(resp, "7F", 2) == 0) ||
                        (strncmp(resp, "7A9", 3) == 0) ||
                        (strncmp(resp, "7EF", 3) == 0));

    if (is_pid_resp) {
        if (strcmp(resp, s_last_pid_resp) == 0) {
            /* 慢轮数据重复，跳过解析 */
            ESP_LOGD(TAG, "PID resp unchanged, skip parse: %s", resp);
            s_got_data = true;
            return;
        }
        /* 更新缓存 */
        strncpy(s_last_pid_resp, resp, sizeof(s_last_pid_resp) - 1);
        s_last_pid_resp[sizeof(s_last_pid_resp) - 1] = '\0';
    } else if (is_ecu_resp) {
        if (strcmp(resp, s_last_ecu_resp) == 0) {
            /* ECU DID 数据重复，跳过解析 */
            ESP_LOGD(TAG, "ECU resp unchanged, skip parse: %s", resp);
            ecu_clear_pending();
            s_got_data = true;
            return;
        }
        /* 更新缓存 */
        strncpy(s_last_ecu_resp, resp, sizeof(s_last_ecu_resp) - 1);
        s_last_ecu_resp[sizeof(s_last_ecu_resp) - 1] = '\0';
    }

    /* 1. 41xx 标准 PID 响应 (ATH0 模式, 无 CAN 头部) */
    if (resp[0] == '4' && (resp[1] == '1' || resp[1] == '2')) {
        parse_std(resp);
        s_got_data = true;
        return;
    }

    /* 2. ELM327 复合请求多帧格式 */
    if (resp[0] == '0' && resp[1] == ':') {
        const char *payload = resp + 2;
        parse_std(payload);
        parse_loose(payload);
        s_got_data = true;
        return;
    }
    if (resp[0] == '1' && resp[1] == ':') {
        const char *payload = resp + 2;
        parse_loose(payload);
        s_got_data = true;
        return;
    }
    /* 长度指示如 "00B" 直接忽略 */
    if (resp[0] == '0' && isxdigit((unsigned char)resp[1]) && isxdigit((unsigned char)resp[2]) && resp[3] == '\0') {
        return;
    }

    /* 3. UDS DID 响应: 62(正响应) / 7A9(BMS) / 7EF(SGCM) / 7F(负响应)
     * 收到 62xxxx 后不立刻推进，标记 s_got_data=true
     * 等 > 提示符到来后再推进到 RESTORE，避免 ATSH7E0 打断 ELM327 输出 */
    if ((resp[0] == '6' && (resp[1] == '2' || resp[1] == '3')) ||
        (strncmp(resp, "7F", 2) == 0)) {
        ecu_parse_uds(resp);
        ecu_clear_pending();
        s_got_data = true;  /* 标记已收到数据，等 > 到来后推进 */
        return;
    }
    if ((strncmp(resp, "7A9", 3) == 0 || strncmp(resp, "7EF", 3) == 0) && strlen(resp) > 5) {
        ecu_parse_uds(resp);
        ecu_clear_pending();
        s_got_data = true;  /* 标记已收到数据，等 > 到来后推进 */
        return;
    }

    /* 4. ISO-TP CAN 帧检测与拼接 (ATH1 模式) */
    uint16_t can_id = extract_can_id(resp);
    if (can_id != 0) {
        iso_tp_feed(resp, can_id);
        return;
    }

    ESP_LOGI(TAG, "RX[%d]: '%s'", s_st, resp);

    /* 5. 错误与状态处理 */
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
            /* init 阶段: > 推进, 要求确实发过命令 */
            if (s_wait > 0) s_rx_trigger = true;
        } else if (s_st == ST_ECU_REQ) {
            /* ECU REQ阶段: 如果已收到62xxxx数据，> 表示 ELM327 输出完成
             * 此时才推进到 RESTORE，避免 ATSH7E0 打断 ELM327 输出导致 STOPPED */
            if (s_got_data && s_wait > 0) {
                s_rx_trigger = true;
            }
            return;
        } else if (s_st >= ST_ECU_ATSH) {
            /* ECU ATSH/RESTORE阶段: > 直接推进 */
            if (s_wait > 0) s_rx_trigger = true;
        } else {
            /* PID 阶段: > 只在已收到数据后才推进 */
            if (s_got_data && s_wait > 0) s_rx_trigger = true;
        }
        return;
    }

    if (is_echo(resp)) {
        return; /* PID 命令的回显, 忽略 */
    }

    if (is_ok(resp)) {
        if (s_st < ST_PID_RUN) {
            s_rx_trigger = true;
        } else if (s_st == ST_ECU_ATSH || s_st == ST_ECU_RESTORE || s_st == ST_ECU_RESTORE_THEN_ATSH) {
            /* ECU 的 ATSH/RESTORE 命令: OK 后立即推进，省去1500ms超时等待 */
            if (s_wait > 0) s_rx_trigger = true;
        }
        return;
    }

    if (strstr(resp, "ELM327")) {
        if (s_st == ST_INIT_ATZ) s_atz_ready = true;
        return; /* 不推进, 等 > 到来 */
    }

    if (strstr(resp, "SEARCHING")) return;
}

/* next: 状态机推进（统一调度 OBD PID + ECU DID） */
static void next(void)
{
    st_t old = s_st;

    /* === ECU 状态流转（优化后：同地址DID连续查询，减少ATSH切换） ===
     * ATSH → REQ → [同地址?直接下一个REQ : 不同地址?RESTORE→ATSH : RESTORE→PID_RUN]
     * 设计目标: 相同header的DID连续发22XX，只在地址变化或全部完成时才切回7E0
     * burst限制: 每个PID轮次中ECU最多连续发ECU_BURST_MAX个DID，防止ECU阶段过长导致车速刷新延迟 */
    if (old == ST_ECU_ATSH) {
        /* 从 ATSH7A1 命令中提取当前地址 */
        if (strncmp(s_ecu_header_buf, "ATSH", 4) == 0) {
            strncpy(s_ecu_current_addr, s_ecu_header_buf + 4, sizeof(s_ecu_current_addr) - 1);
            s_ecu_current_addr[sizeof(s_ecu_current_addr) - 1] = '\0';
        }
        s_st = ST_ECU_REQ;
        s_ecu_burst_cnt++;
        return;
    }
    if (old == ST_ECU_REQ) {
        uint16_t did;
        const char *addr;
        /* burst超限: 强制回到PID，保证车速刷新 */
        if (s_ecu_burst_cnt >= s_ecu_burst_max) {
            s_st = ST_ECU_RESTORE;
            return;
        }
        /* 先偷看下一个DID是否同地址 */
        if (ecu_peek_next_did(&did, &addr) && strcmp(addr, s_ecu_current_addr) == 0) {
            /* 同地址: 消耗队列，直接发下一个22XX，不切回7E0 */
            ecu_get_next_did(&did, &addr);
            snprintf(s_ecu_req_buf, sizeof(s_ecu_req_buf), "22%04X", did);
            s_st = ST_ECU_REQ;  /* 保持REQ状态 */
            s_ecu_burst_cnt++;
        } else if (ecu_get_next_did(&did, &addr)) {
            /* 不同地址: RESTORE → ATSH → REQ */
            snprintf(s_ecu_header_buf, sizeof(s_ecu_header_buf), "ATSH%s", addr);
            snprintf(s_ecu_req_buf, sizeof(s_ecu_req_buf), "22%04X", did);
            s_st = ST_ECU_RESTORE_THEN_ATSH;
        } else {
            /* 没有更多DID，切回7E0 */
            s_st = ST_ECU_RESTORE;
        }
        return;
    }
    if (old == ST_ECU_RESTORE) {
        s_ecu_burst_cnt = 0;  /* 重置burst计数 */
        s_got_data = false;   /* 重置数据标记 */
        s_st = ST_PID_RUN;
        return;
    }
    if (old == ST_ECU_RESTORE_THEN_ATSH) {
        s_got_data = false;   /* 重置数据标记 */
        s_st = ST_ECU_ATSH;
        return;
    }

    /* === PID 轮结束：检查 ECU DID + 双速轮询计数 ===
     * 在FUEL结束时计数slow_tick，决定下一轮是fast(只发010D)还是slow(完整PID) */
    if (old == ST_PID_FUEL) {
        s_ecu_burst_cnt = 0;  /* 每个PID轮次开始时重置burst计数 */

        uint16_t did;
        const char *addr;
        bool has_ecu = ecu_get_next_did(&did, &addr);

        s_slow_tick++;
        if (s_slow_tick > 0 && s_slow_tick < SLOW_EVERY) {
            /* fast round: 不发PID + ECU(4个DID: V/I/Tmax/Tmin, 50ms间隔) */
            s_ecu_burst_max = 5;
            ESP_LOGI(TAG, "fast round %d/%d, ECU5(V/I/Tmax/Tmin)", s_slow_tick, SLOW_EVERY);
        } else {
            /* slow round: 走完整 PID + ECU(4个DID) */
            s_slow_tick = 0;
            s_ecu_burst_max = 5;
            ESP_LOGI(TAG, "slow round, all PIDs + ECU5(V/I/Tmax/Tmin)");
        }

        if (has_ecu) {
            snprintf(s_ecu_header_buf, sizeof(s_ecu_header_buf), "ATSH%s", addr);
            snprintf(s_ecu_req_buf, sizeof(s_ecu_req_buf), "22%04X", did);
            strncpy(s_ecu_current_addr, addr, sizeof(s_ecu_current_addr) - 1);
            s_ecu_current_addr[sizeof(s_ecu_current_addr) - 1] = '\0';
            s_st = ST_ECU_ATSH;
        } else {
            s_st = ST_PID_RUN;
        }
        return;
    }

    /* === ST_PID_RUN: fast round 不发任何PID，只发ECU DID ===
     * fast round: 跳过010C，直接发ECU DID（burst=4，最大化功率刷新率）
     * slow round: 正常发010C → 0105 → 01A6 → 012F + ECU */
    if (old == ST_PID_RUN) {
        if (s_slow_tick > 0 && s_slow_tick < SLOW_EVERY) {
            /* fast round: 不发PID，只发ECU DID（burst=5: F228/F229/F252/F253 + 1功率预留） */
            s_ecu_burst_cnt = 0;
            s_ecu_burst_max = 5;  /* 功率+温度，50ms间隔 */
            uint16_t did;
            const char *addr;
            if (ecu_get_next_did(&did, &addr)) {
                snprintf(s_ecu_header_buf, sizeof(s_ecu_header_buf), "ATSH%s", addr);
                snprintf(s_ecu_req_buf, sizeof(s_ecu_req_buf), "22%04X", did);
                strncpy(s_ecu_current_addr, addr, sizeof(s_ecu_current_addr) - 1);
                s_ecu_current_addr[sizeof(s_ecu_current_addr) - 1] = '\0';
                s_st = ST_ECU_ATSH;
            } else {
                s_st = ST_PID_RUN;
            }
            return;
        }
        /* slow round: ST_PID_RUN发010C(转速)，直接流转到COOLANT */
        s_st = ST_PID_COOLANT;
        return;
    }

    /* === init 和 PID 正常推进 === */
    
    /* 冷却液温度轮询控制: 非关键数据，降低轮询频率 */
    if (old == ST_PID_COOLANT) {
        s_coolant_tick++;
        /* 每 COOLANT_EVERY 个周期才轮询一次冷却液温度（安全冗余）*/
        if (s_coolant_tick >= COOLANT_EVERY) {
            s_coolant_tick = 0;
            /* 正常推进到下一个 PID */
            s_st = ST_PID_ODOMETER;
        } else {
            /* 跳过冷却液温度，直接进入 ECU 阶段 */
            s_st = ST_ECU_ATSH;
        }
        return;
    }
    
    /* 其他状态正常推进 */
    s_st++;
    if (s_st >= ST_ECU_ATSH) s_st = ST_PID_RUN;

    /* init→PID 切换 */
    if (old <= ST_INIT_ATSTFF && s_st >= ST_PID_RUN) {
        elm_rx_clear();
        s_got_data = false;
        s_rx_trigger = false;
        s_slow_tick = 0;
        s_coolant_tick = 0;  /* 重置冷却液轮询计数 */
        ecu_enter_pid();
        ESP_LOGI(TAG, "enter PID, clear backlog");
    }
}

/* cmd_of: 状态 → 命令字符串 */
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
    case ST_PID_RUN:       return "010C";    /* 转速（slow round）*/
    case ST_PID_RPM:       return "010C";    /* 转速 */
    case ST_PID_COOLANT:   return "0105";
    case ST_PID_ODOMETER:  return "01A6";
    case ST_PID_FUEL:      return "012F";
    case ST_ECU_ATSH:              return s_ecu_header_buf;
    case ST_ECU_REQ:               return s_ecu_req_buf;
    case ST_ECU_RESTORE:           return "ATSH7E0";
    case ST_ECU_RESTORE_THEN_ATSH: return "ATSH7E0";
    default: return "";
    }
}

/* tof: 超时时间 (ms) */
static int tof(st_t st)
{
    if (st == ST_INIT_ATZ) return 2000;  /* ATZ 复位需 2s（从3s减少）*/
    if (st <= ST_INIT_ATSTFF) return 600;  /* 初始化阶段 600ms（从1s减少）*/
    if (st == ST_ECU_RESTORE_THEN_ATSH) return 60;   /* RESTORE→ATSH 快速连续 */
    if (st >= ST_ECU_ATSH && st <= ST_ECU_RESTORE) return 200;  /* ECU 阶段200ms超时 */
    return 500;  /* PID 阶段: 500ms超时（从1s减少）*/
}

/* ---------- 公共接口 ---------- */

/* obd_init: 初始化 OBD 状态机 + ecu 轮询器 */
void obd_init(void)
{
    memset(&s_data, 0, sizeof(s_data));
    s_running = false;
    s_st = ST_IDLE;
    s_atz_ready = false;
    s_rx_trigger = false;
    s_got_data = false;
    s_slow_tick = 0;
    s_coolant_tick = 0;
    memset(s_tp_slots, 0, sizeof(s_tp_slots));
    memset(s_last_pid_resp, 0, sizeof(s_last_pid_resp));
    memset(s_last_ecu_resp, 0, sizeof(s_last_ecu_resp));
    elm_init();
    ecu_init();
}

/* obd_start: 启动 OBD 轮询 */
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

/* obd_stop: 停止 OBD 轮询 */
void obd_stop(void)
{
    s_running = false;
    s_st = ST_IDLE;
    s_got_data = false;
    ESP_LOGI(TAG, "stop");
}

/* obd_is_running: 查询 OBD 是否正在运行 */
bool obd_is_running(void) { return s_running; }

/* obd_update: 状态机轮询 (每 20ms 调用) */
void obd_update(void)
{
    if (!s_running) return;

    elm_poll(0);
    while (elm_rx_ready()) handle_rx(elm_rx_buf());

    uint32_t now = lv_tick_get();

    /* 清理 ISO-TP 超时拼接槽 */
    tp_timeout_check(now);

    /* ---------- 功率估算 ----------
     * 优先: 电池总功率 P = U × I / 1000 [kW]
     *        条件: bms_total_v > 10V && |bms_total_current| > 0.1A
     *        F228=电压(V), F229=电流(A), 正负保留(正=放电, 负=回收)
     * 回退: throttle × speed 估算 (避免一直为 0) */
    {
        float i_batt = s_data.bms_total_current;  /* F229, (raw-6000)*0.1, 有符号 */
        float u_batt = s_data.bms_total_v;         /* F228, raw*0.1 [V] */
        if (fabsf(i_batt) > 0.1f && u_batt > 10.0f) {
            s_data.power_kw = (u_batt * i_batt) / 1000.0f;
            ESP_LOGD(TAG, "power real: %.1fkW (I=%.1fA U=%.1fV)",
                     s_data.power_kw, i_batt, u_batt);
        } else {
            /* 回退估算: power = throttle × speed × 0.008 */
            float v_mps = s_data.speed / 3.6f;
            s_data.power_kw = (s_data.throttle / 100.0f) * (v_mps / 100.0f) * 80.0f;
            ESP_LOGD(TAG, "power fallback: %.1fkW (thr=%d spd=%d)",
                     s_data.power_kw, s_data.throttle, s_data.speed);
        }
    }

    /* 累加功率对时间的积分
     * 取 fabsf(power_kw): 动能回收也计入能耗, avg_kw 始终为正值 */
    if (s_energy_start_ms > 0 && s_energy_last_ms > 0) {
        uint32_t dt = now - s_energy_last_ms;
        if (dt > 0 && dt < 5000) {
            s_power_integral += fabsf(s_data.power_kw) * dt;
            uint32_t total_ms = now - s_energy_start_ms;
            if (total_ms > 0) {
                s_data.avg_kw = s_power_integral / (float)total_ms;
            }
        }
        s_energy_last_ms = now;
    }

    /* 收到响应后推进状态机 */
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

    /* 发送当前阶段命令 */
    if (s_wait == 0) {
        if (s_st > ST_IDLE && s_st <= ST_INIT_ATSTFF && (now - s_last_tx_ms) < INIT_MIN_INTERVAL)
            return;
        /* ECU 状态不受 PID 间隔限制，ATSH 和 22XXXX 应快速连续发出 */
        if (s_st >= ST_PID_RUN && s_st < ST_ECU_ATSH && (now - s_last_tx_ms) < PID_MIN_INTERVAL)
            return;

        const char *c = cmd_of(s_st);
        if (c[0]) {
            if (tx(c)) {
                s_wait = now;
                /* ECU REQ 发出后标记 pending */
                if (s_st == ST_ECU_REQ) {
                    uint16_t did = (uint16_t)strtol(s_ecu_req_buf + 2, NULL, 16);
                    ecu_set_pending(did);
                }
            } else {
                s_wait = now;
            }
        } else {
            next();
        }
    }
    
    /* 发送数据到消息队列，供 UI 任务非阻塞接收 */
    obd_queue_send_from_obd_update();
}

/* obd_get_data: 获取最新 OBD 数据 (只读) */
const obd_data_t *obd_get_data(void)
{
    return &s_data;
}

/* obd_get_data_rw: 内部模块写 DID 数据 */
obd_data_t *obd_get_data_rw(void)
{
    return &s_data;
}

/* ---------- 消息队列实现 ---------- */

/* obd_queue_init: 初始化 OBD 数据消息队列 */
void obd_queue_init(void)
{
    if (s_obd_queue == NULL) {
        s_obd_queue = xQueueCreate(OBD_QUEUE_LENGTH, OBD_QUEUE_ITEM_SIZE);
        if (s_obd_queue != NULL) {
            ESP_LOGI(TAG, "OBD queue initialized (len=%d, size=%u)", 
                     OBD_QUEUE_LENGTH, OBD_QUEUE_ITEM_SIZE);
        } else {
            ESP_LOGE(TAG, "Failed to create OBD queue");
        }
    }
}

/* obd_queue_receive: 从队列非阻塞接收最新 OBD 数据 */
bool obd_queue_receive(obd_data_t *data)
{
    if (s_obd_queue == NULL || data == NULL) {
        return false;
    }
    
    /* 非阻塞接收，队列为空时立即返回 */
    if (xQueueReceive(s_obd_queue, data, 0) == pdTRUE) {
        return true;
    }
    return false;
}

/* obd_queue_send: 发送 OBD 数据到队列 */
bool obd_queue_send(const obd_data_t *data)
{
    if (s_obd_queue == NULL || data == NULL) {
        return false;
    }
    
    /* 控制发送频率，避免队列过快填满 */
    uint32_t now = lv_tick_get();
    if (now - s_last_send_ms < OBD_QUEUE_SEND_INTERVAL_MS) {
        return false;
    }
    
    /* 发送数据到队列（非阻塞）*/
    if (xQueueSend(s_obd_queue, data, 0) == pdTRUE) {
        s_last_send_ms = now;
        return true;
    }
    
    /* 队列满，移除最旧数据并重试 */
    s_queue_overflow_cnt++;
    if (s_queue_overflow_cnt % 10 == 1) {  /* 每10次溢出记录一次日志 */
        ESP_LOGW(TAG, "OBD queue overflow, drop oldest (cnt=%u)", s_queue_overflow_cnt);
    }
    obd_data_t temp_data;
    xQueueReceive(s_obd_queue, &temp_data, 0);
    if (xQueueSend(s_obd_queue, data, 0) == pdTRUE) {
        s_last_send_ms = now;
        return true;
    }
    
    return false;
}

/* obd_queue_send_from_obd_update: 在 OBD 更新完成后调用，发送数据到队列 */
void obd_queue_send_from_obd_update(void)
{
    obd_queue_send(&s_data);
}

/* obd_is_busy: 查询 OBD 是否正在初始化 */
bool obd_is_busy(void)
{
    return s_running && s_st < ST_PID_RUN;
}

/* obd_is_waiting_response: 查询 OBD 是否正在等待 ELM327 响应 */
bool obd_is_waiting_response(void)
{
    return s_wait > 0;
}

/* obd_get_queue_overflow_cnt: 获取队列溢出计数 */
uint32_t obd_get_queue_overflow_cnt(void)
{
    return s_queue_overflow_cnt;
}