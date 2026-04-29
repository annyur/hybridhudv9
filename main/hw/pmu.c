/* pmu.c — AXP2101 电源管理 */
#include "pmu.h"

void pmu_init(void) {}
float pmu_get_batt_voltage(void) { return 0.0f; }
int pmu_get_batt_percent(void) { return 0; }
bool pmu_is_charging(void) { return false; }
