/* ble.c — BLE 连接主控 */
#include "ble.h"
#include "scan.h"
#include "conn.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"

static const char *TAG = "BLE";
static bool s_enabled = false;

static void ble_on_sync(void) { ESP_LOGI(TAG, "nimble sync"); }
static void ble_on_reset(int reason) { ESP_LOGE(TAG, "nimble reset: %d", reason); }

static void ble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_init(void)
{
    ESP_LOGI(TAG, "nimble init");
    nimble_port_init();

    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;  // OBD 无需配对
    ble_hs_cfg.sm_sc = 0;                         // 关闭 Secure Connections

    nimble_port_freertos_init(ble_host_task);

    scan_init();
    conn_init();
}

void ble_enable(void)
{
    if (s_enabled) return;
    s_enabled = true;
    ESP_LOGI(TAG, "enabled");
}

void ble_disable(void)
{
    if (!s_enabled) return;
    s_enabled = false;
    conn_disconnect();
    scan_stop();
    ESP_LOGI(TAG, "disabled");
}

bool ble_is_enabled(void) { return s_enabled; }   /* <-- 新增 */
{
    return s_enabled && conn_is_connected();
}

void ble_update(void)
{
    if (!s_enabled) return;
    scan_poll();
    conn_poll();
}

bool ble_write(const char *data)
{
    if (!s_enabled || !data) return false;
    return conn_write(data, strlen(data));
}

bool ble_rx_ready(void)
{
    if (!s_enabled) return false;
    return conn_rx_ready();
}

const char *ble_rx_buf(void)
{
    if (!s_enabled) return "";
    return conn_rx_buf();
}

void ble_rx_clear(void)
{
    if (!s_enabled) return;
    conn_rx_clear();
}