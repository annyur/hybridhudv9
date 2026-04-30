/* scan.c — NimBLE GAP 扫描 */
#include "scan.h"
#include "esp_log.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include <string.h>

static const char *TAG = "SCAN";

#define MAX_DEVICES 16
static scan_device_t s_devices[MAX_DEVICES];
static int s_count = 0;
static bool s_running = false;

static int gap_scan_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    if (event->type == BLE_GAP_EVENT_DISC) {
        const struct ble_gap_disc_desc *d = &event->disc;

        /* 检查是否已存在 */
        for (int i = 0; i < s_count; i++) {
            if (memcmp(s_devices[i].addr, d->addr.val, 6) == 0) {
                s_devices[i].rssi = d->rssi;
                /* 更新名称（如果有的话） */
                if (d->length_data > 0 && d->data) {
                    uint8_t len = d->data[0];
                    if (len > 1 && len < 32 && d->data[1] == BLE_HS_ADV_TYPE_COMP_NAME) {
                        int name_len = len - 1;
                        if (name_len > 31) name_len = 31;
                        memcpy(s_devices[i].name, &d->data[2], name_len);
                        s_devices[i].name[name_len] = '\0';
                    }
                }
                return 0;
            }
        }

        /* 新增设备 */
        if (s_count < MAX_DEVICES) {
            scan_device_t *dev = &s_devices[s_count];
            memset(dev, 0, sizeof(*dev));
            memcpy(dev->addr, d->addr.val, 6);
            dev->addr_type = d->addr.type;  /* 保存地址类型 */
            dev->rssi = d->rssi;

            /* 解析名称 */
            if (d->length_data > 0 && d->data) {
                uint8_t idx = 0;
                while (idx < d->length_data) {
                    uint8_t len = d->data[idx];
                    if (len == 0 || idx + len >= d->length_data) break;
                    uint8_t type = d->data[idx + 1];
                    if (type == BLE_HS_ADV_TYPE_COMP_NAME || type == BLE_HS_ADV_TYPE_INCOMP_NAME) {
                        int name_len = len - 1;
                        if (name_len > 31) name_len = 31;
                        memcpy(dev->name, &d->data[idx + 2], name_len);
                        dev->name[name_len] = '\0';
                        break;
                    }
                    idx += len + 1;
                }
            }

            ESP_LOGI(TAG, "[%d] %s [%02X:%02X:%02X:%02X:%02X:%02X] type=%d rssi=%d",
                     s_count, dev->name[0] ? dev->name : "unknown",
                     dev->addr[0], dev->addr[1], dev->addr[2],
                     dev->addr[3], dev->addr[4], dev->addr[5],
                     dev->addr_type, dev->rssi);
            s_count++;
        }
    }
    return 0;
}

void scan_init(void)
{
    memset(s_devices, 0, sizeof(s_devices));
    s_count = 0;
    s_running = false;
}

void scan_start(void)
{
    if (s_running) return;
    s_running = true;

    struct ble_gap_disc_params params = {
        .itvl = 0x50,
        .window = 0x30,
        .passive = 0,
        .filter_duplicates = 0,
        .filter_policy = 0,
        .limited = 0,
    };

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 30000, &params, gap_scan_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "scan start failed: %d", rc);
        s_running = false;
    } else {
        ESP_LOGI(TAG, "scan started");
    }
}

void scan_stop(void)
{
    if (!s_running) return;
    s_running = false;
    ble_gap_disc_cancel();
    ESP_LOGI(TAG, "scan stopped, %d devices", s_count);
}

void scan_poll(void) { }

int scan_get_count(void) { return s_count; }

const scan_device_t *scan_get_device(int idx)
{
    if (idx < 0 || idx >= s_count) return NULL;
    return &s_devices[idx];
}

bool scan_is_running(void) { return s_running; }