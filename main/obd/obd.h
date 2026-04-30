/* obd.h */
#ifndef OBD_H
#define OBD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* 标准 OBD-II PID 数据 */
    int   rpm;          /* 发动机转速 0-8000 */
    int   speed;        /* 车速 0-200 km/h */
    int   coolant;      /* 冷却液温度 °C */
    int   oil;          /* 机油温度/油量 0-100 */
    float epa_soc;      /* 电池 SOC 0-100% */
    int   dist;         /* 续航里程 km */
    float power_kw;     /* 输出功率 kw */
    float odometer;     /* 总里程 km */
    float batt_v;       /* 电池电压 V */
    int   throttle;     /* 节气门位置 0-100% */

    /* SGCM 电机数据 */
    int   sgcm_rpm;     /* 电机转速 */
    float sgcm_current; /* 电机电流 A */
    int   sgcm_rated;   /* 额定值 */
    float sgcm_current_b; /* B相电流 A */

    /* BMS 电池数据 */
    float bms_total_v;  /* 总电压 V */
    float bms_max_cell; /* 最高单体电压 V */
    float bms_min_cell; /* 最低单体电压 V */
    int   bms_max_temp; /* 最高温度 °C */
    int   bms_min_temp; /* 最低温度 °C */
    int   bms_max_pos;  /* 最高单体位置 */
    int   bms_min_pos;  /* 最低单体位置 */
    int   bms_remain_wh;/* 剩余容量 Wh */
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