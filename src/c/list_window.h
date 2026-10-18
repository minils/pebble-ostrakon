#pragma once
#include <pebble.h>

typedef struct ListWindow ListWindow;

typedef struct {
  uint16_t (*get_count)(void *context);
  void (*get_row)(uint16_t index, const char **title, const char **subtitle,
                  void *context);
  void (*select)(uint16_t index, void *context);
  void (*appear)(void *context);   // optional
  void (*unload)(void *context);   // optional, the ListWindow is freed after
} ListWindowCallbacks;

// A menu window. When there are no rows, the status text is shown instead.
ListWindow *list_window_create(ListWindowCallbacks callbacks, void *context);
void list_window_push(ListWindow *list);
void list_window_remove(ListWindow *list);
void list_window_reload(ListWindow *list);
void list_window_set_status(ListWindow *list, const char *status);
