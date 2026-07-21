#ifndef RETRODESK_CORE_CLIPBOARD_H
#define RETRODESK_CORE_CLIPBOARD_H

#include <stdbool.h>
#include <stddef.h>

/* Process-local, backend-neutral clipboard. Text is stored as validated UTF-8
   and shared by every RetroDesk app instance. */

bool retro_clipboard_set_text(const char *text, size_t length);
const char *retro_clipboard_text(size_t *length);
bool retro_clipboard_has_text(void);
void retro_clipboard_clear(void);

#endif
