/* conn.c — BLE GATT 连接（OBD ELM327 兼容） */
#include "conn.h"
#include "esp_log.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "os/os_mbuf.h"
#include <string.h>

static const char *TAG = "CONN";

#define RX_BUF_SIZE 512
static uint8_t s_rx_buf[RX_BUF_SIZE];
static int s_rx_len = 0;
static bool s_rx_new = false;

static bool s_connected = false;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_write_handle = 0;
static uint16_t s_notify_handle = 0;
static uint16_t s_cccd_handle = 0;

static uint8_t s_target_addr[6];
static uint8_t s_target_addr_type;

/* OBD ELM327 常见 UUID */
static const ble_uuid16_t svc_uuid_obd   = BLE_UUID16_INIT(0xFFF0);
static const ble_uuid16_t chr_uuid_write = BLE_UUID16_INIT(0xFFF1);
static const ble_uuid16_t chr_uuid_notify= BLE_UUID16_INIT(0xFFF2);
static const ble_uuid16_t dsc_uuid_cccd  = BLE_UUID16_INIT(0x2902);

static int gap_conn_event(struct ble_gap_event *event, void *arg);
static int gatt_svc_event(uint16_t conn_handle, const struct ble_gatt_error *error,
                          const struct ble_gatt_svc *service, void *arg);
static int gatt_chr_event(uint16_t conn_handle, const struct ble_gatt_error *error,
                          const struct ble_gatt_chr *chr, void *arg);
static int gatt_dsc_event(uint16_t conn_handle, const struct ble_gatt_error *error,
                          uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg);
static int gatt_write_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                         struct ble_gatt_attr *attr, void *arg);
static int gatt_subscribe_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                             struct ble_gatt_attr *attr, void *arg);

void conn_init(void)
{
    s_connected = false;
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_write_handle = 0;
    s_notify_handle = 0;
    s_cccd_handle = 0;
    s_rx_len = 0;
    s_rx_new = false;
}

bool conn_is_connected(void) { return s_connected; }

void conn_disconnect(void)
{
    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
}

void conn_connect(const uint8_t *addr, bool public)
{
    if (s_connected) conn_disconnect();

    memcpy(s_target_addr, addr, 6);
    s_target_addr_type = public ? BLE_ADDR_PUBLIC : BLE_ADDR_RANDOM;

    struct ble_gap_conn_params params = {
        .scan_itvl = 0x50,
        .scan_window = 0x30,
        .itvl_min = BLE_GAP_INITIAL_CONN_ITVL_MIN,
        .itvl_max = BLE_GAP_INITIAL_CONN_ITVL_MAX,
        .latency = 0,
        .supervision_timeout = 400,
        .min_ce_len = 0,   /* 旧版宏不存在，直接填 0 */
        .max_ce_len = 0,
    };

    ble_addr_t peer_addr = {
        .type = s_target_addr_type,
    };
    memcpy(peer_addr.val, addr, 6);

    int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &peer_addr, 10000, &params, gap_conn_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "connect failed: %d", rc);
    } else {
        ESP_LOGI(TAG, "connecting...");
    }
}

void conn_poll(void) { }

bool conn_write(const char *data, int len)
{
    if (!s_connected || s_write_handle == 0 || !data || len <= 0) return false;
    int rc = ble_gattc_write_flat(s_conn_handle, s_write_handle, data, len, gatt_write_cb, NULL);
    return (rc == 0);
}

bool conn_rx_ready(void) { return s_rx_new; }

const char *conn_rx_buf(void)
{
    s_rx_new = false;
    return (const char *)s_rx_buf;
}

int conn_rx_len(void) { return s_rx_len; }

void conn_rx_clear(void)
{
    s_rx_len = 0;
    s_rx_new = false;
    memset(s_rx_buf, 0, sizeof(s_rx_buf));
}

static int gap_conn_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT: {
        if (event->connect.status == 0) {
            s_connected = true;
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "connected, handle=%d", s_conn_handle);
            ble_gattc_disc_all_svcs(s_conn_handle, gatt_svc_event, NULL);
        } else {
            ESP_LOGE(TAG, "connect failed, status=%d", event->connect.status);
            s_connected = false;
        }
        return 0;
    }
    case BLE_GAP_EVENT_DISCONNECT: {
        ESP_LOGI(TAG, "disconnected, reason=%d", event->disconnect.reason);
        s_connected = false;
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_write_handle = 0;
        s_notify_handle = 0;
        s_cccd_handle = 0;
        return 0;
    }
    case BLE_GAP_EVENT_NOTIFY_RX: {
        /* v5.5 直接用 event->notify_rx 的字段 */
        if (event->notify_rx.status != 0) return 0;
        if (!event->notify_rx.indication && s_notify_handle &&
            event->notify_rx.attr_handle == s_notify_handle) {
            int len = OS_MBUF_PKTLEN(event->notify_rx.om);
            if (len > 0 && len < RX_BUF_SIZE) {
                os_mbuf_copydata(event->notify_rx.om, 0, len, s_rx_buf);
                s_rx_buf[len] = '\0';
                s_rx_len = len;
                s_rx_new = true;
            }
        }
        return 0;
    }
    }
    return 0;
}

static int gatt_svc_event(uint16_t conn_handle, const struct ble_gatt_error *error,
                          const struct ble_gatt_svc *service, void *arg)
{
    (void)arg;
    if (error->status != 0 && error->status != BLE_HS_EDONE) {
        ESP_LOGE(TAG, "svc disc error: %d", error->status);
        return 0;
    }
    if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "svc done, W=0x%04X N=0x%04X CCCD=0x%04X",
                 s_write_handle, s_notify_handle, s_cccd_handle);
        if (s_notify_handle && s_cccd_handle) {
            uint8_t v[2] = {0x01, 0x00};
            ble_gattc_write_flat(conn_handle, s_cccd_handle, v, 2, gatt_subscribe_cb, NULL);
        }
        return 0;
    }
    if (ble_uuid_cmp(&service->uuid.u, &svc_uuid_obd.u) == 0) {
        ESP_LOGI(TAG, "found OBD service");
        ble_gattc_disc_all_chrs(conn_handle, service->start_handle, service->end_handle,
                                gatt_chr_event, NULL);
    }
    return 0;
}

static int gatt_chr_event(uint16_t conn_handle, const struct ble_gatt_error *error,
                          const struct ble_gatt_chr *chr, void *arg)
{
    (void)arg;
    if (error->status != 0 && error->status != BLE_HS_EDONE) return 0;
    if (error->status == BLE_HS_EDONE) return 0;

    if (ble_uuid_cmp(&chr->uuid.u, &chr_uuid_write.u) == 0) {
        s_write_handle = chr->val_handle;
        ESP_LOGI(TAG, "write chr: 0x%04X", s_write_handle);
    } else if (ble_uuid_cmp(&chr->uuid.u, &chr_uuid_notify.u) == 0) {
        s_notify_handle = chr->val_handle;
        ESP_LOGI(TAG, "notify chr: 0x%04X", s_notify_handle);
        ble_gattc_disc_all_dscs(conn_handle, chr->val_handle, chr->val_handle + 2,
                                gatt_dsc_event, NULL);
    }
    return 0;
}

/* v5.5 回调多了 uint16_t chr_val_handle 参数 */
static int gatt_dsc_event(uint16_t conn_handle, const struct ble_gatt_error *error,
                          uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg)
{
    (void)conn_handle; (void)chr_val_handle; (void)arg;
    if (error->status != 0 && error->status != BLE_HS_EDONE) return 0;
    if (error->status == BLE_HS_EDONE) return 0;

    if (ble_uuid_cmp(&dsc->uuid.u, &dsc_uuid_cccd.u) == 0) {
        s_cccd_handle = dsc->handle;
        ESP_LOGI(TAG, "cccd: 0x%04X", s_cccd_handle);
    }
    return 0;
}

static int gatt_write_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                         struct ble_gatt_attr *attr, void *arg)
{
    (void)conn_handle; (void)attr; (void)arg;
    if (error->status != 0) ESP_LOGE(TAG, "write err: %d", error->status);
    return 0;
}

static int gatt_subscribe_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                             struct ble_gatt_attr *attr, void *arg)
{
    (void)conn_handle; (void)attr; (void)arg;
    if (error->status == 0) ESP_LOGI(TAG, "notify enabled");
    else ESP_LOGE(TAG, "subscribe err: %d", error->status);
    return 0;
}