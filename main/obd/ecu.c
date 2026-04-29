/* ecu.c — ECU DID 配置与解析器 */
#include "ecu.h"

static const uint16_t s_bms_dids[] = {
    0xF250, 0xF251, 0xF252, 0xF253, 0xF254, 0xF255, 0xF229, 0xF228
};

static const uint16_t s_sgcm_dids[] = {
    0x0808, 0x0809, 0x0812, 0x080A
};

void ecu_parse_bms(uint16_t did, const uint8_t *d, int len, obd_data_t *out)
{
    switch (did) {
        case 0xF228: if (len >= 2) out->bms_total_v  = (((uint16_t)d[0] << 8) | d[1]) * 0.1f;   break;
        case 0xF250: if (len >= 2) out->bms_max_cell = (((uint16_t)d[0] << 8) | d[1]) * 0.001f; break;
        case 0xF251: if (len >= 2) out->bms_min_cell = (((uint16_t)d[0] << 8) | d[1]) * 0.001f; break;
        case 0xF252: if (len >= 1) out->bms_max_temp = (int)d[0] - 40; break;
        case 0xF253: if (len >= 1) out->bms_min_temp = (int)d[0] - 40; break;
        case 0xF254: if (len >= 1) out->bms_max_pos = d[0]; break;
        case 0xF255: if (len >= 1) out->bms_min_pos = d[0]; break;
        case 0xF229: if (len >= 2) out->bms_remain_wh = ((uint16_t)d[0] << 8) | d[1]; break;
    }
}

void ecu_parse_sgcm(uint16_t did, const uint8_t *d, int len, obd_data_t *out)
{
    switch (did) {
        case 0x0808: if (len >= 2) out->sgcm_rpm       = ((uint16_t)d[0] << 8) | d[1]; break;
        case 0x0809: if (len >= 2) out->sgcm_current   = (((uint16_t)d[0] << 8) | d[1]) * 0.01f; break;
        case 0x0812: if (len >= 2) out->sgcm_rated     = ((uint16_t)d[0] << 8) | d[1]; break;
        case 0x080A: if (len >= 2) out->sgcm_current_b = (((uint16_t)d[0] << 8) | d[1]) * 0.01f; break;
    }
}

const ecu_config_t ecu_list[] = {
    {"BMS",  0x7A1, 0x7A9, s_bms_dids,  8, ecu_parse_bms,  false},
    {"SGCM", 0x7E7, 0x7EF, s_sgcm_dids, 4, ecu_parse_sgcm, false},
};

const int ecu_count = sizeof(ecu_list) / sizeof(ecu_list[0]);

void ecu_init(void) {}
