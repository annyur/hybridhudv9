/* scan.h — BLE 扫描 */
#ifndef SCAN_H
#define SCAN_H

#ifdef __cplusplus
extern "C" {
#endif

void scan_init(void);
void scan_start(void);
void scan_stop(void);
void scan_poll(void);

#ifdef __cplusplus
}
#endif

#endif