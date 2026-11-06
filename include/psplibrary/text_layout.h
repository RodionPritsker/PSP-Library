#ifndef PSPLIBRARY_TEXT_LAYOUT_H
#define PSPLIBRARY_TEXT_LAYOUT_H

#include <stddef.h>
#include <stdint.h>

size_t pspl_utf8_next(const char *text, size_t index, uint32_t *codepoint);
size_t pspl_utf8_length(const char *text);
size_t pspl_utf8_truncate(const char *text, size_t max_bytes);

#endif
