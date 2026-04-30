/* obd.h — Mazda EZ-6 / 深蓝 SL03 增程版数据 */
#ifndef OBD_H
#define OBD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* 标准 OBD-II PID */
    int   rpm;           /* 0x010C */
    int   speed;         /* 0x010D */
    int   coolant;       /* 0x0105 */
    int   oil;           /* 0x013C */
    float epa_soc;       /* 0x015B */
    int   dist;          /* 0x0131 */
    float power_kw;      /* 估算值 */
    float odometer;      /* 0x01A6 */
    float batt_v;        /* 0x0142 */
    int   throttle;      /* 0x0111 */

    /* BMS DID (7A1→7A9) */
    float bms_total_v;   /* 0xF228 */
    float bms_max_cell;  /* 0xF250 */
    float bms_min_cell;  /* 0xF251 */
    int   bms_max_temp;  /* 0xF252 */
    int   bms_min_temp;  /* 0xF253 */
    int   bms_max_pos;   /* 0xF254 */
    int   bms_min_pos;   /* 0xF255 */
    int   bms_remain_wh; /* 0xF229 */

    /* SGCM DID (7E7→7EF) */
    int   sgcm_rpm;      /* 0x0808 */
    float sgcm_current;  /* 0x0809 A相 */
    float sgcm_current_b;/* 0x080A B相 */
    int   sgcm_rated;    /* 0x0812 */
} obd_data_t;

void obd_init(void);
void obd_start(void);
void obd_stop(void);
void obd_update(void);
const obd_data_t *obd_get_data(void);

#ifdef __cplusplus
}
#endif

#endif
