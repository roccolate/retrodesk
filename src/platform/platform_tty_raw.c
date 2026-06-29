#include "platform/platform_backend_internal.h"

#if !defined(_WIN32) && !defined(__DJGPP__)

#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* Global pointer for signal handler to restore terminal state on crash. */
static PlatformBackend *g_tty_signal_platform = NULL;

static void platform_tty_signal_handler(int sig) {
    /* NOTE: tcsetattr is async-signal-safe per POSIX. The xterm mouse
       disable path uses fputs/fflush which are technically not safe, but
       leaving mouse tracking enabled on crash is worse for the user. */
    if (g_tty_signal_platform) {
        if (g_tty_signal_platform->tty_raw_enabled &&
            g_tty_signal_platform->tty_has_saved_mode) {
            tcsetattr(STDIN_FILENO, TCSANOW, &g_tty_signal_platform->tty_saved_mode);
        }
        if (g_tty_signal_platform->xterm_mouse_tracking_forced) {
            /* Best-effort: write(2) would be safer but mouse disable uses
               fputs which is acceptable for a crash-recovery path. */
            platform_disable_xterm_mouse_tracking();
        }
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

static bool platform_query_tty_size(int *rows, int *cols) {
    if (!rows || !cols) return false;
    struct winsize ws = {0};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0) return false;
    if (ws.ws_row <= 0 || ws.ws_col <= 0) return false;
    *rows = ws.ws_row;
    *cols = ws.ws_col;
    return true;
}

static void platform_make_raw_mode(struct termios *term) {
    if (!term) return;
    term->c_iflag &= (tcflag_t) ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
                                  ICRNL | IXON);
    term->c_oflag &= (tcflag_t) ~OPOST;
    term->c_lflag &= (tcflag_t) ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    term->c_cflag &= (tcflag_t) ~(CSIZE | PARENB);
    term->c_cflag |= CS8;
}

static bool platform_enable_tty_raw(PlatformBackend *platform) {
    if (!platform) return false;

    struct termios raw = {0};
    if (tcgetattr(STDIN_FILENO, &platform->tty_saved_mode) != 0) return false;
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

static int platform_key_up_code(void) {
#ifdef KEY_UP
    return KEY_UP;
#else
    return 1001;
#endif
}

static int platform_key_down_code(void) {
#ifdef KEY_DOWN
    return KEY_DOWN;
#else
    return 1002;
#endif
}

static int platform_key_left_code(void) {
#ifdef KEY_LEFT
    return KEY_LEFT;
#else
    return 1003;
#endif
}

static int platform_key_right_code(void) {
#ifdef KEY_RIGHT
    return KEY_RIGHT;
#else
    return 1004;
#endif
}

static TtyDecoderKeyMap platform_tty_key_map(void) {
    TtyDecoderKeyMap map = {
        .up = platform_key_up_code(),
        .down = platform_key_down_code(),
        .left = platform_key_left_code(),
        .right = platform_key_right_code(),
    };
    return map;
}

static bool platform_emit_tty_resize_if_needed(PlatformBackend *platform,
                                               RetroEvent *out_event) {
    if (!platform || !out_event || !platform->features.resize_events) return false;

    int rows = 0;
    int cols = 0;
    if (!platform_query_tty_size(&rows, &cols)) return false;

    if (rows == platform->tty_rows && cols == platform->tty_cols) return false;
    platform->tty_rows = rows;
    platform->tty_cols = cols;

    out_event->type = RETRO_EVENT_RESIZE;
    out_event->data.resize.rows = rows;
    out_event->data.resize.cols = cols;
    return true;
}

static bool platform_decode_tty_key(PlatformBackend *platform, RetroEvent *out_event) {
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
    platform->features.color = (term && strcmp(term, "dumb") != 0);
    platform->features.resize_events =
        platform_query_tty_size(&platform->tty_rows, &platform->tty_cols);
    if (!term_is_linux_vc) {
        platform->xterm_mouse_tracking_forced = platform_enable_xterm_mouse_tracking();
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

    /* Install signal handlers to restore terminal on crash. */
    g_tty_signal_platform = platform;
    signal(SIGINT, platform_tty_signal_handler);
    signal(SIGTERM, platform_tty_signal_handler);
    signal(SIGSEGV, platform_tty_signal_handler);
    signal(SIGABRT, platform_tty_signal_handler);

    return true;
}

bool platform_poll_event_tty_raw(PlatformBackend *platform, RetroEvent *out_event,
                                 int timeout_ms) {
    if (!platform || !out_event) return false;
    if (timeout_ms < 0) timeout_ms = 0;

    if (platform_decode_tty_key(platform, out_event)) return true;
    if (platform_emit_tty_resize_if_needed(platform, out_event)) return true;

    struct pollfd pfd = {.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
    int rc = poll(&pfd, 1, timeout_ms);
    if (rc < 0) return false;
    if (rc == 0) {
        return platform_emit_tty_resize_if_needed(platform, out_event);
    }

    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        /* stdin closed or errored — synthesize an Escape key so the desktop
           loop can detect quit cleanly. */
        out_event->type = RETRO_EVENT_KEY;
        out_event->data.key.key_code = 'q';
        out_event->data.key.is_printable = true;
        out_event->data.key.ascii = 'q';
        return true;
    }

    if (pfd.revents & POLLIN) {
        unsigned char buf[32];
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n <= 0) {
            /* EOF or read error — synthesize quit. */
            out_event->type = RETRO_EVENT_KEY;
            out_event->data.key.key_code = 'q';
            out_event->data.key.is_printable = true;
            out_event->data.key.ascii = 'q';
            return true;
        }
        tty_decoder_append(&platform->tty_decoder, buf, (size_t)n);
    }

    if (platform_decode_tty_key(platform, out_event)) return true;
    return platform_emit_tty_resize_if_needed(platform, out_event);
}

void platform_destroy_tty_raw_backend(PlatformBackend *platform) {
    if (g_tty_signal_platform == platform) {
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGSEGV, SIG_DFL);
        signal(SIGABRT, SIG_DFL);
        g_tty_signal_platform = NULL;
    }
    platform_disable_tty_raw(platform);
}

#endif
