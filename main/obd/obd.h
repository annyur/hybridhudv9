/* obd.h — OBD-II 数据接口
 *
 * 覆盖范围:
 *   - 标准 OBD-II PID (Mode 01)
 *   - BMS DID (UDS 22xx, 7A1→7A9)
 *   - SGCM DID (UDS 22xx, 7E7→7EF)
 *
 * 数据结构: obd_data_t — 全局唯一的实时数据池
 *   - obd.c  写入 PID 数据 (010C/010D/0105 等)
 *   - ecu.c  写入 DID 数据 (F228/F229/0809 等)
 *   - general.c / race.c 只读刷新 UI
 *
 * 非阻塞数据同步:
 *   - OBD 任务收集数据后发送到消息队列
 *   - UI 任务从队列非阻塞接收，队列为空时立即返回
 *   - 实现生产者-消费者模式，避免 UI 阻塞
 *
 * 车辆: Mazda EZ-6 / 长安深蓝 SL03 (VIN: LVRHDCEM3SN021133)
 * 协议: ISO 15765-4 CAN 11-bit 500kbps (ATSP6)
 */
#ifndef OBD_H
#define OBD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- 标准 OBD-II PID 宏定义 ----------
 * 来源: my_car.csv 扫描结果 (55 PID 支持, 14 个有值)
 * 用法: switch(pid) { case PID_RPM: ... } */
#define PID_ENGINE_LOAD              0x04   /* 发动机负荷, 公式: A*100/255 [%] */
#define PID_COOLANT_TEMP             0x05   /* 冷却液温度, 公式: A-40 [°C] */
#define PID_SHORT_FUEL_TRIM_1        0x06   /* 短期燃油修正, 公式: (A-128)*100/128 [%] */
#define PID_LONG_FUEL_TRIM_1         0x07   /* 长期燃油修正, 公式: (A-128)*100/128 [%] */
#define PID_MAP                      0x0B   /* 进气歧管压力, 公式: A [kPa] */
#define PID_RPM                      0x0C   /* 发动机转速, 公式: (A*256+B)/4 [rpm] */
#define PID_SPEED                    0x0D   /* 车速, 公式: A [km/h] */
#define PID_TIMING_ADVANCE           0x0E   /* 点火提前角, 公式: (A-128)/2 [°] */
#define PID_INTAKE_TEMP              0x0F   /* 进气温度, 公式: A-40 [°C] */
#define PID_THROTTLE                 0x11   /* 节气门开度, 公式: A*100/255 [%] */
#define PID_O2S11_VOLTAGE            0x14   /* O2传感器 B1S1 电压, 公式: A*0.005 [V] */
#define PID_O2S12_VOLTAGE            0x15   /* O2传感器 B1S2 电压, 公式: A*0.005 [V] */
#define PID_OBD_STD                  0x1C   /* OBD标准, 枚举值 */
#define PID_RUNTIME                  0x1F   /* 发动机运行时间, 公式: A*256+B [s] */
#define PID_DIST_MIL                 0x21   /* 故障灯后行驶距离, 公式: A*256+B [km] */
#define PID_FUEL_RAIL_PRESS_DIESEL   0x23   /* 燃油轨压, 公式: (A*256+B)*10 [kPa] */
#define PID_EGR_ERR                  0x2D   /* EGR误差, 公式: (A-128)*100/128 [%] */
#define PID_EVAP_PCT                 0x2E   /* 蒸发净化, 公式: A*100/255 [%] */
#define PID_FUEL_LEVEL               0x2F   /* 燃油液位, 公式: A*100/255 [%] */
#define PID_BARO                     0x33   /* 大气压, 公式: A [kPa] */
#define PID_MODULE_VOLTAGE           0x42   /* 控制模块电压, 公式: (A*256+B)*0.001 [V] */
#define PID_ABS_LOAD                 0x43   /* 绝对负荷, 公式: A*100/255 [%] */
#define PID_REL_THROTTLE             0x45   /* 相对节气门, 公式: A*100/255 [%] */
#define PID_AMBIENT_TEMP             0x46   /* 环境温度, 公式: A-40 [°C] */
#define PID_HYBRID_SOC               0x5B   /* 混动电池 SOC, 公式: A*100/255 [%] */
#define PID_ODOMETER                 0xA6   /* 总里程, 公式: (A..D)/10 [km] */

/* ---------- DID (UDS 22xx 服务) 宏定义 ----------
 * 请求格式: 7A1#0322XXXX / 7E7#0322XXXX
 * 正响应: 62 XX XX [data...] */
#define DID_SGCM_RPM              0x0808   /* SGCM 转速, 单位 1rpm */
#define DID_SGCM_CURRENT_A        0x0809   /* SGCM A相电流, 单位 0.1A (有符号) */
#define DID_SGCM_CURRENT_B        0x080A   /* SGCM B相电流, 单位 0.1A (有符号) */
#define DID_SGCM_RATED_POWER      0x0812   /* SGCM 额定功率/状态 */
#define DID_BMS_VOLTAGE           0xF228   /* BMS 总电压, 单位 0.1V */
#define DID_BMS_TOTAL_CURRENT     0xF229   /* BMS 电池总电流, 公式: (raw-6000)*0.1 [A], 偏移6000=0A, 正=放电, 负=充电 */
#define DID_BMS_MAX_CELL_V        0xF250   /* 最高单体电压, 单位 0.001V */
#define DID_BMS_MIN_CELL_V        0xF251   /* 最低单体电压, 单位 0.001V */
#define DID_BMS_MAX_TEMP          0xF252   /* 最高单体温度, 单位 1°C */
#define DID_BMS_MIN_TEMP          0xF253   /* 最低单体温度, 单位 1°C */
#define DID_BMS_MAX_CELL_POS      0xF254   /* 最高单体位置 */
#define DID_BMS_MIN_CELL_POS      0xF255   /* 最低单体位置 */

/* ---------- OBD 实时数据结构 ----------
 * 全局唯一实例, 由 obd.c 和 ecu.c 写入, UI 层只读 */
typedef struct {
    /* === 高频核心 PID (已加入轮询, 200ms 间隔) === */
    int   rpm;            /* 发动机转速 [rpm], PID 0x010C */
    int   speed;          /* 车速 [km/h], PID 0x010D, 经动态补偿 */
    int   coolant;        /* 冷却液温度 [°C], PID 0x0105 */
    int   throttle;       /* 节气门开度 [0~100%], PID 0x0111 */
    float batt_v;         /* 控制模块电压 [V], PID 0x0142 */

    /* === 中低频 PID (已扫描确认, 解析就绪, 按需加入轮询) === */
    float engine_load;    /* 发动机负荷 [%], PID 0x0104 */
    float fuel_trim_short; /* 短期燃油修正 [%], PID 0x0106 */
    float fuel_trim_long; /* 长期燃油修正 [%], PID 0x0107 */
    int   map;            /* 进气歧管压力 [kPa], PID 0x010B */
    float timing_advance; /* 点火提前角 [°], PID 0x010E */
    int   intake_temp;    /* 进气温度 [°C], PID 0x010F */
    float o2s11_voltage;  /* O2传感器 B1S1 电压 [V], PID 0x0114 */
    float o2s12_voltage;  /* O2传感器 B1S2 电压 [V], PID 0x0115 */
    uint8_t obd_std;      /* OBD标准, PID 0x011C */
    int   runtime;        /* 发动机运行时间 [s], PID 0x011F */
    int   dist_mil;       /* 故障灯后距离 [km], PID 0x0121 */
    float fuel_rail_press;/* 燃油轨压 [kPa], PID 0x0123 */
    float egr_err;        /* EGR误差 [%], PID 0x012D */
    float evap_pct;       /* 蒸发净化 [%], PID 0x012E */
    float fuel_level;     /* 燃油液位 [0~100%], PID 0x012F */
    int   baro;           /* 大气压 [kPa], PID 0x0133 */
    int   ambient_temp;   /* 环境温度 [°C], PID 0x0146 */
    int   oil;            /* 机油温度 [°C], PID 0x013C (扫描未激活) */

    /* === 混动/增程专用 PID === */
    float epa_soc;        /* 混动电池 SOC [0~100%], PID 0x015B */
    float odometer;       /* 总里程 [km], PID 0x01A6 */
    int   dist;           /* 故障清除后距离 [km], PID 0x0131 */

    /* === 软件估算值 === */
    float power_kw;       /* 估算功率 [kW]
                           * 优先: bms_total_v * bms_total_current / 1000 (电池总功率, 真实)
                           * 回退: throttle * speed * 0.008 (估算) */
    float avg_kw;         /* 自启动后的平均功率 [kW] */

    /* === BMS DID (UDS 22xx, 7A1→7A9) === */
    float bms_total_v;    /* BMS 总电压 [V], DID 0xF228 */
    float bms_max_cell;   /* 最高单体电压 [V], DID 0xF250 */
    float bms_min_cell;   /* 最低单体电压 [V], DID 0xF251 */
    int   bms_max_temp;   /* 最高单体温度 [°C], DID 0xF252 */
    int   bms_min_temp;   /* 最低单体温度 [°C], DID 0xF253 */
    int   bms_max_pos;    /* 最高单体位置, DID 0xF254 */
    int   bms_min_pos;    /* 最低单体位置, DID 0xF255 */
    float bms_total_current; /* 电池总电流 [A], DID 0xF229
                              * 公式: (raw-6000)*0.1, 偏移6000=0A基准
                              * 正值 = 放电(驱动/空调), 负值 = 充电(动能回收) */

    /* === SGCM DID (UDS 22xx, 7E7→7EF) === */
    int   sgcm_rpm;       /* 发电机/驱动电机转速 [rpm], DID 0x0808 */
    float sgcm_current;   /* A相电流 [A], DID 0x0809 (有符号, 负值=回收) */
    float sgcm_current_b;/* B相电流 [A], DID 0x080A (有符号) */
    int   sgcm_rated;     /* 额定功率/状态, DID 0x0812 */
} obd_data_t;

/* ---------- 公共接口 ---------- */

/* obd_init: 初始化 OBD 状态机 + ecu 轮询器 (开机调用一次) */
void obd_init(void);

/* obd_start: 启动 OBD 轮询 (蓝牙 GATT 就绪后调用)
 * 状态机: ST_INIT_ATZ → ... → ST_PID_RUN (010C010D 复合请求循环) */
void obd_start(void);

/* obd_stop: 停止 OBD 轮询 (蓝牙断开时调用) */
void obd_stop(void);

/* obd_update: 状态机轮询 (每 20ms 在 main 循环中调用)
 * 处理: 命令发送 / 响应解析 / 超时 / ISO-TP 拼接 / ecu_update */
void obd_update(void);

/* obd_is_running: 查询 OBD 是否正在运行 */
bool obd_is_running(void);

/* obd_is_busy: 查询 OBD 是否正在初始化
 * ecu.c 调用此函数避免在 OBD 初始化时发送 DID，防止命令踩踏导致 ELM327 STOPPED */
bool obd_is_busy(void);

/* obd_is_waiting_response: 查询 OBD 是否正在等待 ELM327 响应
 * ECU 调用此函数避免在 OBD 命令响应未回来时发送 DID，防止串口命令踩踏 */
bool obd_is_waiting_response(void);

/* obd_get_data: 获取最新 OBD 数据 (只读, UI 刷新时从这里取数) */
const obd_data_t *obd_get_data(void);

/* obd_get_data_rw: 内部模块使用 (ecu.c 写 DID 数据) */
obd_data_t *obd_get_data_rw(void);

/* ---------- 消息队列接口 (非阻塞数据同步) ---------- */

/* obd_queue_init: 初始化 OBD 数据消息队列 */
void obd_queue_init(void);

/* obd_queue_receive: 从队列非阻塞接收最新 OBD 数据
 * @param data 接收缓冲区
 * @return true 成功接收到数据, false 队列为空 */
bool obd_queue_receive(obd_data_t *data);

/* obd_queue_send: 发送 OBD 数据到队列 (内部使用)
 * @param data 要发送的数据
 * @return true 发送成功, false 队列满 */
bool obd_queue_send(const obd_data_t *data);

/* obd_queue_send_from_obd_update: 在 OBD 更新完成后调用，发送数据到队列 (内部使用) */
void obd_queue_send_from_obd_update(void);

#ifdef __cplusplus
}
#endif

#endif