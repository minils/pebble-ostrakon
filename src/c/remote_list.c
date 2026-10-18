#include "remote_list.h"
#include "toast.h"

static void finish(RemoteList *list) {
  comm_finish();
  list->loading = false;
}

static void response_handler(DictionaryIterator *iter, void *context) {
  RemoteList *list = context;

  if (!iter) {
    list->loading = false;
    list_window_set_status(list->window, "No response from phone");
    return;
  }
  if (comm_response_failed(iter)) {
    finish(list);
    list_window_set_status(list->window, comm_response_error(iter));
    return;
  }

  // the first message announces the number of items
  int32_t count = comm_get_int(iter, MESSAGE_KEY_LIST_COUNT, -1);
  if (count >= 0) {
    // the phone adds a note if the list comes from its offline cache
    if (dict_find(iter, MESSAGE_KEY_TEXT)) {
      toast_show_quiet(comm_response_error(iter));
    }
    list->expected = count < list->capacity ? count : list->capacity;
    if (list->expected == 0) {
      finish(list);
      list_window_set_status(list->window, list->empty_text);
    }
    return;
  }

  int32_t index = comm_get_int(iter, MESSAGE_KEY_ITEM_INDEX, -1);
  if (index < 0 || index >= list->capacity) {
    return;
  }
  RemoteListItem *item = &list->items[index];
  comm_copy_string(iter, MESSAGE_KEY_ITEM_ID, item->id, sizeof(item->id));
  comm_copy_string(iter, MESSAGE_KEY_ITEM_TITLE, item->title,
                   sizeof(item->title));
  comm_copy_string(iter, MESSAGE_KEY_ITEM_SUBTITLE, item->subtitle,
                   sizeof(item->subtitle));

  // items arrive in order, so they can be shown while the rest loads
  list->count = index + 1;
  if (list->count >= list->expected) {
    finish(list);
  }
  list_window_reload(list->window);
}

void remote_list_load(RemoteList *list, CommAction action, const char *target) {
  list->count = 0;
  list->expected = 0;
  list_window_set_status(list->window, "Loading...");

  list->loading = comm_request(action, target, NULL, response_handler, list);
  if (!list->loading) {
    list_window_set_status(list->window, "Phone not connected");
  }
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

uint16_t remote_list_get_count(void *context) {
  RemoteList *list = context;
  return list->count;
}

void remote_list_get_row(uint16_t index, const char **title,
                         const char **subtitle, void *context) {
  RemoteList *list = context;
  *title = list->items[index].title;
  *subtitle = list->items[index].subtitle;
}
