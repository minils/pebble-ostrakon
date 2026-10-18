#pragma once
#include <pebble.h>

// Opens the action menu for a memo: Open (optional), Archive, Delete, Comment
void memo_actions_open(const char *memo_name, bool include_open);
