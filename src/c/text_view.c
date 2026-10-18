#include "text_view.h"

#define TEXT_VIEW_MAX_HEIGHT 4000
#define TEXT_VIEW_BOTTOM_PADDING 10

static int16_t horizontal_inset(GRect bounds) {
  // keep text away from the curved edges on round displays
  return PBL_IF_ROUND_ELSE(bounds.size.w / 8, 2);
}

void text_view_create(TextView *view, Window *window, const char *font_key,
                      GTextAlignment alignment,
                      ClickConfigProvider click_config_provider) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int16_t inset = horizontal_inset(bounds);

  view->text_layer = text_layer_create(
      GRect(inset, PBL_IF_ROUND_ELSE(bounds.size.h / 10, 0),
            bounds.size.w - 2 * inset, bounds.size.h));
  text_layer_set_text_alignment(view->text_layer, alignment);
  text_layer_set_font(view->text_layer, fonts_get_system_font(font_key));

  view->scroll_layer = scroll_layer_create(bounds);
  scroll_layer_set_callbacks(view->scroll_layer,
                             (ScrollLayerCallbacks){
                                 .click_config_provider = click_config_provider,
                             });
  scroll_layer_add_child(view->scroll_layer,
                         text_layer_get_layer(view->text_layer));
  scroll_layer_set_click_config_onto_window(view->scroll_layer, window);
  layer_add_child(root, scroll_layer_get_layer(view->scroll_layer));
}

void text_view_destroy(TextView *view) {
  text_layer_destroy(view->text_layer);
  scroll_layer_destroy(view->scroll_layer);
}

void text_view_set_text(TextView *view, const char *text) {
  Layer *text_layer = text_layer_get_layer(view->text_layer);
  GRect frame = layer_get_frame(text_layer);
  GRect bounds = layer_get_bounds(scroll_layer_get_layer(view->scroll_layer));

  // measure with plenty of room, then shrink the layer to the text
  frame.size.h = TEXT_VIEW_MAX_HEIGHT;
  layer_set_frame(text_layer, frame);
  text_layer_set_text(view->text_layer, text);
  GSize size = text_layer_get_content_size(view->text_layer);

  frame.size.h = size.h + TEXT_VIEW_BOTTOM_PADDING;
  layer_set_frame(text_layer, frame);

  int16_t content_height = frame.origin.y + frame.size.h;
  if (content_height < bounds.size.h) {
    content_height = bounds.size.h;
  }
  scroll_layer_set_content_size(view->scroll_layer,
                                GSize(bounds.size.w, content_height));
  scroll_layer_set_content_offset(view->scroll_layer, GPointZero, false);
}
