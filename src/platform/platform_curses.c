#include "platform/platform_backend_internal.h"

#include <string.h>

#include "core/key_chord.h"

/* Translate backend-specific key codes (curses KEY_* macros, whose values
   vary across ncurses and PDCurses) into portable RETRO_KEY_* chords.
   Raw bytes (ASCII and extended ASCII) pass through unchanged. */
static int translate_key_chord(int raw) {
    switch (raw) {
#ifdef KEY_UP
    case KEY_UP:    return RETRO_KEY_UP;
#endif
#ifdef KEY_DOWN
    case KEY_DOWN:  return RETRO_KEY_DOWN;
#endif
#ifdef KEY_LEFT
    case KEY_LEFT:  return RETRO_KEY_LEFT;
#endif
#ifdef KEY_RIGHT
    case KEY_RIGHT: return RETRO_KEY_RIGHT;
#endif
#ifdef KEY_HOME
    case KEY_HOME:  return RETRO_KEY_HOME;
#endif
#ifdef KEY_END
    case KEY_END:   return RETRO_KEY_END;
#endif
#ifdef KEY_PPAGE
    case KEY_PPAGE: return RETRO_KEY_PPAGE;
#endif
#ifdef KEY_NPAGE
    case KEY_NPAGE: return RETRO_KEY_NPAGE;
#endif
#ifdef KEY_DC
    case KEY_DC:    return RETRO_KEY_DC;
#endif
#ifdef KEY_IC
    case KEY_IC:    return RETRO_KEY_IC;
#endif
#ifdef KEY_BTAB
    case KEY_BTAB:  return RETRO_KEY_BTAB;
#endif
#ifdef KEY_F1
    case KEY_F1:    return RETRO_KEY_F1;
#endif
#ifdef KEY_F2
    case KEY_F2:    return RETRO_KEY_F2;
#endif
#ifdef KEY_F3
    case KEY_F3:    return RETRO_KEY_F3;
#endif
#ifdef KEY_F4
    case KEY_F4:    return RETRO_KEY_F4;
#endif
#ifdef KEY_F5
    case KEY_F5:    return RETRO_KEY_F5;
#endif
#ifdef KEY_F6
    case KEY_F6:    return RETRO_KEY_F6;
#endif
#ifdef KEY_F7
    case KEY_F7:    return RETRO_KEY_F7;
#endif
#ifdef KEY_F8
    case KEY_F8:    return RETRO_KEY_F8;
#endif
#ifdef KEY_F9
    case KEY_F9:    return RETRO_KEY_F9;
#endif
#ifdef KEY_F10
    case KEY_F10:   return RETRO_KEY_F10;
#endif
#ifdef KEY_F11
    case KEY_F11:   return RETRO_KEY_F11;
#endif
#ifdef KEY_F12
    case KEY_F12:   return RETRO_KEY_F12;
#endif
    default:        return raw;
    }
}

static bool normalize_mouse(const MEVENT *raw, PlatformBackend *platform,
                            RetroMouseEvent *out) {
    memset(out, 0, sizeof(*out));
    out->y = raw->y;
    out->x = raw->x;

    mmask_t bs = raw->bstate;
    if (bs & BUTTON1_PRESSED) out->button1_pressed = true;
    if (bs & BUTTON1_RELEASED) out->button1_released = true;
    if (bs & BUTTON1_CLICKED) out->button1_clicked = true;
#ifdef BUTTON1_DOUBLE_CLICKED
    if (bs & BUTTON1_DOUBLE_CLICKED) out->button1_dblclick = true;
#endif
#ifdef BUTTON3_PRESSED
    if (bs & BUTTON3_PRESSED) out->button3_pressed = true;
#endif
#ifdef BUTTON3_CLICKED
    if (bs & BUTTON3_CLICKED) out->button3_clicked = true;
#endif
#ifdef BUTTON4_PRESSED
    if (bs & BUTTON4_PRESSED) out->scroll_up = true;
#endif
#ifdef BUTTON5_PRESSED
    if (bs & BUTTON5_PRESSED) out->scroll_down = true;
#endif

    if (out->y != platform->last_mouse_y || out->x != platform->last_mouse_x) {
        out->moved = true;
    }
    platform->last_mouse_y = out->y;
    platform->last_mouse_x = out->x;
    return true;
}

bool platform_init_curses_backend(PlatformBackend *platform,
                                  const PlatformConfig *config) {
    if (!platform) return false;
    if (initscr() == NULL) return false;

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mouseinterval(0);

    platform->uses_curses = true;
    platform->features.keyboard_basic = true;
    platform->features.color = has_colors();
    platform->features.unicode_basic = true;
    bool term_is_linux_vc = platform_is_linux_virtual_console();

#if defined(HAVE_PDCURSES) || defined(_WIN32) || defined(__DJGPP__)
    platform->features.input_backend = INPUT_BACKEND_PDCURSES;
#else
    platform->features.input_backend = INPUT_BACKEND_NCURSES;
#endif

#ifdef KEY_RESIZE
    /* Linux virtual console is treated as keyboard-first for resize semantics. */
    platform->features.resize_events = !term_is_linux_vc;
#else
    platform->features.resize_events = false;
#endif

    mmask_t mouse_request = ALL_MOUSE_EVENTS;
#ifdef REPORT_MOUSE_POSITION
    mouse_request |= REPORT_MOUSE_POSITION;
#endif
    mmask_t available_mask = mousemask(mouse_request, NULL);
    if (term_is_linux_vc) {
        mousemask(0, NULL);
        available_mask = 0;
    }
    platform->features.mouse_basic = (available_mask != 0);
    /* Drag starts optimistic whenever mouse exists and is downgraded at runtime. */
    platform->features.drag_reliable = platform->features.mouse_basic;

#if !defined(_WIN32) && !defined(__DJGPP__)
    if (platform->features.input_backend == INPUT_BACKEND_NCURSES &&
        platform->features.mouse_basic && !term_is_linux_vc) {
        platform->xterm_mouse_tracking_forced = platform_enable_xterm_mouse_tracking();
    }
#endif

#ifdef BUTTON1_DOUBLE_CLICKED
    platform->features.double_click =
        platform->features.mouse_basic &&
        ((available_mask & BUTTON1_DOUBLE_CLICKED) != 0);
#else
    platform->features.double_click = false;
#endif

platform->features.right_click = false;
#if defined(BUTTON3_PRESSED)
    if (available_mask & BUTTON3_PRESSED) platform->features.right_click = true;
#endif
#if defined(BUTTON3_CLICKED)
    if (available_mask & BUTTON3_CLICKED) platform->features.right_click = true;
#endif

    platform->features.linux_tty_keyboard_only = term_is_linux_vc;

    platform_update_mask(&platform->features);
    platform->last_mouse_y = -1;
    platform->last_mouse_x = -1;

    if (config && config->input_timeout_ms > 0) {
        timeout(config->input_timeout_ms);
    } else {
        timeout(100);
    }

    return true;
}

bool platform_poll_event_curses(PlatformBackend *platform, RetroEvent *out_event,
                                int timeout_ms) {
    if (!platform || !out_event) return false;
    if (timeout_ms < 0) timeout_ms = 0;

    timeout(timeout_ms);
    int ch = getch();
    if (ch == ERR) return false;

#ifdef KEY_RESIZE
    if (ch == KEY_RESIZE) {
        out_event->type = RETRO_EVENT_RESIZE;
        out_event->data.resize.rows = LINES;
        out_event->data.resize.cols = COLS;
        return true;
    }
#endif

#ifdef KEY_MOUSE
    if (ch == KEY_MOUSE) {
        if (!platform->features.mouse_basic) return false;
        MEVENT raw;
        if (getmouse(&raw) != OK) return false;
        out_event->type = RETRO_EVENT_MOUSE;
        return normalize_mouse(&raw, platform, &out_event->data.mouse);
    }
#endif

    out_event->type = RETRO_EVENT_KEY;
    out_event->data.key.key_code = translate_key_chord(ch);
    out_event->data.key.is_printable = retro_key_is_printable(ch);
    out_event->data.key.ascii = out_event->data.key.is_printable ? (char)ch : '\0';
    out_event->data.key.text_codepoint = out_event->data.key.is_printable
                                             ? (uint32_t)(unsigned char)ch
                                             : 0;
    out_event->data.key.modifiers = RETRO_MOD_NONE;
    return true;
}
