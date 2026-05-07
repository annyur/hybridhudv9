/* key.c — PWR + BOOT 按键轮询（消抖后触发）
 * 精简版：只保留 race + bluetooth 切换
 *   PWR  键 ── 短按切换 race ↔ bluetooth
 *   BOOT 键 ── race 界面 G-force 归零
 */
#include "key.h"
#include "screen.h"
#include "bsp/esp32_s3_touch_amoled_1_75.h"
#include "esp_io_expander.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "KEY";

#define BOOT_GPIO    GPIO_NUM_0      /* BOOT 键直连 GPIO */
#define PWR_PIN_MASK (1ULL << 4)    /* TCA9554 EXIO4 */

#define DEBOUNCE_CNT     5          /* 连续 5 次确认（5×20ms = 100ms） */
#define BOOT_COOLDOWN_MS 2000      /* 开机冷静期 2 秒 */

static esp_io_expander_handle_t s_io = NULL;
static uint32_t s_init_ms = 0;       /* key_init() 调用时间 */
static bool s_cooldown_done = false; /* 冷静期是否结束 */
static bool s_pwr_baseline = false;  /* 冷静期后 PWR 是否按下（基线） */

/* ---------- 按键状态机 ---------- */
typedef struct {
    int  stable;        /* stable: 0=释放 1=按下 */
    int  debounce;      /* debounce: 消抖计数 */
    bool triggered;     /* triggered: 已触发, 等待释放 */
} key_state_t;

static key_state_t s_boot = {0};
static key_state_t s_pwr  = {0};

void key_init(void)
{
    s_init_ms = lv_tick_get();
    s_cooldown_done = false;
    s_pwr_baseline = false;

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

void key_poll(void)
{
    uint32_t now = lv_tick_get();

    /* ========== 开机冷静期 ========== */
    if (!s_cooldown_done) {
        if ((now - s_init_ms) < BOOT_COOLDOWN_MS) {
            /* 冷静期内清零消抖计数, 不响应按键 */
            s_boot.debounce = s_boot.stable = s_boot.triggered = 0;
            s_pwr.debounce  = s_pwr.stable  = s_pwr.triggered  = 0;
            return;
        }
        /* 冷静期结束: 记录 PWR 基线 */
        s_cooldown_done = true;
        s_pwr_baseline = read_pwr_raw();
        if (s_pwr_baseline) {
            ESP_LOGW(TAG, "cooldown end, PWR still pressed (baseline), will ignore until release");
        } else {
            ESP_LOGI(TAG, "cooldown end, PWR released, key polling active");
        }
    }

    /* ── PWR 键：切换 race / bluetooth ── */
    bool pwr_now = read_pwr_raw();

    if (s_pwr_baseline) {
        if (!pwr_now) {
            ESP_LOGI(TAG, "PWR released, baseline cleared, now active");
            s_pwr_baseline = false;
            s_pwr.debounce = s_pwr.stable = s_pwr.triggered = 0;
        }
    } else {
        if (debounce_process(pwr_now, &s_pwr)) {
            /* 短按: race ↔ bluetooth 切换 */
            screen_id_t next = (screen_current() == SCREEN_RACE) ? SCREEN_BLUETOOTH : SCREEN_RACE;
            ESP_LOGI(TAG, "PWR pressed → switch to %s", (next == SCREEN_RACE) ? "RACE" : "BLUETOOTH");
            screen_switch(next);
        }
    }

    /* ── BOOT 键：仅在 Race 界面归零 G-force ── */
    bool boot_raw = read_boot_raw();
    if (boot_raw) {
        ESP_LOGD(TAG, "BOOT raw=true, db=%d st=%d tr=%d", s_boot.debounce, s_boot.stable, s_boot.triggered);
    }
    if (debounce_process(boot_raw, &s_boot)) {
        if (screen_current() == SCREEN_RACE) {
            ESP_LOGI(TAG, "BOOT pressed in RACE → G-force zero");
            extern void race_G_zero(void);
            race_G_zero();
        } else {
            ESP_LOGD(TAG, "BOOT pressed ignored (not in RACE, cur=%d)", screen_current());
        }
    }
}