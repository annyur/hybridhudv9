/* obd.c — OBD 数据轮询 */
#include "obd.h"
#include "elm.h"
#include "ecu.h"
#include "ble.h"
#include <string.h>

static obd_data_t s_data;
static bool s_running = false;

void obd_init(void)
{
    memset(&s_data, 0, sizeof(s_data));
    elm_init();
    ecu_init();
}

void obd_start(void)  { s_running = true; }
void obd_stop(void)   { s_running = false; }

void obd_update(void)
{
    if (!s_running) return;
    /* PID 轮询 + ECU Burst 逻辑写在这里 */
}

const obd_data_t *obd_get_data(void)
{
    return &s_data;
}
