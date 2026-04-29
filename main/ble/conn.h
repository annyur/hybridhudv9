/* conn.h — BLE 连接 */
#ifndef CONN_H
#define CONN_H

#ifdef __cplusplus
extern "C" {
#endif

void conn_init(void);
void conn_poll(void);

#ifdef __cplusplus
}
#endif

#endif