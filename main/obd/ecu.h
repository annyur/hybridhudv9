/* ecu.h — ECU DID 配置 */
#ifndef ECU_H
#define ECU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int rpm;
    int speed;
    float soc;
    float power_kw;
    float gforce_x;
    float gforce_y;
} obd_data_t;

typedef void (*ecu_parser_t)(uint16_t did, const uint8_t *d, int len, obd_data_t *out);

typedef struct {
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