/* bluetooth.c — Bluetooth 界面业务 */
#include "bluetooth.h"
#include <stdbool.h>

static bool s_active = false;

void bluetooth_enter(void) { s_active = true; }
void bluetooth_exit(void)  { s_active = false; }
void bluetooth_update(void) { }