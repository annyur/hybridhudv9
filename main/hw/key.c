/* key.c — PWR + BOOT 按键轮询（消抖后触发）
 * 按键功能：
 *   BOOT 键 ── 短按打开 Bluetooth 界面，长按(>2秒)开关机
 *   PWR 键 ── race 界面 G-force 归零
 */
#include "key.h"
#include "screen.h"
#include "bsp/esp32_s3_touch_amoled_1_75.h"
#include "esp_io_expander.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "KEY";

#define BOOT_GPIO    GPIO_NUM_0      /* BOOT 键直连 GPIO */
#define PWR_PIN_MASK (1ULL << 4)    /* TCA9554 EXIO4 */

#define DEBOUNCE_CNT     5          /* 连续 5 次确认（5×20ms = 100ms） */
#define BOOT_COOLDOWN_MS 2000      /* 开机冷静期 2 秒 */
#define LONG_PRESS_MS    2000      /* 长按阈值 2 秒 */

static esp_io_expander_handle_t s_io = NULL;
static uint32_t s_init_ms = 0;       /* key_init() 调用时间 */
static bool s_cooldown_done = false; /* 冷静期是否结束 */
static bool s_pwr_baseline = false;  /* 冷静期后 BOOT 是否按下（基线） */

/* ---------- BOOT 键专用状态机 ---------- */
typedef enum {
    BOOT_IDLE = 0,        /* 空闲，等待按下 */
    BOOT_DEBOUNCE,        /* 消抖中 */
    BOOT_PRESSED,         /* 已按下，等待释放或长按判定 */
    BOOT_LONG_PRESSED     /* 已触发长按 */
} boot_state_t;

static boot_state_t s_boot_state = BOOT_IDLE;
static uint32_t s_boot_press_ms = 0;  /* 按下时刻 */
static int s_boot_debounce_cnt = 0;   /* 消抖计数 */

/* ---------- 通用按键状态机 ---------- */
typedef struct {
    int  stable;        /* stable: 0=释放 1=按下 */
    int  debounce;      /* debounce: 消抖计数 */
    bool triggered;     /* triggered: 已触发, 等待释放 */
} key_state_t;

static key_state_t s_pwr = {0};

void key_init(void)
{
    s_init_ms = lv_tick_get();
    s_cooldown_done = false;
    s_pwr_baseline = false;
    s_boot_state = BOOT_IDLE;

    /* GPIO0: BOOT 键, 输入上拉 */
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BOOT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BOOT gpio_config failed: %d", err);
    } else {
        ESP_LOGI(TAG, "BOOT key GPIO%d configured, level=%d", BOOT_GPIO, gpio_get_level(BOOT_GPIO));
    }

    /* TCA9554 EXIO4: PWR 键, BSP 初始化 */
    s_io = bsp_io_expander_init();
    if (s_io == NULL) {
        ESP_LOGE(TAG, "TCA9554 init failed, PWR key unavailable");
    } else {
        /* 显式设置 EXIO4 为输入 (TCA9554 默认即为输入) */
        esp_io_expander_set_dir(s_io, PWR_PIN_MASK, IO_EXPANDER_INPUT);
        ESP_LOGI(TAG, "PWR key (TCA9554 EXIO4) ready");
    }
}

/* read_boot_raw: 读取 BOOT 键, true=按下(低电平) */
static bool read_boot_raw(void)
{
    return gpio_get_level(BOOT_GPIO) == 0;
}

static bool read_pwr_raw(void)
{
    if (!s_io) return false;
    uint32_t level = 0;
    if (esp_io_expander_get_level(s_io, PWR_PIN_MASK, &level) != ESP_OK) {
        return false;
    }
    /* bit4=0 表示按下 (低电平有效) */
    return (level & PWR_PIN_MASK) == 0;
}

/* debounce_process: 消抖状态机
 * 返回 true = 刚刚确认按下 */
static bool debounce_process(bool raw, key_state_t *st)
{
    if (raw) {
        if (st->debounce < DEBOUNCE_CNT) {
            st->debounce++;
            if (st->debounce >= DEBOUNCE_CNT && !st->stable) {
                st->stable = 1;
                if (!st->triggered) {
                    st->triggered = true;
                    return true;  /* 首次确认按下 */
                }
            }
        }
    } else {
        st->debounce = 0;
        st->stable = 0;
        st->triggered = false;
    }
    return false;
}

/* BOOT 键专用状态机处理 */
static void boot_key_task(uint32_t now)
{
    bool boot_now = read_boot_raw();

    switch (s_boot_state) {
        case BOOT_IDLE:
            if (boot_now) {
                /* 检测到按下，开始消抖 */
                s_boot_debounce_cnt = 0;
                s_boot_state = BOOT_DEBOUNCE;
                ESP_LOGD(TAG, "BOOT: IDLE → DEBOUNCE");
            }
            break;

        case BOOT_DEBOUNCE:
            if (boot_now) {
                s_boot_debounce_cnt++;
                if (s_boot_debounce_cnt >= DEBOUNCE_CNT) {
                    /* 消抖完成，进入按下状态 */
                    s_boot_press_ms = now;
                    s_boot_state = BOOT_PRESSED;
                    ESP_LOGD(TAG, "BOOT: DEBOUNCE → PRESSED (press_ms=%lu)", now);
                }
            } else {
                /* 按键释放，忽略（抖动） */
                s_boot_debounce_cnt = 0;
                s_boot_state = BOOT_IDLE;
                ESP_LOGD(TAG, "BOOT: DEBOUNCE → IDLE (released)");
            }
            break;

        case BOOT_PRESSED:
            /* 检查长按是否触发 */
            if ((now - s_boot_press_ms) >= LONG_PRESS_MS) {
                ESP_LOGI(TAG, "BOOT long press (>%dms) → restart", LONG_PRESS_MS);
                s_boot_state = BOOT_LONG_PRESSED;
                esp_restart();
                break;
            }

            /* 检查是否释放 */
            if (!boot_now) {
                /* 短按触发：打开 Bluetooth 界面 */
                ESP_LOGI(TAG, "BOOT short press → switch to BLUETOOTH");
                screen_request_switch(SCREEN_BLUETOOTH);
                s_boot_state = BOOT_IDLE;
                s_boot_press_ms = 0;
            }
            break;

        case BOOT_LONG_PRESSED:
            /* 不可能到达这里（重启后），但防止意外 */
            if (!boot_now) {
                s_boot_state = BOOT_IDLE;
            }
            break;
    }
}

void key_poll(void)
{
    uint32_t now = lv_tick_get();

    /* ========== 开机冷静期 ========== */
    if (!s_cooldown_done) {
        if ((now - s_init_ms) < BOOT_COOLDOWN_MS) {
            /* 冷静期内清零状态, 不响应按键 */
            s_pwr.debounce = s_pwr.stable = s_pwr.triggered = 0;
            s_boot_state = BOOT_IDLE;
            return;
        }
        /* 冷静期结束: 记录 BOOT 基线 */
        s_cooldown_done = true;
        s_pwr_baseline = read_boot_raw();
        if (s_pwr_baseline) {
            ESP_LOGW(TAG, "cooldown end, BOOT still pressed (baseline), will ignore until release");
        } else {
            ESP_LOGI(TAG, "cooldown end, BOOT released, key polling active");
        }
    }

    /* ── BOOT 键：短按打开 Bluetooth，长按(>2秒)开关机 ── */
    if (s_pwr_baseline) {
        if (!read_boot_raw()) {
            ESP_LOGI(TAG, "BOOT released, baseline cleared, now active");
            s_pwr_baseline = false;
            s_boot_state = BOOT_IDLE;
        }
    } else {
        boot_key_task(now);
    }

    /* ── PWR 键：仅在 Race 界面归零 G-force ── */
    bool pwr_raw = read_pwr_raw();
    if (debounce_process(pwr_raw, &s_pwr)) {
        if (screen_current() == SCREEN_RACE) {
            ESP_LOGI(TAG, "PWR pressed in RACE → G-force zero");
            extern void race_G_zero(void);
            race_G_zero();
        }
    }
}
