#pragma once
#include <pebble.h>

// New memo; unsent text is kept as a draft when leaving
void compose_window_push_new(bool start_dictation);

// Continue editing a saved draft
void compose_window_push_draft(int draft_index);

// Comment on the memo with the given name
void compose_window_push_comment(const char *memo_name);
