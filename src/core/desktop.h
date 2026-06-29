#ifndef RETRODESK_CORE_DESKTOP_H
#define RETRODESK_CORE_DESKTOP_H

#include <stdbool.h>
#include <stddef.h>

#include "app/app_runtime.h"
#include "render/render.h"
#include "platform/platform.h"
#include "ui/theme.h"

/* Top-level desktop lifecycle: owns WM, renderer, app registry, and the
   global event loop. Consumes RetroEvent values and dispatches them to
   global hotkeys or to the WM for window-local handling. */

typedef struct Desktop Desktop;

typedef struct DesktopConfig {
    PlatformBackend *platform;
    int input_timeout_ms;
    bool bench_mode;
    RenderBackendKind render_backend;
    RetroThemeKind theme_kind;
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
    const char *render_backend_name;
    const char *theme_name;
    bool mouse_enabled;
    bool drag_enabled;
    bool drag_degraded;
    bool resize_enabled;
    bool linux_tty_keyboard_only;
} DesktopDiagnostics;

Desktop *desktop_create(const DesktopConfig *config);
int desktop_run(Desktop *desktop);
RetroAppInstance *app_launch(Desktop *desktop, const char *app_id);
void desktop_request_redraw(Desktop *desktop);
const DesktopDiagnostics *desktop_diagnostics(const Desktop *desktop);
size_t desktop_app_count(const Desktop *desktop);
size_t desktop_window_count(const Desktop *desktop);
void desktop_shutdown(Desktop *desktop);

#endif
