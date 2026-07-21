#ifndef RETRODESK_PLATFORM_PLATFORM_INTERNAL_H
#define RETRODESK_PLATFORM_PLATFORM_INTERNAL_H

#if defined(_WIN32)
#include <windows.h>
/* Windows and PDCurses both publish MOUSE_MOVED. Let curses own the token used
   by the terminal backend, while windows.h remains protected by its include guard. */
#ifdef MOUSE_MOVED
#undef MOUSE_MOVED
#endif
#endif
#include <curses.h>

#include "platform/platform.h"

/* NOTE: This header unconditionally includes <curses.h> because the build
   always links a curses library (ncurses or PDCurses). If a curses-free
   build is ever needed, guard this include behind a RETRODESK_HAS_CURSES
   define and use void* as an opaque handle for WINDOW*. */

WINDOW *platform_native_stdscr(const PlatformBackend *platform);
bool platform_native_has_colors(const PlatformBackend *platform);

#endif
