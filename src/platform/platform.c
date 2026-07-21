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

#if !defined(_WIN32) && !defined(__DJGPP__)
#include <errno.h>
#include <poll.h>
#endif

enum {
    PLATFORM_MOUSE_CLICK_INTERVAL_MS = 150,
};

#if defined(_WIN32)
static bool windows_has_console(void) {
    DWORD mode = 0;
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (input == INVALID_HANDLE_VALUE || output == INVALID_HANDLE_VALUE) {
        return false;
    }
    if (!GetConsoleMode(input, &mode)) return false;
    if (!GetConsoleMode(output, &mode)) return false;
    return true;
}

static bool windows_reopen_stdio(FILE *stream, const char *name,
                                 const char *mode) {
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

#if !defined(_WIN32) && !defined(__DJGPP__)
/* RetroDesk owns one interactive terminal backend at a time. Signal handlers
   only set a sig_atomic_t flag; cleanup and recovery remain in normal code. */
static PlatformBackend *g_signal_platform = NULL;

static void platform_signal_handler(int signal_number) {
    (void)signal_number;
    if (g_signal_platform) g_signal_platform->tty_signal_pending = 1;
}

static void platform_restore_signal_handlers(PlatformBackend *platform) {
    if (!platform || !platform->signal_handlers_installed) return;
    if (platform->previous_sigint != SIG_ERR) {
        (void)signal(SIGINT, platform->previous_sigint);
    }
    if (platform->previous_sigterm != SIG_ERR) {
        (void)signal(SIGTERM, platform->previous_sigterm);
    }
    if (platform->previous_sighup != SIG_ERR) {
        (void)signal(SIGHUP, platform->previous_sighup);
    }
    platform->signal_handlers_installed = false;
    if (g_signal_platform == platform) g_signal_platform = NULL;
}

static bool platform_install_signal_handlers(PlatformBackend *platform) {
    if (!platform || (g_signal_platform && g_signal_platform != platform)) {
        return false;
    }

    g_signal_platform = platform;
    platform->previous_sigint = signal(SIGINT, platform_signal_handler);
    if (platform->previous_sigint == SIG_ERR) goto fail;
    platform->previous_sigterm = signal(SIGTERM, platform_signal_handler);
    if (platform->previous_sigterm == SIG_ERR) goto fail;
    platform->previous_sighup = signal(SIGHUP, platform_signal_handler);
    if (platform->previous_sighup == SIG_ERR) goto fail;
    platform->signal_handlers_installed = true;
    return true;

fail:
    if (platform->previous_sigint != SIG_ERR) {
        (void)signal(SIGINT, platform->previous_sigint);
    }
    if (platform->previous_sigterm != SIG_ERR) {
        (void)signal(SIGTERM, platform->previous_sigterm);
    }
    g_signal_platform = NULL;
    return false;
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

static void platform_normalize_pointer_activation(PlatformBackend *platform,
                                                  RetroEvent *event) {
    if (!platform || !event || event->type != RETRO_EVENT_MOUSE) return;

    RetroMouseEvent *mouse = &event->data.mouse;
    if (mouse->button1_pressed) {
        platform->button1_down = true;
        platform->button1_dragged = false;
        platform->button1_press_y = mouse->y;
        platform->button1_press_x = mouse->x;
    } else if (platform->button1_down &&
               (mouse->moved ||
                mouse->y != platform->button1_press_y ||
                mouse->x != platform->button1_press_x)) {
        platform->button1_dragged = true;
    }

    if (mouse->button1_released) {
        if (platform->button1_down && !platform->button1_dragged &&
            !mouse->button1_clicked) {
            mouse->button1_clicked = true;
        }
        platform->button1_down = false;
        platform->button1_dragged = false;
    }

    if (mouse->button1_clicked || mouse->button1_dblclick) {
        platform->button1_down = false;
        platform->button1_dragged = false;
    }
}

static RetroPollResult platform_poll_curses_result(
    PlatformBackend *platform, RetroEvent *out_event, int timeout_ms) {
#if !defined(_WIN32) && !defined(__DJGPP__)
    errno = 0;
#endif
    if (platform_poll_event_curses(platform, out_event, timeout_ms)) {
        return RETRO_POLL_EVENT;
    }

#if !defined(_WIN32) && !defined(__DJGPP__)
    int read_errno = errno;
    if (read_errno == EIO || read_errno == ENXIO) {
        return RETRO_POLL_CLOSED;
    }
    if (read_errno == EBADF) return RETRO_POLL_ERROR;

    struct pollfd descriptor = {
        .fd = STDIN_FILENO,
        .events = POLLIN,
        .revents = 0,
    };
    int ready = poll(&descriptor, 1, 0);
    if (ready < 0) {
        if (errno == EINTR && platform->tty_signal_pending) {
            return RETRO_POLL_CLOSED;
        }
        return errno == EINTR ? RETRO_POLL_TIMEOUT : RETRO_POLL_ERROR;
    }
    if (ready > 0) {
        if (descriptor.revents & POLLHUP) return RETRO_POLL_CLOSED;
        if (descriptor.revents & (POLLERR | POLLNVAL)) {
            return RETRO_POLL_ERROR;
        }
    }
#endif

    return RETRO_POLL_TIMEOUT;
}

#if !defined(_WIN32) && !defined(__DJGPP__)
bool platform_enable_xterm_mouse_tracking(void) {
    const char *term = getenv("TERM");
    if (!term || strcmp(term, "dumb") == 0) return false;
    /* 1000: basic click tracking, 1002: button-drag motion, 1006: SGR coords. */
    if (fputs("\033[?1000h\033[?1002h\033[?1006h", stdout) < 0) {
        return false;
    }
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
        if (!platform_init_tty_raw_backend(platform) ||
            !platform_install_signal_handlers(platform)) {
            platform_destroy_tty_raw_backend(platform);
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
#if !defined(_WIN32) && !defined(__DJGPP__)
    if (!platform_install_signal_handlers(platform)) {
        platform_destroy_curses_backend(platform);
        free(platform);
        return NULL;
    }
#endif
    if (platform->features.mouse_basic) {
        (void)mouseinterval(PLATFORM_MOUSE_CLICK_INTERVAL_MS);
    }

    return platform;
}

RetroPollResult platform_poll_event(PlatformBackend *platform,
                                    RetroEvent *out_event,
                                    int timeout_ms) {
    if (!platform || !out_event) return RETRO_POLL_ERROR;

    memset(out_event, 0, sizeof(*out_event));
    out_event->type = RETRO_EVENT_NONE;

#if !defined(_WIN32) && !defined(__DJGPP__)
    if (platform->tty_signal_pending) return RETRO_POLL_CLOSED;
#endif

    RetroPollResult result = RETRO_POLL_TIMEOUT;
#if !defined(_WIN32) && !defined(__DJGPP__)
    if (platform->features.input_backend == INPUT_BACKEND_TTY_RAW) {
        result = platform_poll_event_tty_raw(
            platform, out_event, timeout_ms);
    } else
#endif
    {
        result = platform_poll_curses_result(
            platform, out_event, timeout_ms);
    }

    if (result == RETRO_POLL_EVENT) {
        platform_normalize_pointer_activation(platform, out_event);
    }
    return result;
}

const PlatformFeatures *platform_features(const PlatformBackend *platform) {
    return platform ? &platform->features : NULL;
}

const char *platform_backend_name(const PlatformBackend *platform) {
    if (!platform) return "unknown";
    switch (platform->features.input_backend) {
        case INPUT_BACKEND_PDCURSES: return "pdcurses";
        case INPUT_BACKEND_NCURSES: return "ncurses";
        case INPUT_BACKEND_TTY_RAW: return "tty-raw";
        default: return "unknown";
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

    platform_destroy_curses_backend(platform);
#if !defined(_WIN32) && !defined(__DJGPP__)
    platform_restore_signal_handlers(platform);
#endif
    free(platform);
}

WINDOW *platform_native_stdscr(const PlatformBackend *platform) {
    if (!platform || !platform->uses_curses) return NULL;
    return stdscr;
}

bool platform_native_has_colors(const PlatformBackend *platform) {
    return platform && platform->features.color;
}
