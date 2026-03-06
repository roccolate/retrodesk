#include "platform/platform_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
static int is_tty(FILE *stream) {
    return isatty(fileno(stream));
}
#endif

struct PlatformBackend {
    PlatformFeatures features;
    int last_mouse_y;
    int last_mouse_x;
    bool xterm_mouse_tracking_forced;
};

#if defined(_WIN32)
static bool windows_has_console(void) {
    DWORD mode = 0;
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (in == INVALID_HANDLE_VALUE || out == INVALID_HANDLE_VALUE) return false;
    if (!GetConsoleMode(in, &mode)) return false;
    if (!GetConsoleMode(out, &mode)) return false;
    return true;
}

static bool windows_reopen_stdio(FILE *stream, const char *name, const char *mode) {
#if defined(_MSC_VER)
    FILE *opened = NULL;
    return freopen_s(&opened, name, mode, stream) == 0;
#else
    return freopen(name, mode, stream) != NULL;
#endif
}

static bool windows_ensure_console(void) {
    if (windows_has_console()) return true;

    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        if (!AllocConsole()) return false;
    }

    if (!windows_reopen_stdio(stdin, "CONIN$", "r")) return false;
    if (!windows_reopen_stdio(stdout, "CONOUT$", "w")) return false;
    if (!windows_reopen_stdio(stderr, "CONOUT$", "w")) return false;
    return windows_has_console();
}
#endif

static bool is_linux_virtual_console(void) {
#if defined(_WIN32)
    return false;
#else
    const char *term = getenv("TERM");
    return term && strcmp(term, "linux") == 0;
#endif
}

#if !defined(_WIN32) && !defined(__DJGPP__)
static bool platform_enable_xterm_mouse_tracking(void) {
    const char *term = getenv("TERM");
    if (!term || strcmp(term, "dumb") == 0) return false;
    /* 1000: basic click tracking, 1002: button-drag motion, 1006: SGR coords. */
    if (fputs("\033[?1000h\033[?1002h\033[?1006h", stdout) < 0) return false;
    fflush(stdout);
    return true;
}

static void platform_disable_xterm_mouse_tracking(void) {
    fputs("\033[?1006l\033[?1002l\033[?1000l\033[?1003l", stdout);
    fflush(stdout);
}
#endif

static void platform_update_mask(PlatformFeatures *f) {
    unsigned int mask = 0;
    if (f->keyboard_basic) mask |= PLATFORM_CAP_KEYBOARD_BASIC;
    if (f->mouse_basic) mask |= PLATFORM_CAP_MOUSE_BASIC;
    if (f->drag_reliable) mask |= PLATFORM_CAP_DRAG_RELIABLE;
    if (f->resize_events) mask |= PLATFORM_CAP_RESIZE;
    if (f->color) mask |= PLATFORM_CAP_COLOR;
    if (f->unicode_basic) mask |= PLATFORM_CAP_UNICODE_BASIC;
    if (f->double_click) mask |= PLATFORM_CAP_DOUBLE_CLICK;
    if (f->right_click) mask |= PLATFORM_CAP_RIGHT_CLICK;
    f->capability_mask = mask;
}

PlatformBackend *platform_create(const PlatformConfig *config) {
    PlatformBackend *platform = calloc(1, sizeof(*platform));
    if (!platform) return NULL;

#if defined(_WIN32)
    /* Windows terminals like Git Bash/mintty can start detached from a real
       console; try to attach/create one before curses initialization. */
    if (!windows_ensure_console()) {
        free(platform);
        return NULL;
    }
#else
    if (!is_tty(stdin) || !is_tty(stdout)) {
        free(platform);
        return NULL;
    }
#endif

    if (initscr() == NULL) {
        free(platform);
        return NULL;
    }

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mouseinterval(0);

    platform->features.keyboard_basic = true;
    platform->features.color = has_colors();
    platform->features.unicode_basic = true;
    bool term_is_linux_vc = is_linux_virtual_console();

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
    platform->features.mouse_basic = (available_mask != 0);

    /* Drag starts optimistic whenever mouse exists and is downgraded at runtime
       if no real motion events are observed. */
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

#if defined(BUTTON3_PRESSED) || defined(BUTTON3_CLICKED)
    platform->features.right_click = false;
#ifdef BUTTON3_PRESSED
    if (available_mask & BUTTON3_PRESSED) platform->features.right_click = true;
#endif
#ifdef BUTTON3_CLICKED
    if (available_mask & BUTTON3_CLICKED) platform->features.right_click = true;
#endif
#else
    platform->features.right_click = false;
#endif

    /* "keyboard-only" mode is only true when running in linux VC and no usable mouse. */
    platform->features.linux_tty_keyboard_only =
        term_is_linux_vc && !platform->features.mouse_basic;

    platform_update_mask(&platform->features);

    platform->last_mouse_y = -1;
    platform->last_mouse_x = -1;

    if (config && config->input_timeout_ms >= 0) {
        timeout(config->input_timeout_ms);
    } else {
        timeout(100);
    }

    return platform;
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

bool platform_poll_event(PlatformBackend *platform, RetroEvent *out_event,
                         int timeout_ms) {
    if (!platform || !out_event) return false;

    memset(out_event, 0, sizeof(*out_event));
    out_event->type = RETRO_EVENT_NONE;

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
    out_event->data.key.key_code = ch;
    out_event->data.key.is_printable = (ch >= 32 && ch <= 126);
    out_event->data.key.ascii = out_event->data.key.is_printable ? (char)ch : '\0';
    return true;
}

const PlatformFeatures *platform_features(const PlatformBackend *platform) {
    if (!platform) return NULL;
    return &platform->features;
}

const char *platform_backend_name(const PlatformBackend *platform) {
    if (!platform) return "unknown";
    switch (platform->features.input_backend) {
    case INPUT_BACKEND_PDCURSES:
        return "pdcurses";
    case INPUT_BACKEND_NCURSES:
        return "ncurses";
    default:
        return "unknown";
    }
}

void platform_destroy(PlatformBackend *platform) {
    if (!platform) return;
#if !defined(_WIN32) && !defined(__DJGPP__)
    if (platform->xterm_mouse_tracking_forced) {
        platform_disable_xterm_mouse_tracking();
    }
#endif
    endwin();
    free(platform);
}

WINDOW *platform_native_stdscr(const PlatformBackend *platform) {
    (void)platform;
    return stdscr;
}

bool platform_native_has_colors(const PlatformBackend *platform) {
    return platform && platform->features.color;
}
