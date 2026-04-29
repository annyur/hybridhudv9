/* pmu.h — AXP2101 电源管理 */
#ifndef PMU_H
#define PMU_H

#ifdef __cplusplus
extern "C" {
#endif

void pmu_init(void);
float pmu_get_batt_voltage(void);
int pmu_get_batt_percent(void);
bool pmu_is_charging(void);

#ifdef __cplusplus
}
#endif

#endif