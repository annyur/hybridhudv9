/* obd.h — OBD 数据轮询 */
#ifndef OBD_H
#define OBD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int rpm;
    int speed;
    int coolant;
    int oil;
    float epa_soc;
    int mil_on;
    int dtc_count;
    float batt_v;
    int dist;
    int map;
    int iat;
    int throttle;
    float abs_load;
    int torque_pct;
    int torque_ref;
    float odometer;

    /* ECU */
    int sgcm_rpm;
    float sgcm_current;
    int sgcm_rated;
    float sgcm_current_b;

    float bms_total_v;
    float bms_max_cell;
    float bms_min_cell;
    int bms_max_temp;
    int bms_min_temp;
    int bms_max_pos;
    int bms_min_pos;
    int bms_remain_wh;

    float power_kw;
} obd_data_t;

void obd_init(void);
void obd_update(void);
void obd_start(void);
void obd_stop(void);
const obd_data_t *obd_get_data(void);

#ifdef __cplusplus
}
#endif

#endif