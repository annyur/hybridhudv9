/* ecu.h — ECU DID 配置 */
#ifndef ECU_H
#define ECU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "obd.h"

typedef void (*ecu_parser_t)(uint16_t did, const uint8_t *d, int len, obd_data_t *out);

typedef struct {
    const char *label;
    uint16_t tx_addr;
    uint16_t rx_addr;
    const uint16_t *dids;
    int did_count;
    ecu_parser_t parser;
    bool needs_session;
} ecu_config_t;

extern const ecu_config_t ecu_list[];
extern const int ecu_count;

void ecu_init(void);

void ecu_parse_bms(uint16_t did, const uint8_t *d, int len, obd_data_t *out);
void ecu_parse_sgcm(uint16_t did, const uint8_t *d, int len, obd_data_t *out);

#ifdef __cplusplus
}
#endif

#endif