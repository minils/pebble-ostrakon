#pragma once
#include <pebble.h>
#include "comm.h"
#include "list_window.h"

typedef struct {
  char id[COMM_ID_SIZE];
  char title[COMM_TITLE_SIZE];
  char subtitle[COMM_SUBTITLE_SIZE];
  int32_t flags; // meaning depends on the list, e.g. task done
} RemoteListItem;

// Items of a list loaded from the phone, shown in a ListWindow
typedef struct {
  RemoteListItem *items;
  uint16_t capacity;
  uint16_t count;
  uint16_t expected;
  bool loading;
  const char *empty_text;
  ListWindow *window;
} RemoteList;

// Clears the list and requests it from the phone
void remote_list_load(RemoteList *list, CommAction action, const char *target);

// Stops loading, e.g. when the window is closed
void remote_list_cancel(RemoteList *list);

// Removes the item with the given id, e.g. after it was archived
void remote_list_remove(RemoteList *list, const char *id);

// ListWindowCallbacks helpers; the context must be the RemoteList
uint16_t remote_list_get_count(void *context);
void remote_list_get_row(uint16_t index, const char **title,
                         const char **subtitle, void *context);
