#ifndef RETRODESK_PLATFORM_TTY_DECODER_H
#define RETRODESK_PLATFORM_TTY_DECODER_H

#include <stdbool.h>
#include <stddef.h>

#include "core/event.h"

/* Stateful decoder for ANSI/VT escape sequences and SGR mouse reports on
   the raw-TTY input backend. One instance per PlatformBackend. */

typedef struct TtyDecoder {
    int last_mouse_y;
    int last_mouse_x;
    unsigned char pending[64];
    size_t pending_len;
} TtyDecoder;

typedef struct TtyDecoderKeyMap {
    int up;
    int down;
    int left;
    int right;
} TtyDecoderKeyMap;

void tty_decoder_init(TtyDecoder *decoder);
void tty_decoder_append(TtyDecoder *decoder, const unsigned char *bytes, size_t len);
bool tty_decoder_decode(TtyDecoder *decoder, const TtyDecoderKeyMap *keys,
                        RetroEvent *out_event);

#endif
