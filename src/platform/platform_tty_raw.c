#include "platform/platform_backend_internal.h"

#if !defined(_WIN32) && !defined(__DJGPP__)

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "core/key_chord.h"

/* The handler only records a signal. Terminal restoration happens from the
   normal platform_destroy path, where stdio and termios are safe to use. */
static PlatformBackend *g_tty_signal_platform = NULL;

static void platform_tty_signal_handler(int signal_number) {
    (void)signal_number;
    if (g_tty_signal_platform) {
        g_tty_signal_platform->tty_signal_pending = 1;
    }
}

static bool platform_query_tty_size(int *rows, int *cols) {
    if (!rows || !cols) return false;
    struct winsize size = {0};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0) return false;
    if (size.ws_row <= 0 || size.ws_col <= 0) return false;
    *rows = size.ws_row;
    *cols = size.ws_col;
    return true;
}

static void platform_make_raw_mode(struct termios *term) {
    if (!term) return;
    term->c_iflag &= (tcflag_t)~(IGNBRK | BRKINT | PARMRK | ISTRIP |
                                 INLCR | IGNCR | ICRNL | IXON);
    term->c_oflag &= (tcflag_t)~OPOST;
    term->c_lflag &= (tcflag_t)~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    term->c_cflag &= (tcflag_t)~(CSIZE | PARENB);
    term->c_cflag |= CS8;
}

static bool platform_enable_tty_raw(PlatformBackend *platform) {
    if (!platform) return false;

    struct termios raw = {0};
    if (tcgetattr(STDIN_FILENO, &platform->tty_saved_mode) != 0) {
        return false;
    }
    platform->tty_has_saved_mode = true;
    raw = platform->tty_saved_mode;
    platform_make_raw_mode(&raw);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return false;
    platform->tty_raw_enabled = true;
    return true;
}

static void platform_disable_tty_raw(PlatformBackend *platform) {
    if (!platform) return;
    if (platform->tty_raw_enabled && platform->tty_has_saved_mode) {
        tcsetattr(STDIN_FILENO, TCSANOW, &platform->tty_saved_mode);
    }
    platform->tty_raw_enabled = false;
}

/* The tty-raw backend never touches curses; it emits portable RETRO_KEY_*
   chords directly so consumers can dispatch without backend-specific
   magic numbers. */
static TtyDecoderKeyMap platform_tty_key_map(void) {
    TtyDecoderKeyMap map = {
        .up = RETRO_KEY_UP,
        .down = RETRO_KEY_DOWN,
        .left = RETRO_KEY_LEFT,
        .right = RETRO_KEY_RIGHT,
    };
    return map;
}

static bool platform_emit_tty_resize_if_needed(PlatformBackend *platform,
                                                RetroEvent *out_event) {
    if (!platform || !out_event || !platform->features.resize_events) {
        return false;
    }

    int rows = 0;
    int cols = 0;
    if (!platform_query_tty_size(&rows, &cols)) return false;
    if (rows == platform->tty_rows && cols == platform->tty_cols) {
        return false;
    }
    platform->tty_rows = rows;
    platform->tty_cols = cols;

    out_event->type = RETRO_EVENT_RESIZE;
    out_event->data.resize.rows = rows;
    out_event->data.resize.cols = cols;
    return true;
}

static bool platform_decode_tty_key(PlatformBackend *platform,
                                    RetroEvent *out_event) {
    if (!platform || !out_event) return false;
    TtyDecoderKeyMap keys = platform_tty_key_map();
    return tty_decoder_decode(&platform->tty_decoder, &keys, out_event);
}

bool platform_init_tty_raw_backend(PlatformBackend *platform) {
    if (!platform) return false;
    if (!platform_enable_tty_raw(platform)) return false;

    bool term_is_linux_vc = platform_is_linux_virtual_console();
    const char *term = getenv("TERM");

    platform->uses_curses = false;
    platform->features.input_backend = INPUT_BACKEND_TTY_RAW;
    platform->features.keyboard_basic = true;
    platform->features.unicode_basic = true;
    platform->features.color = term && strcmp(term, "dumb") != 0;
    platform->features.resize_events =
        platform_query_tty_size(&platform->tty_rows, &platform->tty_cols);
    if (!term_is_linux_vc) {
        platform->xterm_mouse_tracking_forced =
            platform_enable_xterm_mouse_tracking();
        if (platform->xterm_mouse_tracking_forced) {
            platform->features.mouse_basic = true;
            platform->features.drag_reliable = true;
            platform->features.right_click = true;
        }
    }
    platform->features.linux_tty_keyboard_only =
        term_is_linux_vc && !platform->features.mouse_basic;
    platform->last_mouse_y = -1;
    platform->last_mouse_x = -1;
    tty_decoder_init(&platform->tty_decoder);
    platform_update_mask(&platform->features);

    g_tty_signal_platform = platform;
    signal(SIGINT, platform_tty_signal_handler);
    signal(SIGTERM, platform_tty_signal_handler);

    return true;
}

RetroPollResult platform_poll_event_tty_raw(PlatformBackend *platform,
                                            RetroEvent *out_event,
                                            int timeout_ms) {
    if (!platform || !out_event) return RETRO_POLL_ERROR;
    if (platform->tty_signal_pending) return RETRO_POLL_CLOSED;
    if (timeout_ms < 0) timeout_ms = 0;

    if (platform_decode_tty_key(platform, out_event)) {
        return RETRO_POLL_EVENT;
    }
    if (platform_emit_tty_resize_if_needed(platform, out_event)) {
        return RETRO_POLL_EVENT;
    }

    struct pollfd descriptor = {
        .fd = STDIN_FILENO,
        .events = POLLIN,
        .revents = 0,
    };
    int result = poll(&descriptor, 1, timeout_ms);
    if (result < 0) {
        if (errno == EINTR && platform->tty_signal_pending) {
            return RETRO_POLL_CLOSED;
        }
        return RETRO_POLL_ERROR;
    }
    if (result == 0) {
        return platform_emit_tty_resize_if_needed(platform, out_event)
                   ? RETRO_POLL_EVENT
                   : RETRO_POLL_TIMEOUT;
    }

    if (descriptor.revents & POLLNVAL) return RETRO_POLL_ERROR;
    if (descriptor.revents & POLLERR) return RETRO_POLL_ERROR;
    if (descriptor.revents & POLLHUP) return RETRO_POLL_CLOSED;

    if (descriptor.revents & POLLIN) {
        unsigned char bytes[32];
        ssize_t count = read(STDIN_FILENO, bytes, sizeof(bytes));
        if (count == 0) return RETRO_POLL_CLOSED;
        if (count < 0) {
            if (errno == EINTR && platform->tty_signal_pending) {
                return RETRO_POLL_CLOSED;
            }
            return RETRO_POLL_ERROR;
        }
        tty_decoder_append(&platform->tty_decoder, bytes, (size_t)count);
    }

    if (platform_decode_tty_key(platform, out_event)) {
        return RETRO_POLL_EVENT;
    }
    return platform_emit_tty_resize_if_needed(platform, out_event)
               ? RETRO_POLL_EVENT
               : RETRO_POLL_TIMEOUT;
}

void platform_destroy_tty_raw_backend(PlatformBackend *platform) {
    if (g_tty_signal_platform == platform) {
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        g_tty_signal_platform = NULL;
    }
    platform_disable_tty_raw(platform);
}

#endif
