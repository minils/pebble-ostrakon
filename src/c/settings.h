#pragma once
#include <pebble.h>

// Settings the watch needs without the phone, pushed from the config page

void settings_init(void);
bool settings_get_vibrate(void);
void settings_set_vibrate(bool vibrate);
