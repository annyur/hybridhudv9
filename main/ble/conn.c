/* conn.c — BLE GATT 连接（完全串行发现：svc → chr → dsc） */
#include "conn.h"
#include "esp_log.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "os/os_mbuf.h"
#include <string.h>

static const char *TAG = "CONN";

#define RX_BUF_SIZE 1024
static uint8_t  s_rx_buf[RX_BUF_SIZE];
static int      s_rx_len = 0;
static bool     s_rx_new = false;

static bool     s_connected = false;
static bool     s_connecting = false;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_write_handle = 0;
static uint16_t s_notify_handle = 0;
static uint16_t s_cccd_handle = 0;
static bool     s_notify_enabled = false;

static uint8_t  s_target_addr[6];
static uint8_t  s_target_addr_type;

/* ========== 串行发现状态机 ========== */
typedef enum {
    DISC_SVC = 0,   /* 发现所有服务 */
    DISC_CHR,       /* 逐个服务发现特征 */
    DISC_DSC,       /* 逐个特征发现描述符 */
    DISC_DONE
} disc_state_t;

static disc_state_t s_disc_state = DISC_SVC;

#define MAX_SVCS 8
static struct {
    uint16_t start_handle;
    uint16_t end_handle;
    uint16_t uuid16;
} s_svcs[MAX_SVCS];
static int s_svc_count = 0;
static int s_cur_svc = 0;

#define MAX_CHRS 16
static struct {
    uint16_t val_handle;
    uint16_t def_handle;
    uint8_t  properties;
    uint16_t uuid16;
    uint16_t svc_idx;   /* 属于哪个服务 */
} s_chrs[MAX_CHRS];
static int s_chr_count = 0;
static int s_cur_chr = 0;

/* 前向声明 */
static int gap_conn_event(struct ble_gap_event *event, void *arg);
static uint16_t uuid16_of(const ble_uuid_t *uuid);
static void log_uuid(const ble_uuid_t *uuid, const char *prefix);

static int gatt_svc_event(uint16_t conn_handle, const struct ble_gatt_error *error,
                          const struct ble_gatt_svc *service, void *arg);
static int gatt_chr_event(uint16_t conn_handle, const struct ble_gatt_error *error,
                          const struct ble_gatt_chr *chr, void *arg);
static int gatt_dsc_event(uint16_t conn_handle, const struct ble_gatt_error *error,
                          uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg);

static void disc_chr_start(uint16_t conn_handle);
static void disc_chr_next_svc(uint16_t conn_handle);
static void disc_dsc_start(uint16_t conn_handle);
static void disc_dsc_next_chr(uint16_t conn_handle);
static void disc_finish(void);
static void enable_notify(uint16_t conn_handle, uint16_t cccd_handle);

/* ========== 公共接口 ========== */
void conn_init(void)
{
    s_connected = false; s_connecting = false;
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_write_handle = 0; s_notify_handle = 0; s_cccd_handle = 0;
    s_notify_enabled = false;
    s_rx_len = 0; s_rx_new = false;

    s_disc_state = DISC_SVC;
    s_svc_count = 0; s_cur_svc = 0;
    s_chr_count = 0; s_cur_chr = 0;
    memset(s_svcs, 0, sizeof(s_svcs));
    memset(s_chrs, 0, sizeof(s_chrs));
}

bool conn_is_connected(void) { return s_connected; }

bool conn_is_ready(void)
{
    return s_connected && s_disc_state == DISC_DONE
           && s_write_handle != 0 && s_notify_handle != 0 && s_notify_enabled;
}

const uint8_t *conn_get_connected_addr(void)
{
    return s_connected ? s_target_addr : NULL;
}

void conn_disconnect(void)
{
    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    s_connecting = false;  /* 确保清除连接中状态 */
    s_connected = false;
    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
}

void conn_connect(const uint8_t *addr, uint8_t addr_type)
{
    if (s_connecting) { ESP_LOGW(TAG, "already connecting"); return; }
    if (s_connected) { conn_disconnect(); return; }

    s_connecting = true;
    memcpy(s_target_addr, addr, 6);
    s_target_addr_type = addr_type;

    struct ble_gap_conn_params params = {
        .scan_itvl = 0x50, .scan_window = 0x30,
        .itvl_min = 0x0018, .itvl_max = 0x0030,
        .latency = 0, .supervision_timeout = 500,
        .min_ce_len = 0, .max_ce_len = 0,
    };

    ble_addr_t peer_addr = { .type = addr_type };
    memcpy(peer_addr.val, addr, 6);

    int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &peer_addr, 8000, &params, gap_conn_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "connect failed: %d", rc);
        s_connecting = false;
    } else {
        const char *type_str = (addr_type == BLE_ADDR_PUBLIC) ? "PUBLIC" :
                               (addr_type == BLE_ADDR_RANDOM) ? "RANDOM" : "OTHER";
        ESP_LOGI(TAG, "connecting to %02X:%02X:%02X:%02X:%02X:%02X (%s)",
                 addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], type_str);
    }
}

/* 原始数据接口 */
bool conn_write(const char *data, int len)
{
    if (!s_connected || s_conn_handle == BLE_HS_CONN_HANDLE_NONE || s_write_handle == 0) {
        return false;
    }
    int rc = ble_gattc_write_flat(s_conn_handle, s_write_handle, data, len, NULL, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "write err rc=%d", rc);
        return false;
    }
    return true;
}

bool conn_rx_ready(void) {
    if (s_rx_new) { s_rx_new = false; return true; }
    return false;
}

const char *conn_rx_buf(void) { return (const char *)s_rx_buf; }
int conn_rx_len(void) { return s_rx_len; }
void conn_rx_clear(void) { s_rx_len = 0; s_rx_buf[0] = '\0'; s_rx_new = false; }

/* ========== GAP 事件 ========== */
static int gap_conn_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT: {
        s_connecting = false;
        if (event->connect.status != 0) {
            ESP_LOGE(TAG, "connect failed, status=%d", event->connect.status);
            s_connected = false;
            return 0;
        }
        s_connected = true;
        s_conn_handle = event->connect.conn_handle;
        ESP_LOGI(TAG, "connected, handle=%d", s_conn_handle);

        /* ===== 优化：OBDLink MX+ 固定 handle，跳过 GATT discovery（省 ~3 秒） ===== */
        s_notify_handle = 0x0016;
        s_write_handle  = 0x0019;
        s_cccd_handle   = 0x0017;

        uint8_t v[2] = {0x01, 0x00};
        int rc = ble_gattc_write_flat(s_conn_handle, s_cccd_handle, v, 2, NULL, NULL);
        if (rc == 0) {
            s_notify_enabled = true;
            s_disc_state = DISC_DONE;
            ESP_LOGI(TAG, "hardcode handle OK, skip discovery");
            return 0;
        }

        /* 回退：notify enable 失败时走全量 discovery（兼容其他 BLE-OBD 设备） */
        ESP_LOGW(TAG, "hardcode fail, fallback to discovery");
        s_notify_handle = 0; s_write_handle = 0; s_cccd_handle = 0;
        s_disc_state = DISC_SVC;
        s_svc_count = 0; s_cur_svc = 0;
        s_chr_count = 0; s_cur_chr = 0;
        s_notify_enabled = false;

        rc = ble_gattc_disc_all_svcs(s_conn_handle, gatt_svc_event, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "disc_all_svcs rc=%d", rc);
        }
        return 0;
    }
    case BLE_GAP_EVENT_DISCONNECT: {
        ESP_LOGW(TAG, "disconnected, reason=%d", event->disconnect.reason);
        s_connected = false;
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_write_handle = 0; s_notify_handle = 0; s_cccd_handle = 0;
        s_notify_enabled = false;
        s_disc_state = DISC_SVC;
        return 0;
    }
    case BLE_GAP_EVENT_NOTIFY_RX: {
        int len = OS_MBUF_PKTLEN(event->notify_rx.om);
        if (len > 0 && (s_rx_len + len) < RX_BUF_SIZE - 1) {
            os_mbuf_copydata(event->notify_rx.om, 0, len, s_rx_buf + s_rx_len);
            s_rx_len += len;
            s_rx_buf[s_rx_len] = '\0';
            s_rx_new = true;
            if (s_notify_handle == 0) {
                s_notify_handle = event->notify_rx.attr_handle;
                ESP_LOGI(TAG, "auto-correct notify_handle=0x%04X", s_notify_handle);
            }
        } else if (len > 0) {
            ESP_LOGW(TAG, "RX buffer overflow, drop %d bytes", len);
        }
        return 0;
    }
    case BLE_GAP_EVENT_ENC_CHANGE: {
        ESP_LOGI(TAG, "encryption change, status=%d", event->enc_change.status);
        return 0;
    }
    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        struct ble_sm_io pkey = {0};
        pkey.action = event->passkey.params.action;
        if (pkey.action == BLE_SM_IOACT_NUMCMP) {
            pkey.numcmp_accept = 1;
        } else if (pkey.action == BLE_SM_IOACT_DISP || pkey.action == BLE_SM_IOACT_INPUT) {
            pkey.passkey = 0;
        }
        ble_sm_inject_io(event->passkey.conn_handle, &pkey);
        return 0;
    }
    }
    return 0;
}

/* ========== UUID 辅助 ========== */
static uint16_t uuid16_of(const ble_uuid_t *uuid)
{
    if (uuid->type == BLE_UUID_TYPE_16) return BLE_UUID16(uuid)->value;
    return 0;
}

static void log_uuid(const ble_uuid_t *uuid, const char *prefix)
{
    char buf[64] = "unknown";
    if (uuid->type == BLE_UUID_TYPE_16) {
        snprintf(buf, sizeof(buf), "0x%04X", BLE_UUID16(uuid)->value);
    } else if (uuid->type == BLE_UUID_TYPE_128) {
        uint8_t *v = BLE_UUID128(uuid)->value;
        snprintf(buf, sizeof(buf), "%02X%02X...%02X%02X", v[15], v[14], v[1], v[0]);
    }
    ESP_LOGI(TAG, "%s UUID=%s", prefix, buf);
}

/* ========== 服务发现 ========== */
static int gatt_svc_event(uint16_t conn_handle, const struct ble_gatt_error *error,
                          const struct ble_gatt_svc *service, void *arg)
{
    (void)arg;
    if (error->status != 0 && error->status != BLE_HS_EDONE) {
        ESP_LOGE(TAG, "svc disc error: %d", error->status);
        return 0;
    }
    if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "svc done, count=%d", s_svc_count);
        s_disc_state = DISC_CHR;
        s_cur_svc = 0;
        s_chr_count = 0;
        disc_chr_start(conn_handle);
        return 0;
    }

    uint16_t uuid16 = uuid16_of(&service->uuid.u);
    log_uuid(&service->uuid.u, "svc");
    if (s_svc_count < MAX_SVCS) {
        s_svcs[s_svc_count].start_handle = service->start_handle;
        s_svcs[s_svc_count].end_handle   = service->end_handle;
        s_svcs[s_svc_count].uuid16       = uuid16;
        s_svc_count++;
    }
    return 0;
}

/* ========== 特征发现 ========== */
static void disc_chr_start(uint16_t conn_handle)
{
    s_cur_svc = 0;
    disc_chr_next_svc(conn_handle);
}

static void disc_chr_next_svc(uint16_t conn_handle)
{
    if (s_cur_svc >= s_svc_count) {
        ESP_LOGI(TAG, "chr all done, count=%d", s_chr_count);
        s_disc_state = DISC_DSC;
        s_cur_chr = 0;
        disc_dsc_start(conn_handle);
        return;
    }

    ESP_LOGI(TAG, "chr scan svc[%d] 0x%04X~0x%04X", s_cur_svc,
             s_svcs[s_cur_svc].start_handle, s_svcs[s_cur_svc].end_handle);
    int rc = ble_gattc_disc_all_chrs(conn_handle,
                                     s_svcs[s_cur_svc].start_handle,
                                     s_svcs[s_cur_svc].end_handle,
                                     gatt_chr_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "disc_all_chrs rc=%d, skip svc[%d]", rc, s_cur_svc);
        s_cur_svc++;
        disc_chr_next_svc(conn_handle);
    }
}

static int gatt_chr_event(uint16_t conn_handle, const struct ble_gatt_error *error,
                          const struct ble_gatt_chr *chr, void *arg)
{
    (void)arg;
    if (error->status != 0 && error->status != BLE_HS_EDONE) {
        ESP_LOGE(TAG, "chr disc error: %d", error->status);
        return 0;
    }
    if (error->status == BLE_HS_EDONE) {
        s_cur_svc++;
        disc_chr_next_svc(conn_handle);
        return 0;
    }

    uint16_t uuid16 = uuid16_of(&chr->uuid.u);
    log_uuid(&chr->uuid.u, "chr");
    ESP_LOGI(TAG, "chr val=0x%04X props=0x%02X uuid16=0x%04X", chr->val_handle, chr->properties, uuid16);

    if (s_chr_count < MAX_CHRS) {
        s_chrs[s_chr_count].val_handle = chr->val_handle;
        s_chrs[s_chr_count].def_handle = chr->def_handle;
        s_chrs[s_chr_count].properties = chr->properties;
        s_chrs[s_chr_count].uuid16     = uuid16;
        s_chrs[s_chr_count].svc_idx    = s_cur_svc;
        s_chr_count++;
    }

    bool is_write = (chr->properties & BLE_GATT_CHR_PROP_WRITE) ||
                    (chr->properties & BLE_GATT_CHR_PROP_WRITE_NO_RSP);
    if (is_write || uuid16 == 0xFF15 || uuid16 == 0xFFF1 || uuid16 == 0x2AF1) {
        s_write_handle = chr->val_handle;
        ESP_LOGI(TAG, "write handle = 0x%04X", s_write_handle);
    }

    bool has_notify  = (chr->properties & BLE_GATT_CHR_PROP_NOTIFY) != 0;
    bool has_indicate = (chr->properties & BLE_GATT_CHR_PROP_INDICATE) != 0;
    if (has_notify || has_indicate || uuid16 == 0xFF14 || uuid16 == 0xFFF2 || uuid16 == 0x2AF0) {
        s_notify_handle = chr->val_handle;
        ESP_LOGI(TAG, "notify handle = 0x%04X", s_notify_handle);
    }
    return 0;
}

/* ========== 描述符发现 ========== */
static void disc_dsc_start(uint16_t conn_handle)
{
    s_cur_chr = 0;
    disc_dsc_next_chr(conn_handle);
}

static void disc_dsc_next_chr(uint16_t conn_handle)
{
    while (s_cur_chr < s_chr_count) {
        uint16_t props = s_chrs[s_cur_chr].properties;
        uint16_t uuid16 = s_chrs[s_cur_chr].uuid16;
        bool is_notify = (props & BLE_GATT_CHR_PROP_NOTIFY) || (props & BLE_GATT_CHR_PROP_INDICATE)
                         || uuid16 == 0xFF14 || uuid16 == 0xFFF2 || uuid16 == 0x2AF0;
        if (is_notify) break;
        s_cur_chr++;
    }

    if (s_cur_chr >= s_chr_count) {
        if (!s_notify_enabled && s_notify_handle != 0) {
            uint16_t guess = s_notify_handle + 1;
            ESP_LOGW(TAG, "no cccd found, blind write to 0x%04X", guess);
            enable_notify(conn_handle, guess);
        }
        disc_finish();
        return;
    }

    uint16_t val = s_chrs[s_cur_chr].val_handle;
    uint16_t end = val + 50;
    ESP_LOGI(TAG, "dsc scan chr[%d] val=0x%04X range=0x%04X~0x%04X", s_cur_chr, val, val, end);
    int rc = ble_gattc_disc_all_dscs(conn_handle, val, end, gatt_dsc_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "disc_all_dscs rc=%d, skip chr[%d]", rc, s_cur_chr);
        s_cur_chr++;
        disc_dsc_next_chr(conn_handle);
    }
}

static int gatt_dsc_event(uint16_t conn_handle, const struct ble_gatt_error *error,
                          uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg)
{
    (void)chr_val_handle; (void)arg;
    if (error->status != 0 && error->status != BLE_HS_EDONE) {
        ESP_LOGE(TAG, "dsc disc error: %d", error->status);
        return 0;
    }
    if (error->status == BLE_HS_EDONE) {
        s_cur_chr++;
        disc_dsc_next_chr(conn_handle);
        return 0;
    }

    log_uuid(&dsc->uuid.u, "dsc");
    if (dsc->uuid.u.type == BLE_UUID_TYPE_16 &&
        BLE_UUID16(&dsc->uuid.u)->value == 0x2902) {
        s_cccd_handle = dsc->handle;
        ESP_LOGI(TAG, "found cccd 0x%04X for chr[%d] 0x%04X", s_cccd_handle, s_cur_chr, chr_val_handle);
        enable_notify(conn_handle, s_cccd_handle);
        s_cur_chr = s_chr_count;
    }
    return 0;
}

/* ========== 发现完成（优先 0x18F0 标准协议） ========== */
static void disc_finish(void)
{
    s_disc_state = DISC_DONE;

    uint16_t preferred_notify = 0, preferred_write = 0, preferred_cccd = 0;
    for (int i = 0; i < s_chr_count; i++) {
        if (s_svcs[s_chrs[i].svc_idx].uuid16 == 0x18F0) {
            uint16_t props = s_chrs[i].properties;
            uint16_t uuid16 = s_chrs[i].uuid16;
            if ((props & BLE_GATT_CHR_PROP_NOTIFY) || (props & BLE_GATT_CHR_PROP_INDICATE)
                || uuid16 == 0x2AF0) {
                preferred_notify = s_chrs[i].val_handle;
            }
            if ((props & BLE_GATT_CHR_PROP_WRITE) || (props & BLE_GATT_CHR_PROP_WRITE_NO_RSP)
                || uuid16 == 0x2AF1) {
                preferred_write = s_chrs[i].val_handle;
            }
            if (preferred_cccd == 0 && preferred_notify == s_chrs[i].val_handle) {
                preferred_cccd = s_chrs[i].val_handle + 1;
            }
        }
    }

    if (preferred_notify != 0) {
        s_notify_handle = preferred_notify;
        ESP_LOGI(TAG, "prefer 0x18F0 notify handle 0x%04X", s_notify_handle);
    }
    if (preferred_write != 0) {
        s_write_handle = preferred_write;
        ESP_LOGI(TAG, "prefer 0x18F0 write handle 0x%04X", s_write_handle);
    }
    if (preferred_notify != 0 && s_cccd_handle != preferred_cccd && preferred_cccd != 0) {
        ESP_LOGW(TAG, "cccd mismatch (0x%04X vs expected 0x%04X), re-enable", s_cccd_handle, preferred_cccd);
        enable_notify(s_conn_handle, preferred_cccd);
        s_cccd_handle = preferred_cccd;
    }

    ESP_LOGI(TAG, "disc done, W=0x%04X N=0x%04X CCCD=0x%04X enabled=%d",
             s_write_handle, s_notify_handle, s_cccd_handle, s_notify_enabled);
}

static void enable_notify(uint16_t conn_handle, uint16_t cccd_handle)
{
    uint8_t v[2] = {0x01, 0x00};
    int rc = ble_gattc_write_flat(conn_handle, cccd_handle, v, 2, NULL, NULL);
    ESP_LOGI(TAG, "notify enable at 0x%04X: rc=%d", cccd_handle, rc);
    if (rc == 0) {
        s_notify_enabled = true;
    }
}