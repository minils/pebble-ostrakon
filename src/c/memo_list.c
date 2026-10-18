#include "memo_list.h"
#include "memo_actions.h"
#include "remote_list.h"
#include "ui.h"

// three pages of 20 memos
#define MEMO_LIST_MAX 60

static RemoteListItem s_items[MEMO_LIST_MAX];
static RemoteList s_list = {
    .items = s_items,
    .capacity = MEMO_LIST_MAX,
};

static void select_row(uint16_t index, void *context) {
  if (remote_list_is_more_row(&s_list, index)) {
    remote_list_load_more(&s_list);
    return;
  }
  // an action replaces the pending request, so stop loading for good
  remote_list_cancel(&s_list);
  memo_actions_open(s_items[index].id, true);
}

static void unload(void *context) {
  remote_list_cancel(&s_list);
  s_list.window = NULL;
}

static void push(CommAction action, const char *target, const char *text,
                 const char *empty_text) {
  if (s_list.window) {
    return;
  }
  s_list.empty_text = empty_text;
  s_list.window = list_window_create(
      (ListWindowCallbacks){
          .get_count = remote_list_get_count,
          .get_row = remote_list_get_row,
          .select = select_row,
          .unload = unload,
      },
      &s_list);
  list_window_push(s_list.window);
  remote_list_load(&s_list, action, target, text);
}

void memo_list_push_recent(void) {
  push(CommActionListRecent, NULL, NULL, "No memos");
}

void memo_list_push_tag(const char *tag) {
  push(CommActionListByTag, tag, NULL, "No memos");
}

void memo_list_push_search(const char *query) {
  push(CommActionSearch, NULL, query, "No matches");
}

void memo_list_push_pinned(void) {
  push(CommActionListPinned, NULL, NULL, "No pinned memos");
}

void memo_list_remove(const char *memo_name) {
  if (s_list.window) {
    remote_list_remove(&s_list, memo_name);
  }
}

void memo_list_unpinned(const char *memo_name) {
  if (s_list.action == CommActionListPinned) {
    memo_list_remove(memo_name);
  }
}
