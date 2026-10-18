#include "tasks_window.h"
#include "memo_window.h"
#include "remote_list.h"
#include "toast.h"
#include "ui.h"

#define TASKS_MAX 30
#define TASK_DONE 1

#if PBL_DISPLAY_WIDTH >= 200
#define TITLE_FONT FONT_KEY_GOTHIC_24_BOLD
#define DONE_TITLE_FONT FONT_KEY_GOTHIC_24
#define SUBTITLE_FONT FONT_KEY_GOTHIC_18
#define CELL_HEIGHT 52
#define CHECKBOX_SIZE 16
#else
#define TITLE_FONT FONT_KEY_GOTHIC_18_BOLD
#define DONE_TITLE_FONT FONT_KEY_GOTHIC_18
#define SUBTITLE_FONT FONT_KEY_GOTHIC_14
#define CELL_HEIGHT 44
#define CHECKBOX_SIZE 12
#endif

#define MUTED_COLOR PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack)

static RemoteListItem s_items[TASKS_MAX];
static RemoteList s_list = {
    .items = s_items,
    .capacity = TASKS_MAX,
    .empty_text = "No tasks",
};
static int s_toggling = -1;

/* =================================================================== */
/*                          DRAWING                                    */
/* =================================================================== */

static void draw_checkbox(GContext *ctx, GRect box, bool done, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, box, 2);
  if (!done) {
    return;
  }
  GPoint left = GPoint(box.origin.x + 3, box.origin.y + box.size.h / 2);
  GPoint bottom = GPoint(box.origin.x + box.size.w / 2 - 1,
                         box.origin.y + box.size.h - 4);
  GPoint right = GPoint(box.origin.x + box.size.w - 3, box.origin.y + 3);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, left, bottom);
  graphics_draw_line(ctx, bottom, right);
}

static void draw_row(GContext *ctx, const Layer *cell_layer, uint16_t index,
                     void *context) {
  const RemoteListItem *item = &s_items[index];
  GRect bounds = layer_get_bounds(cell_layer);
  bool done = item->flags & TASK_DONE;
  bool highlighted = menu_cell_layer_is_highlighted(cell_layer);

  GColor color = highlighted ? UI_HIGHLIGHT_TEXT_COLOR
                             : (done ? MUTED_COLOR : GColorBlack);
  int16_t x = PBL_IF_ROUND_ELSE(bounds.size.w / 8, 6);
  int16_t text_x = x + CHECKBOX_SIZE + 6;
  int16_t text_w = bounds.size.w - text_x - PBL_IF_ROUND_ELSE(x, 4);
  int16_t title_h = CELL_HEIGHT * 11 / 20;

  draw_checkbox(ctx,
                GRect(x, (title_h - CHECKBOX_SIZE) / 2 + 3, CHECKBOX_SIZE,
                      CHECKBOX_SIZE),
                done, color);

  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, item->title,
                     fonts_get_system_font(done ? DONE_TITLE_FONT : TITLE_FONT),
                     GRect(text_x, -2, text_w, title_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                     NULL);
  graphics_draw_text(ctx, item->subtitle, fonts_get_system_font(SUBTITLE_FONT),
                     GRect(text_x, title_h - 4, text_w, CELL_HEIGHT - title_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft,
                     NULL);
}

/* =================================================================== */
/*                          TOGGLING                                   */
/* =================================================================== */

static void toggle_handler(DictionaryIterator *iter, void *context) {
  comm_finish();
  int index = s_toggling;
  s_toggling = -1;
  if (index < 0 || index >= s_list.count) {
    return;
  }

  RemoteListItem *item = &s_items[index];
  if (!iter || comm_response_failed(iter)) {
    // undo the optimistic change
    item->flags ^= TASK_DONE;
    toast_show(iter ? comm_response_error(iter) : "No response from phone",
               false);
  } else {
    item->flags = comm_get_int(iter, MESSAGE_KEY_ITEM_FLAGS, item->flags);
    ui_vibrate(true);
  }
  list_window_reload(s_list.window);
}

static void select_row(uint16_t index, void *context) {
  // one change at a time, so a failure can be undone on the right task
  if (s_toggling >= 0) {
    return;
  }
  remote_list_cancel(&s_list);

  RemoteListItem *item = &s_items[index];
  // the title lets the phone check that the memo did not change meanwhile
  if (!comm_request(CommActionToggleTask, item->id, item->title,
                    toggle_handler, NULL)) {
    toast_show("Phone not connected", false);
    return;
  }
  s_toggling = index;
  item->flags ^= TASK_DONE; // show the change right away
  list_window_reload(s_list.window);
}

static void select_long_row(uint16_t index, void *context) {
  if (s_toggling >= 0) {
    return;
  }
  remote_list_cancel(&s_list);

  // task ids are "<memo name>#<task index>"
  char name[COMM_ID_SIZE];
  ui_copy_truncated(name, s_items[index].id, sizeof(name));
  char *separator = strrchr(name, '#');
  if (separator) {
    *separator = '\0';
  }
  memo_window_push(name);
}

/* =================================================================== */
/*                          WINDOW                                     */
/* =================================================================== */

static void unload(void *context) {
  remote_list_cancel(&s_list);
  // a pending toggle still completes on the phone, only the result is lost
  if (s_toggling >= 0) {
    comm_finish();
    s_toggling = -1;
  }
  s_list.window = NULL;
}

void tasks_window_push(void) {
  if (s_list.window) {
    return;
  }
  s_list.window = list_window_create(
      (ListWindowCallbacks){
          .get_count = remote_list_get_count,
          .get_row = remote_list_get_row,
          .select = select_row,
          .select_long = select_long_row,
          .draw_row = draw_row,
          .cell_height = CELL_HEIGHT,
          .unload = unload,
      },
      &s_list);
  list_window_push(s_list.window);
  remote_list_load(&s_list, CommActionListTasks, NULL);
}
