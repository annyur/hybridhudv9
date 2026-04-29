/* ble.h — BLE 连接 */
#ifndef BLE_H
#define BLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

void ble_init(void);
void ble_update(void);
void ble_enable(void);
void ble_disable(void);
bool ble_is_connected(void);

/* 原始数据接口（供 ELM327 / OBD 使用） */
bool ble_write(const char *data);
bool ble_rx_ready(void);
const char *ble_rx_buf(void);
void ble_rx_clear(void);

#ifdef __cplusplus
}
#endif

#endif