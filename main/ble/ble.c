/* ble.c — BLE 连接主控 */
#include "ble.h"
#include "scan.h"
#include "conn.h"
#include <string.h>

static bool s_enabled = false;
static bool s_connected = false;

void ble_init(void)
{
    scan_init();
    conn_init();
}

void ble_enable(void)  { s_enabled = true; }
void ble_disable(void) { s_enabled = false; }
bool ble_is_connected(void) { return s_connected; }

void ble_update(void)
{
    if (!s_enabled) return;
    scan_poll();
    conn_poll();
}

bool ble_write(const char *data)
{
    /* NimBLE GATT write */
    (void)data;
    return s_connected;
}

bool ble_rx_ready(void) { return false; }
const char *ble_rx_buf(void) { return ""; }
void ble_rx_clear(void) {}
