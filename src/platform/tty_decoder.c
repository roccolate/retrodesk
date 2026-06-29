#include "platform/tty_decoder.h"

#include <stdint.h>
#include <string.h>

void tty_decoder_init(TtyDecoder *decoder) {
    if (!decoder) return;
    memset(decoder, 0, sizeof(*decoder));
    decoder->last_mouse_x = -1;
    decoder->last_mouse_y = -1;
}

static void tty_decoder_consume(TtyDecoder *decoder, size_t used) {
    if (!decoder || used == 0 || decoder->pending_len == 0) return;
    if (used >= decoder->pending_len) {
        decoder->pending_len = 0;
        return;
    }
    memmove(decoder->pending, decoder->pending + used, decoder->pending_len - used);
    decoder->pending_len -= used;
}

void tty_decoder_append(TtyDecoder *decoder, const unsigned char *bytes, size_t len) {
    if (!decoder || !bytes || len == 0) return;
    size_t space = sizeof(decoder->pending) - decoder->pending_len;
    if (space == 0) return;
    if (len > space) len = space;
    memcpy(decoder->pending + decoder->pending_len, bytes, len);
    decoder->pending_len += len;
}

static bool tty_parse_uint(const unsigned char *buf, size_t len, size_t *idx,
                           int *out_value) {
    if (!buf || !idx || !out_value || *idx >= len) return false;
    size_t i = *idx;
    int value = 0;
    bool has_digit = false;
    while (i < len && buf[i] >= '0' && buf[i] <= '9') {
        has_digit = true;
        int digit = (int)(buf[i] - '0');
        if (value > (INT32_MAX - digit) / 10) {
            /* Overflow — consume remaining digits and clamp. */
            while (i < len && buf[i] >= '0' && buf[i] <= '9') ++i;
            *idx = i;
            *out_value = INT32_MAX;
            return true;
        }
        value = (value * 10) + digit;
        ++i;
    }
    if (!has_digit) return false;
    *idx = i;
    *out_value = value;
    return true;
}

static bool tty_decoder_starts_mouse(const TtyDecoder *decoder) {
    return decoder->pending_len >= 3 && decoder->pending[0] == 0x1b &&
           decoder->pending[1] == '[' && decoder->pending[2] == '<';
}

static bool tty_decoder_decode_mouse(TtyDecoder *decoder, RetroEvent *out_event) {
    if (!decoder || !out_event) return false;
    if (decoder->pending_len < 6) return false;
    if (decoder->pending[0] != 0x1b || decoder->pending[1] != '[' ||
        decoder->pending[2] != '<') {
        return false;
    }

    size_t term_idx = 0;
    for (size_t i = 3; i < decoder->pending_len; ++i) {
        if (decoder->pending[i] == 'M' || decoder->pending[i] == 'm') {
            term_idx = i;
            break;
        }
    }
    if (term_idx == 0) {
        if (decoder->pending_len == sizeof(decoder->pending)) {
            tty_decoder_consume(decoder, 1);
        }
        return false;
    }

    size_t idx = 3;
    int btn = 0;
    int x = 0;
    int y = 0;
    if (!tty_parse_uint(decoder->pending, term_idx, &idx, &btn)) {
        tty_decoder_consume(decoder, term_idx + 1);
        return false;
    }
    if (idx >= term_idx || decoder->pending[idx] != ';') {
        tty_decoder_consume(decoder, term_idx + 1);
        return false;
    }
    idx++;
    if (!tty_parse_uint(decoder->pending, term_idx, &idx, &x)) {
        tty_decoder_consume(decoder, term_idx + 1);
        return false;
    }
    if (idx >= term_idx || decoder->pending[idx] != ';') {
        tty_decoder_consume(decoder, term_idx + 1);
        return false;
    }
    idx++;
    if (!tty_parse_uint(decoder->pending, term_idx, &idx, &y)) {
        tty_decoder_consume(decoder, term_idx + 1);
        return false;
    }
    if (idx != term_idx) {
        tty_decoder_consume(decoder, term_idx + 1);
        return false;
    }

    unsigned char terminator = decoder->pending[term_idx];
    tty_decoder_consume(decoder, term_idx + 1);
    out_event->type = RETRO_EVENT_MOUSE;
    memset(&out_event->data.mouse, 0, sizeof(out_event->data.mouse));

    int mx = x > 0 ? x - 1 : 0;
    int my = y > 0 ? y - 1 : 0;
    out_event->data.mouse.x = mx;
    out_event->data.mouse.y = my;
    if (mx != decoder->last_mouse_x || my != decoder->last_mouse_y) {
        out_event->data.mouse.moved = true;
    }
    decoder->last_mouse_x = mx;
    decoder->last_mouse_y = my;

    bool release = (terminator == 'm');
    bool motion = ((btn & 32) != 0);
    bool wheel = ((btn & 64) != 0);
    int button = btn & 3;

    if (wheel) {
        if ((btn & 1) != 0) {
            out_event->data.mouse.scroll_down = true;
        } else {
            out_event->data.mouse.scroll_up = true;
        }
        return true;
    }

    if (button == 2) {
        if (release) {
            out_event->data.mouse.button3_clicked = true;
        } else {
            out_event->data.mouse.button3_pressed = true;
            if (!motion) out_event->data.mouse.button3_clicked = true;
        }
        return true;
    }

    if (button == 0 || button == 3) {
        if (release || button == 3) {
            out_event->data.mouse.button1_released = true;
            if (!motion) out_event->data.mouse.button1_clicked = true;
        } else {
            out_event->data.mouse.button1_pressed = true;
        }
    }
    return true;
}

bool tty_decoder_decode(TtyDecoder *decoder, const TtyDecoderKeyMap *keys,
                        RetroEvent *out_event) {
    if (!decoder || !keys || !out_event || decoder->pending_len == 0) return false;

    if (tty_decoder_decode_mouse(decoder, out_event)) return true;
    if (decoder->pending_len == 0) return false;

    /* A pending "ESC [ <" prefix must be a (still-incomplete) mouse sequence;
       never reinterpret it as an Escape key. */
    if (tty_decoder_starts_mouse(decoder)) return false;

    unsigned char b0 = decoder->pending[0];
    if (b0 == 0x1b) {
        if (decoder->pending_len >= 3 && decoder->pending[1] == '[') {
            int key_code = 0;
            switch (decoder->pending[2]) {
            case 'A':
                key_code = keys->up;
                break;
            case 'B':
                key_code = keys->down;
                break;
            case 'C':
                key_code = keys->right;
                break;
            case 'D':
                key_code = keys->left;
                break;
            default:
                break;
            }
            if (key_code != 0) {
                tty_decoder_consume(decoder, 3);
                out_event->type = RETRO_EVENT_KEY;
                out_event->data.key.key_code = key_code;
                out_event->data.key.is_printable = false;
                out_event->data.key.ascii = '\0';
                return true;
            }
            /* Unknown CSI sequence — find the terminating byte (0x40-0x7E)
               and consume the entire sequence to avoid leaving garbage. */
            size_t end = 3;
            while (end < decoder->pending_len) {
                unsigned char c = decoder->pending[end];
                if (c >= 0x40 && c <= 0x7E) {
                    tty_decoder_consume(decoder, end + 1);
                    return false; /* sequence consumed, no event emitted */
                }
                ++end;
            }
            /* Incomplete CSI — wait for more data unless buffer is full. */
            if (decoder->pending_len < sizeof(decoder->pending)) return false;
            tty_decoder_consume(decoder, decoder->pending_len);
            return false;
        }
        /* Lone ESC or ESC followed by non-'[' — emit Escape key. */
        tty_decoder_consume(decoder, 1);
        out_event->type = RETRO_EVENT_KEY;
        out_event->data.key.key_code = 27;
        out_event->data.key.is_printable = false;
        out_event->data.key.ascii = '\0';
        return true;
    }

    tty_decoder_consume(decoder, 1);
    out_event->type = RETRO_EVENT_KEY;
    out_event->data.key.key_code = (b0 == '\r') ? '\n' : (int)b0;
    out_event->data.key.is_printable = (b0 >= 32 && b0 <= 126);
    out_event->data.key.ascii = out_event->data.key.is_printable ? (char)b0 : '\0';
    return true;
}
