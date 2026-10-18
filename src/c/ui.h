#pragma once
#include <pebble.h>

/* ---------- Fonts ---------- */
// Larger displays (emery, gabbro) get bigger fonts so text is not tiny
#if PBL_DISPLAY_WIDTH >= 200
#define UI_COMPOSE_FONT_KEY FONT_KEY_GOTHIC_28_BOLD
#define UI_BODY_FONT_KEY FONT_KEY_GOTHIC_28
#else
#define UI_COMPOSE_FONT_KEY FONT_KEY_GOTHIC_24_BOLD
#define UI_BODY_FONT_KEY FONT_KEY_GOTHIC_24
#endif

/* ---------- Colors ---------- */
#define UI_HIGHLIGHT_COLOR PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorBlack)
#define UI_HIGHLIGHT_TEXT_COLOR GColorWhite

// Copies src into dst, cutting at a UTF-8 character boundary if needed
void ui_copy_truncated(char *dst, const char *src, size_t dst_size);

// True if text is empty or contains only whitespace
bool ui_is_blank(const char *text);
