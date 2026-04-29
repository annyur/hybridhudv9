/* setting.c — Setting 界面业务 */
#include "setting.h"
#include <stdbool.h>

static bool s_active = false;

void setting_enter(void) { s_active = true; }
void setting_exit(void)  { s_active = false; }
void setting_update(void) { }