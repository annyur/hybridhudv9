/* conn.h — BLE 连接 */
#ifndef CONN_H
#define CONN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

void conn_init(void);
void conn_poll(void);
void conn_connect(const uint8_t *addr, bool public);
void conn_disconnect(void);
bool conn_is_connected(void);

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