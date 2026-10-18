#include "ui.h"
#include "settings.h"

/* =================================================================== */
/*                          TITLE BAR                                  */
/* =================================================================== */

typedef struct {
  const char *title;
} TitleBarData;

static void title_bar_update(Layer *layer, GContext *ctx) {
  TitleBarData *data = layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, UI_HIGHLIGHT_COLOR);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  GFont font = fonts_get_system_font(PBL_DISPLAY_WIDTH >= 200
                                         ? FONT_KEY_GOTHIC_18_BOLD
                                         : FONT_KEY_GOTHIC_14_BOLD);
  graphics_context_set_text_color(ctx, UI_HIGHLIGHT_TEXT_COLOR);
  // the system fonts have some space above the letters
  graphics_draw_text(ctx, data->title, font,
                     GRect(0, -3, bounds.size.w, bounds.size.h + 3),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
}

Layer *ui_title_bar_create(GRect window_bounds, const char *title) {
  Layer *layer = layer_create_with_data(
      GRect(0, 0, window_bounds.size.w, UI_TITLE_BAR_HEIGHT),
      sizeof(TitleBarData));
  ((TitleBarData *)layer_get_data(layer))->title = title;
  layer_set_update_proc(layer, title_bar_update);
  return layer;
}

/* =================================================================== */
/*                          SELECT HINT                                */
/* =================================================================== */

static void select_hint_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, UI_HIGHLIGHT_COLOR);
  // only the left half of the circle is inside the layer
  graphics_fill_circle(ctx, GPoint(bounds.size.w, bounds.size.h / 2),
                       UI_SELECT_HINT_RADIUS);
}

Layer *ui_select_hint_create(GRect window_bounds) {
  Layer *layer = layer_create(
      GRect(window_bounds.size.w - UI_SELECT_HINT_RADIUS, 0,
            UI_SELECT_HINT_RADIUS, window_bounds.size.h));
  layer_set_update_proc(layer, select_hint_update);
  return layer;
}

/* =================================================================== */
/*                          VIBRATION                                  */
/* =================================================================== */

// shorter than vibes_short_pulse() so the feedback is less intrusive
static const uint32_t SUCCESS_PATTERN[] = {50};
static const uint32_t FAILURE_PATTERN[] = {50, 120, 50};

void ui_vibrate(bool success) {
  if (!settings_get_vibrate()) {
    return;
  }
  VibePattern pattern = success
                            ? (VibePattern){.durations = SUCCESS_PATTERN,
                                            .num_segments =
                                                ARRAY_LENGTH(SUCCESS_PATTERN)}
                            : (VibePattern){.durations = FAILURE_PATTERN,
                                            .num_segments =
                                                ARRAY_LENGTH(FAILURE_PATTERN)};
  vibes_enqueue_custom_pattern(pattern);
}

/* =================================================================== */
/*                          TEXT                                       */
/* =================================================================== */

void ui_copy_truncated(char *dst, const char *src, size_t dst_size) {
  size_t len = strlen(src);
  if (len >= dst_size) {
    len = dst_size - 1;
    // step back over UTF-8 continuation bytes so no character is split
    while (len > 0 && ((uint8_t)src[len] & 0xC0) == 0x80) {
      len--;
    }
  }
  memcpy(dst, src, len);
  dst[len] = '\0';
}

bool ui_is_blank(const char *text) {
  for (const char *c = text; *c; c++) {
    if (*c != ' ' && *c != '\n' && *c != '\t' && *c != '\r') {
      return false;
    }
  }
  return true;
}
