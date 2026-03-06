#ifndef RETRODESK_CORE_DESKTOP_H
#define RETRODESK_CORE_DESKTOP_H

#include <stdbool.h>

#include "platform/platform.h"

typedef struct Desktop Desktop;

typedef struct DesktopConfig {
    PlatformBackend *platform;
    int input_timeout_ms;
    bool bench_mode;
} DesktopConfig;

typedef struct DesktopCapabilities {
    unsigned int capability_mask;
    bool keyboard_basic;
    bool mouse_basic;
    bool drag_reliable;
    bool resize_events;
    bool color;
    bool unicode_basic;
    bool double_click;
    bool right_click;
    bool linux_tty_keyboard_only;
} DesktopCapabilities;

typedef struct DesktopDiagnostics {
    const char *backend_name;
    bool mouse_enabled;
    bool drag_enabled;
    bool drag_degraded;
    bool resize_enabled;
    bool linux_tty_keyboard_only;
} DesktopDiagnostics;

Desktop *desktop_create(const DesktopConfig *config);
int desktop_run(Desktop *desktop);
void desktop_request_redraw(Desktop *desktop);
const DesktopCapabilities *desktop_capabilities(const Desktop *desktop);
const DesktopDiagnostics *desktop_diagnostics(const Desktop *desktop);
void desktop_shutdown(Desktop *desktop);

#endif
