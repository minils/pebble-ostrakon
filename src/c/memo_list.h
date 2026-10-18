#pragma once
#include <pebble.h>

// Recent memos, or only those with the given tag if tag is not NULL
void memo_list_push(const char *tag);

// Removes a memo from the open list, e.g. after it was archived
void memo_list_remove(const char *memo_name);
