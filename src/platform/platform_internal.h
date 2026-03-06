#ifndef RETRODESK_PLATFORM_PLATFORM_INTERNAL_H
#define RETRODESK_PLATFORM_PLATFORM_INTERNAL_H

#include <curses.h>

#include "platform/platform.h"

WINDOW *platform_native_stdscr(const PlatformBackend *platform);
bool platform_native_has_colors(const PlatformBackend *platform);

#endif
