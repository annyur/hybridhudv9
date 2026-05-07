/* elm.c — ELM327 行缓冲适配层（从 BLE notify 拼包成行） */
#include "elm.h"
#include "ble.h"
#include "esp_log.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "ELM";

#define ELM_RX_SIZE 1024
static uint8_t  s_rx[ELM_RX_SIZE];
static int      s_rx_len = 0;
static bool     s_line_ready = false;
static char     s_line[256];

void elm_init(void)
{
    s_rx_len = 0;
    s_line_ready = false;
    memset(s_rx, 0, sizeof(s_rx));
    memset(s_line, 0, sizeof(s_line));
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
    /* 从 BLE 收取原始数据并追加到环形缓冲 */
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
            /* 单次数据太大，丢弃 */
            ESP_LOGW(TAG, "chunk too large (%d), drop", len);
            ble_rx_clear();
            continue;
        }
        if (s_rx_len + len >= ELM_RX_SIZE) {
            /* 缓冲区满：丢弃前半部分 */
            int half = s_rx_len / 2;
            memmove(s_rx, s_rx + half, s_rx_len - half);
            s_rx_len -= half;
            /* 再次检查是否足够 */
            if (s_rx_len + len >= ELM_RX_SIZE) {
                ESP_LOGW(TAG, "buffer overflow, clear");
                s_rx_len = 0;
            }
        }
        memcpy(s_rx + s_rx_len, buf, len);
        s_rx_len += len;
        s_rx[s_rx_len] = '\0';
        ble_rx_clear();
    }

    /* 解析出行：按 \r、\n、> 分割 */
    if (!s_line_ready && s_rx_len > 0) {
        for (int i = 0; i < s_rx_len; i++) {
            char c = (char)s_rx[i];
            if (c == '\r' || c == '\n' || c == '>') {
                int line_len = i;
                if (line_len > 0 && line_len < (int)sizeof(s_line)) {
                    memcpy(s_line, s_rx, line_len);
                    s_line[line_len] = '\0';
                    /* 检查是否有实质内容（非空/非空格） */
                    bool has_content = false;
                    for (int j = 0; j < line_len; j++) {
                        if (!isspace((unsigned char)s_line[j])) {
                            has_content = true; break;
                        }
                    }
                    if (has_content) {
                        s_line_ready = true;
                    }
                }
                /* 特殊：分隔符是 '>' 且前面没有内容时，返回 ">" 本身作为提示符 */
                if (c == '>' && line_len == 0) {
                    s_line[0] = '>';
                    s_line[1] = '\0';
                    s_line_ready = true;
                }
                /* 消费分隔符及之前的内容 */
                int consumed = i + 1;
                if (consumed < s_rx_len) {
                    memmove(s_rx, s_rx + consumed, s_rx_len - consumed);
                    s_rx_len -= consumed;
                } else {
                    s_rx_len = 0;
                }
                s_rx[s_rx_len] = '\0';
                if (s_line_ready) break;
                /* 空行继续找下一行 */
                i = -1;  /* for 循环会 ++，下次从 0 开始 */
            }
        }
    }
}

bool elm_rx_ready(void) { return s_line_ready; }

const char *elm_rx_buf(void)
{
    if (s_line_ready) {
        s_line_ready = false;
        return s_line;
    }
    return "";
}

void elm_rx_clear(void)
{
    s_line_ready = false;
    s_rx_len = 0;
    memset(s_rx, 0, sizeof(s_rx));
    memset(s_line, 0, sizeof(s_line));
}