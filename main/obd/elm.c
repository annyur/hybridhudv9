/* elm.c — ELM327 行缓冲适配层（从 BLE notify 拼包成行） */
#include "elm.h"
#include "ble.h"
#include "esp_log.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "ELM";

/* 环形缓冲区配置 */
#define ELM_RX_SIZE 1024

/* 环形缓冲区结构 */
typedef struct {
    uint8_t buf[ELM_RX_SIZE];
    int head;  /* 写入位置 */
    int tail;  /* 读取位置 */
    int len;   /* 当前数据长度 */
} ring_buf_t;

static ring_buf_t s_ring = {
    .head = 0,
    .tail = 0,
    .len = 0
};

/* 解析状态 */
static bool s_line_ready = false;
static int s_line_start = 0;  /* 当前行在环形缓冲区中的起始位置 */
static int s_line_len = 0;    /* 当前行长度 */

/* 环形缓冲区辅助函数 */
static inline int ring_write(ring_buf_t *r, const uint8_t *data, int len)
{
    if (len <= 0 || r->len + len >= ELM_RX_SIZE) {
        return -1;  /* 空间不足 */
    }

    int to_write = len;
    int first_part = ELM_RX_SIZE - r->head;

    if (to_write <= first_part) {
        /* 单次写入 */
        memcpy(r->buf + r->head, data, to_write);
        r->head = (r->head + to_write) % ELM_RX_SIZE;
    } else {
        /* 分两次写入 */
        memcpy(r->buf + r->head, data, first_part);
        memcpy(r->buf, data + first_part, to_write - first_part);
        r->head = to_write - first_part;
    }
    r->len += to_write;
    return to_write;
}

static inline int ring_read(ring_buf_t *r, uint8_t *data, int len)
{
    if (len <= 0 || r->len <= 0) {
        return 0;
    }

    int to_read = len > r->len ? r->len : len;
    int first_part = ELM_RX_SIZE - r->tail;

    if (to_read <= first_part) {
        memcpy(data, r->buf + r->tail, to_read);
        r->tail = (r->tail + to_read) % ELM_RX_SIZE;
    } else {
        memcpy(data, r->buf + r->tail, first_part);
        memcpy(data + first_part, r->buf, to_read - first_part);
        r->tail = to_read - first_part;
    }
    r->len -= to_read;
    return to_read;
}

static inline char ring_peek(ring_buf_t *r, int offset)
{
    if (offset >= r->len) {
        return '\0';
    }
    int pos = (r->tail + offset) % ELM_RX_SIZE;
    return (char)r->buf[pos];
}

static inline int ring_skip(ring_buf_t *r, int count)
{
    if (count <= 0 || count > r->len) {
        return 0;
    }
    r->tail = (r->tail + count) % ELM_RX_SIZE;
    r->len -= count;
    return count;
}

static inline int ring_find_char(ring_buf_t *r, char c, int max_search)
{
    int search_len = max_search > r->len ? r->len : max_search;
    for (int i = 0; i < search_len; i++) {
        if (ring_peek(r, i) == c) {
            return i;
        }
    }
    return -1;
}

void elm_init(void)
{
    s_ring.head = 0;
    s_ring.tail = 0;
    s_ring.len = 0;
    s_line_ready = false;
    s_line_start = 0;
    s_line_len = 0;
}

bool elm_tx(const char *data)
{
    if (!data) return false;
    ESP_LOGI(TAG, "TX raw: %s", data);
    return ble_write(data);
}

void elm_poll(uint32_t now_ms)
{
    (void)now_ms;

    /* 从 BLE 收取原始数据并写入环形缓冲区 */
    while (ble_rx_ready()) {
        const char *buf = ble_rx_buf();
        int len = (int)strlen(buf);
        if (len <= 0) {
            ble_rx_clear();
            continue;
        }
        ESP_LOGI(TAG, "RAW chunk[%d]: %.*s", len, len, buf);

        /* 缓冲区溢出保护 */
        if (len >= ELM_RX_SIZE) {
            ESP_LOGW(TAG, "chunk too large (%d), drop", len);
            ble_rx_clear();
            continue;
        }

        /* 环形缓冲区写入（无碎片移动） */
        int written = ring_write(&s_ring, (const uint8_t *)buf, len);
        if (written < 0) {
            /* 缓冲区满，丢弃最旧数据 */
            ESP_LOGW(TAG, "buffer full, drop oldest");
            ring_skip(&s_ring, s_ring.len / 2);
            ring_write(&s_ring, (const uint8_t *)buf, len);
        }
        ble_rx_clear();
    }

    /* 零拷贝内联解析：直接在环形缓冲区中查找行分隔符 */
    if (!s_line_ready && s_ring.len > 0) {
        /* 查找行分隔符：\r \n > */
        int idx_r = ring_find_char(&s_ring, '\r', s_ring.len);
        int idx_n = ring_find_char(&s_ring, '\n', s_ring.len);
        int idx_p = ring_find_char(&s_ring, '>', s_ring.len);

        /* 找到最早的分隔符位置 */
        int sep_idx = -1;
        if (idx_r >= 0) sep_idx = idx_r;
        if (idx_n >= 0 && (sep_idx < 0 || idx_n < sep_idx)) sep_idx = idx_n;
        if (idx_p >= 0 && (sep_idx < 0 || idx_p < sep_idx)) sep_idx = idx_p;

        if (sep_idx >= 0) {
            /* 找到完整的一行 */
            s_line_len = sep_idx;

            /* 特殊：分隔符是 '>' 且前面没有内容时 */
            if (s_line_len == 0 && ring_peek(&s_ring, sep_idx) == '>') {
                s_line_len = 1;  /* 返回 ">" */
                s_line_start = sep_idx;
            } else if (s_line_len > 0) {
                /* 检查是否有实质内容（非空/非空格） */
                bool has_content = false;
                for (int i = 0; i < s_line_len; i++) {
                    char c = ring_peek(&s_ring, i);
                    if (!isspace((unsigned char)c)) {
                        has_content = true;
                        break;
                    }
                }
                if (!has_content) {
                    /* 空行，跳过 */
                    ring_skip(&s_ring, sep_idx + 1);
                    return;
                }
                s_line_start = 0;
            }

            s_line_ready = true;
            /* 注意：不立即消费，等待 elm_rx_buf() 时再消费 */
        }
    }
}

/* 静态输出缓冲区（仅在数据跨越环形缓冲区边界时使用） */
static char s_line_buf[256];

bool elm_rx_ready(void) { return s_line_ready; }

const char *elm_rx_buf(void)
{
    if (!s_line_ready || s_line_len <= 0) {
        return "";
    }

    /* 计算行数据在环形缓冲区中的实际起始位置 */
    int data_start = (s_ring.tail + s_line_start) % ELM_RX_SIZE;

    if (data_start + s_line_len <= ELM_RX_SIZE) {
        /* 数据未跨越边界，直接返回指针（零拷贝） */
        s_ring.tail = (s_ring.tail + s_line_len + 1) % ELM_RX_SIZE;  /* 跳过分隔符 */
        s_ring.len -= (s_line_len + 1);
        s_line_ready = false;
        return (const char *)(s_ring.buf + data_start);
    } else {
        /* 数据跨越边界，需要拷贝到临时缓冲区 */
        int first_part = ELM_RX_SIZE - data_start;
        int second_part = s_line_len - first_part;
        
        memcpy(s_line_buf, s_ring.buf + data_start, first_part);
        memcpy(s_line_buf + first_part, s_ring.buf, second_part);
        s_line_buf[s_line_len] = '\0';
        
        s_ring.tail = (s_ring.tail + s_line_len + 1) % ELM_RX_SIZE;  /* 跳过分隔符 */
        s_ring.len -= (s_line_len + 1);
        s_line_ready = false;
        return s_line_buf;
    }
}

void elm_rx_clear(void)
{
    s_line_ready = false;
    s_ring.head = 0;
    s_ring.tail = 0;
    s_ring.len = 0;
    s_line_start = 0;
    s_line_len = 0;
}