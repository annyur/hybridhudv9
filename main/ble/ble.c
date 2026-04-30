#include "ble.h"
#include "scan.h"
#include "conn.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"

static const char *TAG = "BLE";
static bool s_enabled = false;

static void on_sync(void)  { ESP_LOGI(TAG, "nimble sync"); }
static void on_reset(int r){ ESP_LOGE(TAG, "nimble reset: %d", r); }

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

    ble_hs_cfg.sync_cb  = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    
    /* 安全配对配置：无 IO 设备，自动配对 */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;  /* 无键盘/显示器 */
    ble_hs_cfg.sm_bonding = 1;                    /* 启用绑定 */
    ble_hs_cfg.sm_mitm = 0;                       /* 不需要 MITM 保护 */
    ble_hs_cfg.sm_sc = 1;                         /* 启用 LE Secure Connections */
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;

    nimble_port_freertos_init(ble_host_task);

    scan_init();
    conn_init();
}

void ble_enable(void)  { s_enabled = true;  ESP_LOGI(TAG, "enabled"); }
void ble_disable(void) { s_enabled = false; conn_disconnect(); scan_stop(); ESP_LOGI(TAG, "disabled"); }
bool ble_is_enabled(void)  { return s_enabled; }
bool ble_is_connected(void){ return s_enabled && conn_is_connected(); }

void ble_update(void)
{
    if (!s_enabled) return;
    scan_poll();
    conn_poll();
}

bool ble_write(const char *d) { return s_enabled && d && conn_write(d, strlen(d)); }
bool ble_rx_ready(void)       { return s_enabled && conn_rx_ready(); }
const char *ble_rx_buf(void)  { return s_enabled ? conn_rx_buf() : ""; }
void ble_rx_clear(void)       { if (s_enabled) conn_rx_clear(); }