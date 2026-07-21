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

/* DesktopCapabilities is now an alias for PlatformFeatures to avoid
   maintaining two identical structs. */
typedef PlatformFeatures DesktopCapabilities;

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
RetroAppInstance *app_launch_with_path(Desktop *desktop, const char *app_id,
                                       const char *resource_path);
void desktop_request_redraw(Desktop *desktop);
const DesktopDiagnostics *desktop_diagnostics(const Desktop *desktop);
size_t desktop_app_count(const Desktop *desktop);
size_t desktop_window_count(const Desktop *desktop);
WindowId desktop_active_window(const Desktop *desktop);
WindowId desktop_app_window_id(const Desktop *desktop, const char *app_id);
#ifdef RETRODESK_ENABLE_TEST_HOOKS
bool desktop_register_app_for_test(Desktop *desktop,
                                   const RetroAppDescriptor *desc);
bool desktop_dispatch_event_for_test(Desktop *desktop,
                                     const RetroEvent *event);
RetroAppInstance *desktop_app_instance_for_test(Desktop *desktop,
                                                const char *app_id);
bool desktop_shutdown_pending_for_test(const Desktop *desktop);
#endif
void desktop_shutdown(Desktop *desktop);

#endif
