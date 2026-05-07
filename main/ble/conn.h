/* conn.h — BLE 连接 */
#ifndef CONN_H
#define CONN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "esp_timer.h"

void conn_init(void);
void conn_poll(void);
void conn_connect(const uint8_t *addr, uint8_t addr_type);
void conn_disconnect(void);
bool conn_is_connected(void);
bool conn_is_ready(void);           /* GATT 发现完成，可以安全写数据 */
const uint8_t *conn_get_connected_addr(void);

/* 软件定时器接口（用于事件驱动的BLE重连） */
void conn_register_disconnect_cb(void (*cb)(void));
void conn_set_reconnect_callback(esp_timer_cb_t callback, void *arg);
void conn_start_reconnect_timer(void);
void conn_stop_reconnect_timer(void);
esp_timer_handle_t conn_get_reconnect_timer(void);

/* 原始数据接口（供 ELM327 / OBD / ble.c 使用） */
bool conn_write(const char *data, int len);
bool conn_rx_ready(void);
const char *conn_rx_buf(void);
int conn_rx_len(void);
void conn_rx_clear(void);

#ifdef __cplusplus
}
#endif

#endif