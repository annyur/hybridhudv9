/* obd.h — OBD-II 数据接口（标准 PID + BMS DID + SGCM DID） */
#ifndef OBD_H
#define OBD_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OBD 实时数据结构 */
typedef struct {
    /* ===== 标准 OBD-II PID（通过 ELM327 轮询获取） ===== */
    int   rpm;           /* 发动机转速 (0x010C)，单位：rpm */
    int   speed;         /* 车速 (0x010D)，单位：km/h */
    int   coolant;       /* 冷却液温度 (0x0105)，单位：°C */
    int   oil;           /* 机油温度 (0x013C)，单位：°C */
    float epa_soc;       /* 电池电量百分比 (0x015B)，范围 0~100% */
    int   dist;          /* 自故障码清除后的行驶距离 (0x0131)，单位：km */
    float power_kw;      /* 估算功率，单位：kW（正=驱动，负=能量回收） */
    float odometer;      /* 总里程 (0x01A6)，单位：km */
    float batt_v;        /* 电池组总电压 (0x0142)，单位：V */
    int   throttle;      /* 油门踏板开度 (0x0111)，范围 0~100% */
    float avg_kw;        /* 自启动后的平均功率，单位：kW */

    /* ===== BMS DID（通过 UDS 扩展诊断获取，7A1→7A9） ===== */
    float bms_total_v;   /* BMS 总电压 (0xF228)，单位：V */
    float bms_max_cell;  /* 最高单体电压 (0xF250)，单位：V */
    float bms_min_cell;  /* 最低单体电压 (0xF251)，单位：V */
    int   bms_max_temp;  /* 最高单体温度 (0xF252)，单位：°C */
    int   bms_min_temp;  /* 最低单体温度 (0xF253)，单位：°C */
    int   bms_max_pos;   /* 最高单体位置 (0xF254) */
    int   bms_min_pos;   /* 最低单体位置 (0xF255) */
    int   bms_remain_wh; /* 剩余可用能量 (0xF229)，单位：Wh */

    /* ===== SGCM DID（增程器/驱动电机，7E7→7EF） ===== */
    int   sgcm_rpm;      /* 发电机/驱动电机转速 (0x0808)，单位：rpm */
    float sgcm_current;  /* A相电流 (0x0809)，单位：A */
    float sgcm_current_b;/* B相电流 (0x080A)，单位：A */
    int   sgcm_rated;    /* 额定功率/状态 (0x0812) */
} obd_data_t;

/* 初始化 OBD 状态机（开机调用一次） */
void obd_init(void);

/* 启动 OBD 轮询（蓝牙 GATT 就绪后调用，进入 ATZ→ATE0→PID 循环） */
void obd_start(void);

/* 停止 OBD 轮询（蓝牙断开时调用） */
void obd_stop(void);

/* 状态机轮询（每 20ms 在 main 循环中调用，处理收发超时） */
void obd_update(void);

/* 查询 OBD 是否正在运行（返回 true 表示已启动且未停止） */
bool obd_is_running(void);

/* 获取最新 OBD 数据（只读，UI 刷新时从这里取数） */
const obd_data_t *obd_get_data(void);

#ifdef __cplusplus
}
#endif

#endif