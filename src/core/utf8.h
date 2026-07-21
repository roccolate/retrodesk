#ifndef RETRODESK_CORE_UTF8_H
#define RETRODESK_CORE_UTF8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool retro_utf8_encode(uint32_t codepoint, char out[4], size_t *out_len);
bool retro_utf8_decode(const char *text, size_t len, size_t offset,
                       uint32_t *codepoint, size_t *byte_len);
bool retro_utf8_validate(const char *text, size_t len);
size_t retro_utf8_valid_prefix(const char *text, size_t max_bytes);
size_t retro_utf8_prev(const char *text, size_t offset);
size_t retro_utf8_next(const char *text, size_t len, size_t offset);
int retro_utf8_width(uint32_t codepoint);
size_t retro_utf8_columns(const char *text, size_t len,
                          size_t start, size_t end);
size_t retro_utf8_prefix_bytes(const char *text, size_t len,
                               size_t max_columns);

#endif
