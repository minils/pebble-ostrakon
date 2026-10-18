#include <pebble.h>
#include "comm.h"
#include "compose_window.h"
#include "drafts.h"
#include "drafts_window.h"
#include "list_window.h"
#include "memo_list.h"
#include "settings.h"
#include "tag_list.h"
#include "tasks_window.h"

enum {
  MainRowNew,
  MainRowRecent,
  MainRowTasks,
  MainRowTags,
  MainRowDrafts,
  MainRowCount,
};

static ListWindow *s_main_window;
static char s_drafts_subtitle[16];

/* =================================================================== */
/*                          MAIN MENU                                  */
/* =================================================================== */

static uint16_t get_count(void *context) { return MainRowCount; }

static void get_row(uint16_t index, const char **title, const char **subtitle,
                    void *context) {
  switch (index) {
  case MainRowNew:
    *title = "New";
    *subtitle = "Dictate a memo";
    break;
  case MainRowRecent:
    *title = "Recent";
    *subtitle = "Latest memos";
    break;
  case MainRowTasks:
    *title = "Tasks";
    *subtitle = "Checkboxes in memos";
    break;
  case MainRowTags:
    *title = "Tags";
    *subtitle = "Memos by tag";
    break;
  case MainRowDrafts:
    *title = "Drafts";
    *subtitle = s_drafts_subtitle;
    break;
  }
}

static void select_row(uint16_t index, void *context) {
  switch (index) {
  case MainRowNew:
    compose_window_push_new();
    break;
  case MainRowRecent:
    memo_list_push(NULL);
    break;
  case MainRowTasks:
    tasks_window_push();
    break;
  case MainRowTags:
    tag_list_push();
    break;
  case MainRowDrafts:
    drafts_window_push();
    break;
  }
}

static void appear(void *context) {
  int count = drafts_count();
  if (count == 0) {
    snprintf(s_drafts_subtitle, sizeof(s_drafts_subtitle), "None saved");
  } else {
    snprintf(s_drafts_subtitle, sizeof(s_drafts_subtitle), "%d saved", count);
  }
}

/* =================================================================== */
/*                          APP LIFECYCLE                              */
/* =================================================================== */

static void init(void) {
  settings_init();
  comm_init();
  drafts_init();

  s_main_window = list_window_create(
      (ListWindowCallbacks){
          .get_count = get_count,
          .get_row = get_row,
          .select = select_row,
          .appear = appear,
      },
      NULL);
  list_window_push(s_main_window);

  /* ---- Quick launch: start a new memo right away ---- */
  if (launch_reason() == APP_LAUNCH_QUICK_LAUNCH) {
    compose_window_push_new();
  }
}

int main(void) {
  init();
  app_event_loop();
  // windows are destroyed when the app exits and the stack is cleared
}
