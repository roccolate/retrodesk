#include "core/clipboard.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core/utf8.h"

struct RetroClipboard {
    char *text;
    size_t length;
};

RetroClipboard *retro_clipboard_create(void) {
    return calloc(1, sizeof(RetroClipboard));
}

void retro_clipboard_destroy(RetroClipboard *clipboard) {
    if (!clipboard) return;
    free(clipboard->text);
    clipboard->text = NULL;
    clipboard->length = 0;
    free(clipboard);
}

bool retro_clipboard_set_text(RetroClipboard *clipboard,
                              const char *text, size_t length) {
    if (!clipboard || length == SIZE_MAX || (!text && length != 0) ||
        (length > 0 && memchr(text, '\0', length) != NULL) ||
        !retro_utf8_validate(text, length)) {
        return false;
    }

    char *copy = malloc(length + 1);
    if (!copy) return false;
    if (length > 0) memcpy(copy, text, length);
    copy[length] = '\0';

    free(clipboard->text);
    clipboard->text = copy;
    clipboard->length = length;
    return true;
}

const char *retro_clipboard_text(const RetroClipboard *clipboard,
                                 size_t *length) {
    if (length) *length = clipboard ? clipboard->length : 0;
    return clipboard && clipboard->text ? clipboard->text : "";
}

bool retro_clipboard_has_text(const RetroClipboard *clipboard) {
    return clipboard && clipboard->text && clipboard->length > 0;
}

void retro_clipboard_clear(RetroClipboard *clipboard) {
    if (!clipboard) return;
    free(clipboard->text);
    clipboard->text = NULL;
    clipboard->length = 0;
}
