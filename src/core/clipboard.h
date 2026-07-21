#ifndef RETRODESK_CORE_CLIPBOARD_H
#define RETRODESK_CORE_CLIPBOARD_H

#include <stdbool.h>
#include <stddef.h>

/* Desktop-owned, backend-neutral clipboard. Stored text is validated UTF-8.
   Returned text remains owned by the service and is invalidated by the next
   successful set or clear operation. */
typedef struct RetroClipboard RetroClipboard;

RetroClipboard *retro_clipboard_create(void);
void retro_clipboard_destroy(RetroClipboard *clipboard);
bool retro_clipboard_set_text(RetroClipboard *clipboard,
                              const char *text, size_t length);
const char *retro_clipboard_text(const RetroClipboard *clipboard,
                                 size_t *length);
bool retro_clipboard_has_text(const RetroClipboard *clipboard);
void retro_clipboard_clear(RetroClipboard *clipboard);

#endif
