#ifndef RETRODESK_CORE_CLIPBOARD_H
#define RETRODESK_CORE_CLIPBOARD_H

#include <stdbool.h>
#include <stddef.h>

/* Process-local, backend-neutral clipboard. Text is stored as validated UTF-8
   and shared by every RetroDesk app instance. The pointer returned by
   retro_clipboard_text() remains owned by the service and is invalidated by
   the next successful set or clear operation. */

bool retro_clipboard_set_text(const char *text, size_t length);
const char *retro_clipboard_text(size_t *length);
bool retro_clipboard_has_text(void);
void retro_clipboard_clear(void);

#endif
