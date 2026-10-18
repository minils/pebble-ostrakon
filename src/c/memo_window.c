#include "memo_window.h"
#include "comm.h"
#include "memo_actions.h"
#include "text_view.h"
#include "ui.h"

static Window *s_window;
static TextView s_text_view;
static char s_name[COMM_ID_SIZE];
static char s_content[COMM_TEXT_SIZE];
static bool s_loading;

static void show(const char *text) {
  ui_copy_truncated(s_content, text, sizeof(s_content));
  text_view_set_text(&s_text_view, s_content);
}

static void content_handler(DictionaryIterator *iter, void *context) {
  comm_finish();
  s_loading = false;
  if (!iter) {
    show("No response from phone");
  } else if (comm_response_failed(iter)) {
    show(comm_response_error(iter));
  } else {
    comm_copy_string(iter, MESSAGE_KEY_TEXT, s_content, sizeof(s_content));
    text_view_set_text(&s_text_view, s_content);
  }
}

static void select_click_handler(ClickRecognizerRef ref, void *context) {
  if (s_loading) {
    return;
  }
  memo_actions_open(s_name, false);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

static void window_load(Window *window) {
  text_view_create(&s_text_view, window, UI_BODY_FONT_KEY, GTextAlignmentLeft,
                   click_config_provider);
  show("Loading...");
  s_loading = comm_request(CommActionGet, s_name, NULL, content_handler, NULL);
  if (!s_loading) {
    show("Phone not connected");
  }
}

static void window_unload(Window *window) {
  // only cancel our own request, not e.g. a pending archive
  if (s_loading) {
    comm_finish();
    s_loading = false;
  }
  text_view_destroy(&s_text_view);
  window_destroy(s_window);
  s_window = NULL;
}

void memo_window_push(const char *memo_name) {
  if (s_window) {
    return;
  }
  ui_copy_truncated(s_name, memo_name, sizeof(s_name));

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = window_load,
                                           .unload = window_unload,
                                       });
  window_stack_push(s_window, true);
}

void memo_window_close(const char *memo_name) {
  if (s_window && strcmp(s_name, memo_name) == 0) {
    window_stack_remove(s_window, true);
  }
}
