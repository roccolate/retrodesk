#ifndef RETRODESK_PLATFORM_BACKEND_INTERNAL_H
#define RETRODESK_PLATFORM_BACKEND_INTERNAL_H

#include <stdbool.h>

#include <curses.h>

#include "core/event.h"
#include "platform/platform.h"

#if !defined(_WIN32) && !defined(__DJGPP__)
#include <signal.h>
#include <termios.h>

#include "platform/tty_decoder.h"
#endif

struct PlatformBackend {
    PlatformFeatures features;
    int last_mouse_y;
    int last_mouse_x;
    bool xterm_mouse_tracking_forced;
    bool uses_curses;
#if !defined(_WIN32) && !defined(__DJGPP__)
    bool tty_raw_enabled;
    bool tty_has_saved_mode;
    struct termios tty_saved_mode;
    int tty_rows;
    int tty_cols;
    volatile sig_atomic_t tty_signal_pending;
    TtyDecoder tty_decoder;
#endif
};

bool platform_is_linux_virtual_console(void);
void platform_update_mask(PlatformFeatures *features);

#if !defined(_WIN32) && !defined(__DJGPP__)
bool platform_enable_xterm_mouse_tracking(void);
void platform_disable_xterm_mouse_tracking(void);
#endif

bool platform_init_curses_backend(PlatformBackend *platform,
                                  const PlatformConfig *config);
bool platform_poll_event_curses(PlatformBackend *platform, RetroEvent *out_event,
                                int timeout_ms);

#if !defined(_WIN32) && !defined(__DJGPP__)
bool platform_init_tty_raw_backend(PlatformBackend *platform);
bool platform_poll_event_tty_raw(PlatformBackend *platform, RetroEvent *out_event,
                                 int timeout_ms);
void platform_destroy_tty_raw_backend(PlatformBackend *platform);
#endif

#endif
