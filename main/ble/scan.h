/* scan.h — BLE 扫描 */
#ifndef SCAN_H
#define SCAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char name[32];
    uint8_t addr[6];
    uint8_t addr_type;  /* BLE_ADDR_PUBLIC=0, BLE_ADDR_RANDOM=1, BLE_ADDR_RPA_PUB=2, BLE_ADDR_RPA_RAND=3 */
    int8_t rssi;
} scan_device_t;

void scan_init(void);
void scan_start(void);
void scan_stop(void);
void scan_poll(void);

int scan_get_count(void);
const scan_device_t *scan_get_device(int idx);
bool scan_is_running(void);

#ifdef __cplusplus
}
#endif

#endif