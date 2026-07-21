#ifndef RETRODESK_CORE_EVENT_H
#define RETRODESK_CORE_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#include "core/key_chord.h"

/* Backend-neutral event types delivered by the platform layer. */

typedef enum RetroEventType {
    RETRO_EVENT_NONE = 0,
    RETRO_EVENT_KEY,
    RETRO_EVENT_MOUSE,
    RETRO_EVENT_RESIZE,
} RetroEventType;

typedef struct RetroKeyEvent {
    int key_code;
    bool is_printable;
    /* `ascii` is retained for the byte-oriented widget transition. New
       callers should use text_codepoint; unsigned storage prevents bytes
       0x80..0xFF from changing sign across platforms. */
    unsigned char ascii;
    uint32_t text_codepoint;
    /* Backends report only modifiers they can identify reliably. A terminal
       that cannot distinguish a shifted navigation key leaves this as NONE. */
    unsigned int modifiers;
} RetroKeyEvent;

typedef struct RetroMouseEvent {
    int y;
    int x;
    /* Screen coordinates are produced by the platform. The WM fills these
       content-local coordinates before dispatching to an app/widget. */
    int local_y;
    int local_x;
    bool has_local_coordinates;
    bool moved;
    bool button1_pressed;
    bool button1_released;
    bool button1_clicked;
    bool button1_dblclick;
    bool button3_pressed;
    bool button3_clicked;
    bool scroll_up;
    bool scroll_down;
} RetroMouseEvent;

typedef struct RetroResizeEvent {
    int rows;
    int cols;
} RetroResizeEvent;

typedef struct RetroEvent {
    RetroEventType type;
    union {
        RetroKeyEvent key;
        RetroMouseEvent mouse;
        RetroResizeEvent resize;
    } data;
} RetroEvent;

#endif
