/* ecu.h — ECU DID 配置 */
#ifndef ECU_H
#define ECU_H

#include <stdint.h>
#include <stdbool.h>
#include "obd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ecu_parser_t)(uint16_t did, const uint8_t *d, int len, obd_data_t *out);

typedef struct {
    const char *name;
    uint16_t tx_addr;
    uint16_t rx_addr;
    const uint16_t *dids;
    int did_count;
    ecu_parser_t parser;
    bool needs_session;
} ecu_profile_t;

extern const ecu_profile_t ecu_bms;
extern const ecu_profile_t ecu_sgcm;

void ecu_init(void);
void ecu_parse_bms(uint16_t did, const uint8_t *d, int len, obd_data_t *out);
void ecu_parse_sgcm(uint16_t did, const uint8_t *d, int len, obd_data_t *out);

#ifdef __cplusplus
}
#endif

#endif