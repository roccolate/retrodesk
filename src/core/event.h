#ifndef RETRODESK_CORE_EVENT_H
#define RETRODESK_CORE_EVENT_H

#include <stdbool.h>

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
    /* Raw ASCII/byte payload for printable key events. This is unsigned so
       bytes in the 0x80..0xFF range are preserved across platforms where
       plain `char` is signed. Extended bytes are not automatically portable
       printable characters or RetroDesk key chords; callers must interpret
       them according to the active backend/locale/codepage policy. */
    unsigned char ascii;
} RetroKeyEvent;

typedef struct RetroMouseEvent {
    int y;
    int x;
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
