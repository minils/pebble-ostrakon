#pragma once
#include <pebble.h>

// A scrollable text, used by the compose and memo windows
typedef struct {
  ScrollLayer *scroll_layer;
  TextLayer *text_layer;
} TextView;

// click_config_provider adds the window's own buttons; up and down scroll
void text_view_create(TextView *view, Window *window, const char *font_key,
                      GTextAlignment alignment,
                      ClickConfigProvider click_config_provider);
void text_view_destroy(TextView *view);

// The text is not copied and must stay valid while it is shown
void text_view_set_text(TextView *view, const char *text);
