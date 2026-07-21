#include "core/utf8.h"

static bool is_continuation(unsigned char byte) {
    return (byte & 0xC0u) == 0x80u;
}

bool retro_utf8_encode(uint32_t cp, char out[4], size_t *out_len) {
    if (!out || !out_len || cp > 0x10FFFFu ||
        (cp >= 0xD800u && cp <= 0xDFFFu)) {
        return false;
    }
    if (cp <= 0x7Fu) {
        out[0] = (char)cp;
        *out_len = 1;
    } else if (cp <= 0x7FFu) {
        out[0] = (char)(0xC0u | (cp >> 6));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        *out_len = 2;
    } else if (cp <= 0xFFFFu) {
        out[0] = (char)(0xE0u | (cp >> 12));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));
        *out_len = 3;
    } else {
        out[0] = (char)(0xF0u | (cp >> 18));
        out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[3] = (char)(0x80u | (cp & 0x3Fu));
        *out_len = 4;
    }
    return true;
}

bool retro_utf8_decode(const char *text, size_t len, size_t offset,
                       uint32_t *codepoint, size_t *byte_len) {
    if (!text || !codepoint || !byte_len || offset >= len) return false;
    const unsigned char *s = (const unsigned char *)text;
    unsigned char b0 = s[offset];
    uint32_t cp = 0;
    size_t n = 0;

    if (b0 <= 0x7Fu) {
        cp = b0;
        n = 1;
    } else if (b0 >= 0xC2u && b0 <= 0xDFu) {
        cp = b0 & 0x1Fu;
        n = 2;
    } else if (b0 >= 0xE0u && b0 <= 0xEFu) {
        cp = b0 & 0x0Fu;
        n = 3;
    } else if (b0 >= 0xF0u && b0 <= 0xF4u) {
        cp = b0 & 0x07u;
        n = 4;
    } else {
        return false;
    }

    if (offset + n > len) return false;
    for (size_t i = 1; i < n; ++i) {
        unsigned char byte = s[offset + i];
        if (!is_continuation(byte)) return false;
        cp = (cp << 6) | (uint32_t)(byte & 0x3Fu);
    }

    if ((n == 2 && cp < 0x80u) ||
        (n == 3 && cp < 0x800u) ||
        (n == 4 && cp < 0x10000u) ||
        cp > 0x10FFFFu ||
        (cp >= 0xD800u && cp <= 0xDFFFu)) {
        return false;
    }

    *codepoint = cp;
    *byte_len = n;
    return true;
}

bool retro_utf8_validate(const char *text, size_t len) {
    if (!text && len != 0) return false;
    size_t offset = 0;
    while (offset < len) {
        uint32_t cp = 0;
        size_t n = 0;
        if (!retro_utf8_decode(text, len, offset, &cp, &n)) return false;
        (void)cp;
        offset += n;
    }
    return true;
}

size_t retro_utf8_valid_prefix(const char *text, size_t max_bytes) {
    if (!text) return 0;
    size_t offset = 0;
    while (offset < max_bytes && text[offset] != '\0') {
        size_t available = 0;
        while (available < 4 && offset + available < max_bytes &&
               text[offset + available] != '\0') {
            available++;
        }
        uint32_t cp = 0;
        size_t n = 0;
        if (!retro_utf8_decode(text + offset, available, 0, &cp, &n)) break;
        (void)cp;
        offset += n;
    }
    return offset;
}

size_t retro_utf8_prev(const char *text, size_t offset) {
    if (!text || offset == 0) return 0;
    size_t pos = offset - 1;
    while (pos > 0 && is_continuation((unsigned char)text[pos])) pos--;
    return pos;
}

size_t retro_utf8_next(const char *text, size_t len, size_t offset) {
    if (!text || offset >= len) return len;
    uint32_t cp = 0;
    size_t n = 0;
    if (!retro_utf8_decode(text, len, offset, &cp, &n)) return offset + 1;
    (void)cp;
    return offset + n;
}

int retro_utf8_width(uint32_t cp) {
    if (cp == 0 || cp < 0x20u || (cp >= 0x7Fu && cp < 0xA0u)) return 0;
    if ((cp >= 0x0300u && cp <= 0x036Fu) ||
        (cp >= 0x1AB0u && cp <= 0x1AFFu) ||
        (cp >= 0x1DC0u && cp <= 0x1DFFu) ||
        (cp >= 0x20D0u && cp <= 0x20FFu) ||
        (cp >= 0xFE20u && cp <= 0xFE2Fu)) {
        return 0;
    }
    return 1;
}

size_t retro_utf8_columns(const char *text, size_t len,
                          size_t start, size_t end) {
    if (!text || start > len) return 0;
    if (end > len) end = len;
    if (start > end) start = end;
    size_t columns = 0;
    size_t offset = start;
    while (offset < end) {
        uint32_t cp = 0;
        size_t n = 0;
        if (!retro_utf8_decode(text, len, offset, &cp, &n) || offset + n > end) {
            cp = 0xFFFDu;
            n = 1;
        }
        int width = retro_utf8_width(cp);
        if (width > 0) columns += (size_t)width;
        offset += n;
    }
    return columns;
}

size_t retro_utf8_prefix_bytes(const char *text, size_t len,
                               size_t max_columns) {
    if (!text) return 0;
    size_t offset = 0;
    size_t columns = 0;
    while (offset < len) {
        uint32_t cp = 0;
        size_t n = 0;
        if (!retro_utf8_decode(text, len, offset, &cp, &n)) {
            cp = 0xFFFDu;
            n = 1;
        }
        int width = retro_utf8_width(cp);
        if (width > 0 && columns + (size_t)width > max_columns) break;
        if (width > 0) columns += (size_t)width;
        offset += n;
    }
    return offset;
}
