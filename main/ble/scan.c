/* scan.c — BLE 扫描 */
#include "scan.h"
#include "esp_log.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include <string.h>

static const char *TAG = "SCAN";

#define MAX_DEVICES 16
static scan_device_t s_devices[MAX_DEVICES];
static int s_count = 0;
static bool s_scanning = false;

static int gap_disc_event(struct ble_gap_event *event, void *arg);

void scan_init(void)
{
    memset(s_devices, 0, sizeof(s_devices));
    s_count = 0;
    s_scanning = false;
}

void scan_start(void)
{
    if (s_scanning) return;
    s_count = 0;
    memset(s_devices, 0, sizeof(s_devices));
    s_scanning = true;

    struct ble_gap_disc_params disc_params = {
        .itvl = 0x50,
        .window = 0x30,
        .filter_policy = BLE_HCI_SCAN_FILT_NO_WL,
        .limited = 0,
        .passive = 0,
        .filter_duplicates = 1,
    };

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 10000, &disc_params, gap_disc_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "scan start failed: %d", rc);
        s_scanning = false;
    } else {
        ESP_LOGI(TAG, "scan started");
    }
}

void scan_stop(void)
{
    if (!s_scanning) return;
    ble_gap_disc_cancel();
    s_scanning = false;
    ESP_LOGI(TAG, "scan stopped, %d devices", s_count);
}

void scan_poll(void) { }

int scan_get_count(void) { return s_count; }

const scan_device_t *scan_get_device(int idx)
{
    if (idx < 0 || idx >= s_count) return NULL;
    return &s_devices[idx];
}

bool scan_is_running(void) { return s_scanning; }

/* 从广播数据提取设备名称 */
static void extract_name(const uint8_t *data, uint8_t len, char *out, size_t out_len)
{
    out[0] = '\0';
    if (!data || len == 0) return;

    uint8_t i = 0;
    while (i + 2 < len) {
        uint8_t field_len = data[i];
        if (field_len == 0 || i + field_len >= len) break;
        uint8_t type = data[i + 1];
        if ((type == 0x09 || type == 0x08) && field_len > 1) {
            int name_len = field_len - 1;
            if (name_len >= (int)out_len) name_len = out_len - 1;
            memcpy(out, &data[i + 2], name_len);
            out[name_len] = '\0';
            return;
        }
        i += field_len + 1;
    }
}

static bool addr_exists(const uint8_t *addr)
{
    for (int i = 0; i < s_count; i++) {
        if (memcmp(s_devices[i].addr, addr, 6) == 0) return true;
    }
    return false;
}

static int gap_disc_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        const struct ble_gap_disc_desc *disc = &event->disc;
        if (s_count >= MAX_DEVICES) return 0;
        if (addr_exists(disc->addr.val)) return 0;

        scan_device_t *d = &s_devices[s_count];
        memcpy(d->addr, disc->addr.val, 6);
        d->rssi = disc->rssi;
        extract_name(disc->data, disc->length_data, d->name, sizeof(d->name));

        // 保存有名称或信号较强的设备
        if (strlen(d->name) > 0 || disc->rssi > -80) {
            ESP_LOGI(TAG, "[%d] %s [%02X:%02X:%02X:%02X:%02X:%02X] rssi=%d",
                s_count, d->name,
                d->addr[0], d->addr[1], d->addr[2],
                d->addr[3], d->addr[4], d->addr[5],
                d->rssi);
            s_count++;
        }
        return 0;
    }
    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "scan complete, %d devices", s_count);
        s_scanning = false;
        return 0;
    }
    return 0;
}