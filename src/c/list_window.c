#include "list_window.h"
#include "ui.h"

struct ListWindow {
  Window *window;
  MenuLayer *menu_layer;
  ListWindowCallbacks callbacks;
  void *context;
  char status[64];
};

/* =================================================================== */
/*                          MENU CALLBACKS                             */
/* =================================================================== */

static uint16_t get_num_rows(MenuLayer *menu_layer, uint16_t section_index,
                             void *data) {
  ListWindow *list = data;
  uint16_t count = list->callbacks.get_count(list->context);
  return count > 0 ? count : 1; // one row for the status text
}

static void draw_row(GContext *ctx, const Layer *cell_layer,
                     MenuIndex *cell_index, void *data) {
  ListWindow *list = data;
  if (list->callbacks.get_count(list->context) == 0) {
    menu_cell_basic_draw(ctx, cell_layer, list->status, NULL, NULL);
    return;
  }

  const char *title = "";
  const char *subtitle = NULL;
  list->callbacks.get_row(cell_index->row, &title, &subtitle, list->context);
  menu_cell_basic_draw(ctx, cell_layer, title, subtitle, NULL);
}

static void select_click(MenuLayer *menu_layer, MenuIndex *cell_index,
                         void *data) {
  ListWindow *list = data;
  if (cell_index->row < list->callbacks.get_count(list->context)) {
    list->callbacks.select(cell_index->row, list->context);
  }
}

/* =================================================================== */
/*                          WINDOW                                     */
/* =================================================================== */

static void window_load(Window *window) {
  ListWindow *list = window_get_user_data(window);
  Layer *root = window_get_root_layer(window);

  list->menu_layer = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(list->menu_layer, list,
                           (MenuLayerCallbacks){
                               .get_num_rows = get_num_rows,
                               .draw_row = draw_row,
                               .select_click = select_click,
                           });
  menu_layer_set_highlight_colors(list->menu_layer, UI_HIGHLIGHT_COLOR,
                                  UI_HIGHLIGHT_TEXT_COLOR);
  menu_layer_set_click_config_onto_window(list->menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(list->menu_layer));
}

static void window_appear(Window *window) {
  ListWindow *list = window_get_user_data(window);
  if (list->callbacks.appear) {
    list->callbacks.appear(list->context);
  }
  menu_layer_reload_data(list->menu_layer);
}

static void window_unload(Window *window) {
  ListWindow *list = window_get_user_data(window);
  menu_layer_destroy(list->menu_layer);
  if (list->callbacks.unload) {
    list->callbacks.unload(list->context);
  }
  window_destroy(window);
  free(list);
}

ListWindow *list_window_create(ListWindowCallbacks callbacks, void *context) {
  ListWindow *list = calloc(1, sizeof(ListWindow));
  list->callbacks = callbacks;
  list->context = context;

  list->window = window_create();
  window_set_user_data(list->window, list);
  window_set_window_handlers(list->window, (WindowHandlers){
                                               .load = window_load,
                                               .appear = window_appear,
                                               .unload = window_unload,
                                           });
  return list;
}

void list_window_push(ListWindow *list) {
  window_stack_push(list->window, true);
}

void list_window_remove(ListWindow *list) {
  window_stack_remove(list->window, true);
}

void list_window_reload(ListWindow *list) {
  if (list->menu_layer) {
    menu_layer_reload_data(list->menu_layer);
  }
}

void list_window_set_status(ListWindow *list, const char *status) {
  ui_copy_truncated(list->status, status, sizeof(list->status));
  list_window_reload(list);
}
