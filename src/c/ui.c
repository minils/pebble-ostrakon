#include "ui.h"

void ui_copy_truncated(char *dst, const char *src, size_t dst_size) {
  size_t len = strlen(src);
  if (len >= dst_size) {
    len = dst_size - 1;
    // step back over UTF-8 continuation bytes so no character is split
    while (len > 0 && ((uint8_t)src[len] & 0xC0) == 0x80) {
      len--;
    }
  }
  memcpy(dst, src, len);
  dst[len] = '\0';
}

bool ui_is_blank(const char *text) {
  for (const char *c = text; *c; c++) {
    if (*c != ' ' && *c != '\n' && *c != '\t' && *c != '\r') {
      return false;
    }
  }
  return true;
}
