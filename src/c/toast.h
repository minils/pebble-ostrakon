#pragma once
#include <pebble.h>

// Shows a short message in its own window, which closes after a moment
void toast_show(const char *text, bool success);
