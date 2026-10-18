#include "remote_list.h"
#include "toast.h"
#include "ui.h"

static void finish(RemoteList *list) {
  comm_finish();
  list->loading = false;
}

// Errors replace the empty list; with items shown they become a toast
static void show_error(RemoteList *list, const char *error) {
  if (list->count == 0) {
    list_window_set_status(list->window, error);
  } else {
    toast_show(error, false);
    list_window_reload(list->window);
  }
}

static void response_handler(DictionaryIterator *iter, void *context) {
  RemoteList *list = context;

  if (!iter) {
    list->loading = false;
    show_error(list, "No response from phone");
    return;
  }
  if (comm_response_failed(iter)) {
    finish(list);
    show_error(list, comm_response_error(iter));
    return;
  }

  // the first message of a page announces its number of items
  int32_t count = comm_get_int(iter, MESSAGE_KEY_LIST_COUNT, -1);
  if (count >= 0) {
    // the phone adds a note if the list comes from its offline cache
    if (dict_find(iter, MESSAGE_KEY_TEXT)) {
      toast_show_quiet(comm_response_error(iter));
    }
    int32_t end = list->page_start + count;
    list->expected = end < list->capacity ? end : list->capacity;
    list->has_more = comm_get_int(iter, MESSAGE_KEY_ITEM_FLAGS, 0) != 0 &&
                     end < list->capacity;
    if (list->expected <= list->page_start) {
      finish(list);
      if (list->count == 0) {
        list_window_set_status(list->window, list->empty_text);
      }
      list_window_reload(list->window);
    }
    return;
  }

  int32_t index =
      list->page_start + comm_get_int(iter, MESSAGE_KEY_ITEM_INDEX, -1);
  if (index < list->page_start || index >= list->capacity) {
    return;
  }
  RemoteListItem *item = &list->items[index];
  comm_copy_string(iter, MESSAGE_KEY_ITEM_ID, item->id, sizeof(item->id));
  comm_copy_string(iter, MESSAGE_KEY_ITEM_TITLE, item->title,
                   sizeof(item->title));
  comm_copy_string(iter, MESSAGE_KEY_ITEM_SUBTITLE, item->subtitle,
                   sizeof(item->subtitle));
  item->flags = comm_get_int(iter, MESSAGE_KEY_ITEM_FLAGS, 0);

  // items arrive in order, so they can be shown while the rest loads
  list->count = index + 1;
  if (list->count >= list->expected) {
    finish(list);
  }
  list_window_reload(list->window);
}

static void request_page(RemoteList *list) {
  list->page_start = list->count;
  list->expected = list->count;
  list->loading = comm_request_page(
      list->action, list->target[0] ? list->target : NULL,
      list->text[0] ? list->text : NULL, list->page_start, response_handler,
      list);
  if (!list->loading) {
    show_error(list, "Phone not connected");
  }
}

void remote_list_load(RemoteList *list, CommAction action, const char *target,
                      const char *text) {
  list->count = 0;
  list->has_more = false;
  list->action = action;
  ui_copy_truncated(list->target, target ? target : "", sizeof(list->target));
  ui_copy_truncated(list->text, text ? text : "", sizeof(list->text));
  list_window_set_status(list->window, "Loading...");
  request_page(list);
}

void remote_list_load_more(RemoteList *list) {
  if (list->loading || !list->has_more) {
    return;
  }
  request_page(list);
  list_window_reload(list->window);
}

void remote_list_cancel(RemoteList *list) {
  if (list->loading) {
    finish(list);
  }
}

void remote_list_remove(RemoteList *list, const char *id) {
  for (uint16_t i = 0; i < list->count; i++) {
    if (strcmp(list->items[i].id, id) != 0) {
      continue;
    }
    memmove(&list->items[i], &list->items[i + 1],
            (list->count - i - 1) * sizeof(RemoteListItem));
    list->count--;
    list->expected--;
    if (list->count == 0) {
      list_window_set_status(list->window, list->empty_text);
    }
    list_window_reload(list->window);
    return;
  }
}

bool remote_list_is_more_row(const RemoteList *list, uint16_t index) {
  return index == list->count && list->count > 0 && list->has_more;
}

uint16_t remote_list_get_count(void *context) {
  RemoteList *list = context;
  bool more_row = list->count > 0 && list->has_more;
  return list->count + (more_row ? 1 : 0);
}

void remote_list_get_row(uint16_t index, const char **title,
                         const char **subtitle, void *context) {
  RemoteList *list = context;
  if (remote_list_is_more_row(list, index)) {
    *title = list->loading ? "Loading..." : "Load more";
    *subtitle = NULL;
    return;
  }
  *title = list->items[index].title;
  *subtitle = list->items[index].subtitle;
}
