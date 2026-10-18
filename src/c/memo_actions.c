#include "memo_actions.h"
#include "actions.h"
#include "comm.h"
#include "compose_window.h"
#include "memo_list.h"
#include "memo_window.h"
#include "toast.h"
#include "ui.h"

enum {
  MemoActionOpen = 1,
  MemoActionArchive,
  MemoActionDelete,
  MemoActionComment,
};

static char s_name[COMM_ID_SIZE];
static int s_pending_action;

static void modify_handler(DictionaryIterator *iter, void *context) {
  comm_finish();

  if (!iter) {
    toast_show("No response from phone", false);
    return;
  }
  if (comm_response_failed(iter)) {
    toast_show(comm_response_error(iter), false);
    return;
  }

  // the memo is gone from the normal list either way
  memo_window_close(s_name);
  memo_list_remove(s_name);
  toast_show(s_pending_action == MemoActionArchive ? "Archived" : "Deleted",
             true);
}

static void modify(CommAction action) {
  if (!comm_request(action, s_name, NULL, modify_handler, NULL)) {
    toast_show("Phone not connected", false);
  }
}

static void action_chosen(int id, void *context) {
  switch (id) {
  case MemoActionOpen:
    memo_window_push(s_name);
    break;
  case MemoActionArchive:
    s_pending_action = id;
    modify(CommActionArchive);
    break;
  case MemoActionDelete:
    s_pending_action = id;
    modify(CommActionDelete);
    break;
  case MemoActionComment:
    compose_window_push_comment(s_name);
    break;
  }
}

void memo_actions_open(const char *memo_name, bool include_open) {
  static const ActionItem items[] = {
      {.label = "Open", .id = MemoActionOpen},
      {.label = "Archive", .id = MemoActionArchive},
      {.label = "Delete", .id = MemoActionDelete,
       .confirm_label = "Yes, delete"},
      {.label = "Comment", .id = MemoActionComment},
  };

  ui_copy_truncated(s_name, memo_name, sizeof(s_name));
  if (include_open) {
    actions_open(items, ARRAY_LENGTH(items), action_chosen, NULL);
  } else {
    actions_open(items + 1, ARRAY_LENGTH(items) - 1, action_chosen, NULL);
  }
}
