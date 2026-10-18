#include "memo_list.h"
#include "memo_actions.h"
#include "remote_list.h"
#include "ui.h"

#define MEMO_LIST_MAX 20

static RemoteListItem s_items[MEMO_LIST_MAX];
static RemoteList s_list = {
    .items = s_items,
    .capacity = MEMO_LIST_MAX,
    .empty_text = "No memos",
};

static void select_row(uint16_t index, void *context) {
  // an action replaces the pending request, so stop loading for good
  remote_list_cancel(&s_list);
  memo_actions_open(s_items[index].id, true);
}

static void unload(void *context) {
  remote_list_cancel(&s_list);
  s_list.window = NULL;
}

void memo_list_push(const char *tag) {
  if (s_list.window) {
    return;
  }
  s_list.window = list_window_create(
      (ListWindowCallbacks){
          .get_count = remote_list_get_count,
          .get_row = remote_list_get_row,
          .select = select_row,
          .unload = unload,
      },
      &s_list);
  list_window_push(s_list.window);

  if (tag) {
    remote_list_load(&s_list, CommActionListByTag, tag);
  } else {
    remote_list_load(&s_list, CommActionListRecent, NULL);
  }
}

void memo_list_remove(const char *memo_name) {
  if (s_list.window) {
    remote_list_remove(&s_list, memo_name);
  }
}
