#include "core/clipboard.h"

#include <stdlib.h>
#include <string.h>

#include "core/utf8.h"

static char *g_clipboard_text;
static size_t g_clipboard_length;

bool retro_clipboard_set_text(const char *text, size_t length) {
    if ((!text && length != 0) ||
        (length > 0 && memchr(text, '\0', length) != NULL) ||
        !retro_utf8_validate(text, length)) {
        return false;
    }

    char *copy = malloc(length + 1);
    if (!copy) return false;
    if (length > 0) memcpy(copy, text, length);
    copy[length] = '\0';

    free(g_clipboard_text);
    g_clipboard_text = copy;
    g_clipboard_length = length;
    return true;
}

const char *retro_clipboard_text(size_t *length) {
    if (length) *length = g_clipboard_length;
    return g_clipboard_text ? g_clipboard_text : "";
}

bool retro_clipboard_has_text(void) {
    return g_clipboard_text != NULL && g_clipboard_length > 0;
}

void retro_clipboard_clear(void) {
    free(g_clipboard_text);
    g_clipboard_text = NULL;
    g_clipboard_length = 0;
}
