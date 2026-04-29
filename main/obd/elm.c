/* elm.c — ELM327 AT 命令 */
#include "elm.h"
#include "ble.h"
#include <string.h>

static char s_rx[512] = "";
static bool s_ready = false;

void elm_init(void)
{
    s_rx[0] = 0;
    s_ready = false;
}

bool elm_tx(const char *cmd)
{
    ble_rx_clear();
    return ble_write(cmd);
}

bool elm_rx_ready(void) { return s_ready; }
const char *elm_rx_buf(void) { return s_rx; }
void elm_rx_clear(void) { s_ready = false; s_rx[0] = 0; }

void elm_poll(uint32_t now_ms)
{
    (void)now_ms;
    if (ble_rx_ready()) {
        strncpy(s_rx, ble_rx_buf(), sizeof(s_rx) - 1);
        s_rx[sizeof(s_rx) - 1] = 0;
        s_ready = true;
    }
}
