#include "platform/platform_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/platform_backend_internal.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
static int is_tty(FILE *stream) {
    return isatty(fileno(stream));
}
#endif

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

bool platform_is_linux_virtual_console(void) {
#if defined(_WIN32)
    return false;
#else
    const char *term = getenv("TERM");
    return term && strcmp(term, "linux") == 0;
#endif
}

void platform_update_mask(PlatformFeatures *features) {
    if (!features) return;
    unsigned int mask = 0;
    if (features->keyboard_basic) mask |= PLATFORM_CAP_KEYBOARD_BASIC;
    if (features->mouse_basic) mask |= PLATFORM_CAP_MOUSE_BASIC;
    if (features->drag_reliable) mask |= PLATFORM_CAP_DRAG_RELIABLE;
    if (features->resize_events) mask |= PLATFORM_CAP_RESIZE;
    if (features->color) mask |= PLATFORM_CAP_COLOR;
    if (features->unicode_basic) mask |= PLATFORM_CAP_UNICODE_BASIC;
    if (features->double_click) mask |= PLATFORM_CAP_DOUBLE_CLICK;
    if (features->right_click) mask |= PLATFORM_CAP_RIGHT_CLICK;
    features->capability_mask = mask;
}

#if !defined(_WIN32) && !defined(__DJGPP__)
bool platform_enable_xterm_mouse_tracking(void) {
    const char *term = getenv("TERM");
    if (!term || strcmp(term, "dumb") == 0) return false;
    /* 1000: basic click tracking, 1002: button-drag motion, 1006: SGR coords. */
    if (fputs("\033[?1000h\033[?1002h\033[?1006h", stdout) < 0) return false;
    fflush(stdout);
    return true;
}

void platform_disable_xterm_mouse_tracking(void) {
    fputs("\033[?1006l\033[?1002l\033[?1000l\033[?1003l", stdout);
    fflush(stdout);
}
#endif

PlatformBackend *platform_create(const PlatformConfig *config) {
    PlatformBackend *platform = calloc(1, sizeof(*platform));
    if (!platform) return NULL;

#if defined(_WIN32)
    /* Windows terminals like Git Bash/mintty can start detached from a real console. */
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

#if !defined(_WIN32) && !defined(__DJGPP__)
    InputBackendKind requested_backend =
        config ? config->requested_input_backend : INPUT_BACKEND_UNKNOWN;
    if (requested_backend == INPUT_BACKEND_TTY_RAW) {
        if (!platform_init_tty_raw_backend(platform)) {
            free(platform);
            return NULL;
        }
        return platform;
    }
#else
    (void)config;
#endif

    if (!platform_init_curses_backend(platform, config)) {
        free(platform);
        return NULL;
    }

    return platform;
}

RetroPollResult platform_poll_event(PlatformBackend *platform, RetroEvent *out_event,
                                    int timeout_ms) {
    if (!platform || !out_event) return RETRO_POLL_ERROR;

    memset(out_event, 0, sizeof(*out_event));
    out_event->type = RETRO_EVENT_NONE;

#if !defined(_WIN32) && !defined(__DJGPP__)
    if (platform->features.input_backend == INPUT_BACKEND_TTY_RAW) {
        if (platform->tty_signal_pending) return RETRO_POLL_CLOSED;
        return platform_poll_event_tty_raw(platform, out_event, timeout_ms)
                   ? RETRO_POLL_EVENT
                   : RETRO_POLL_TIMEOUT;
    }
#endif

    return platform_poll_event_curses(platform, out_event, timeout_ms)
               ? RETRO_POLL_EVENT
               : RETRO_POLL_TIMEOUT;
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
    case INPUT_BACKEND_TTY_RAW:
        return "tty-raw";
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
    platform_destroy_tty_raw_backend(platform);
#endif

    if (platform->uses_curses) {
        endwin();
    }
    free(platform);
}

WINDOW *platform_native_stdscr(const PlatformBackend *platform) {
    if (!platform || !platform->uses_curses) return NULL;
    return stdscr;
}

bool platform_native_has_colors(const PlatformBackend *platform) {
    return platform && platform->features.color;
}
