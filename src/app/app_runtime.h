#ifndef RETRODESK_APP_APP_RUNTIME_H
#define RETRODESK_APP_APP_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>

#include "core/event.h"
#include "platform/platform.h"
#include "render/render.h"
#include "ui/theme.h"
#include "wm/window_manager.h"

/* App registry + descriptor lifecycle. Apps are registered by id and
   launched by the desktop; they never poll input directly. */

typedef struct Desktop Desktop;
/* DesktopCapabilities is a typedef alias for PlatformFeatures; forward
   declare the underlying struct instead. */
typedef struct PlatformFeatures DesktopCapabilities;

typedef struct RetroAppInstance RetroAppInstance;
typedef struct AppRegistry AppRegistry;

/* Close negotiation is separate from destruction. ALLOWED means the app has
   authorized a close request; Desktop may still defer window destruction until
   every app has authorized a transactional global shutdown. DEFERRED means the
   app owns a pending user decision. CANCELLED aborts that request and causes a
   Desktop transaction to reset every earlier authorization. */
typedef enum RetroCloseResult {
    RETRO_CLOSE_ALLOWED = 0,
    RETRO_CLOSE_DEFERRED,
    RETRO_CLOSE_CANCELLED,
} RetroCloseResult;

typedef struct RetroAppContext {
    Desktop *desktop;
    WindowId window_id;
    const DesktopCapabilities *capabilities;
    const RetroTheme *theme;
    /* Optional resource selected by a launcher (currently a filesystem path).
       The runtime owns the string for the lifetime of the instance. */
    const char *resource_path;
} RetroAppContext;

typedef struct RetroAppDescriptor {
    const char *app_id;
    const char *display_name;
    unsigned int required_capabilities;
    int default_height;
    int default_width;
    int default_y;
    int default_x;
    WindowFlags window_flags;
    bool allow_multiple;
    bool (*create)(RetroAppInstance *instance, const RetroAppContext *ctx);
    void (*on_event)(RetroAppInstance *instance, const RetroEvent *event);
    void (*on_render)(RetroAppInstance *instance, DrawList *draw_list);
    /* Return false to keep a dirty/document-protecting app open. */
    bool (*can_close)(RetroAppInstance *instance);
    void (*destroy)(RetroAppInstance *instance);
} RetroAppDescriptor;

struct RetroAppInstance {
    const RetroAppDescriptor *descriptor;
    RetroAppContext ctx;
    void *state;
    bool close_requested;
    bool close_pending;
    RetroCloseResult close_result;
    char *resource_path_owned;
};

AppRegistry *app_registry_create(void);
void app_registry_destroy(AppRegistry *registry);
void app_registry_reset(AppRegistry *registry);
bool app_registry_register(AppRegistry *registry, const RetroAppDescriptor *desc);
const RetroAppDescriptor *app_registry_find(const AppRegistry *registry,
                                             const char *app_id);
size_t app_registry_count(const AppRegistry *registry);
const RetroAppDescriptor *app_registry_descriptor_at(const AppRegistry *registry,
                                                      size_t index);

void app_handle_event(RetroAppInstance *app, const RetroEvent *event);
void app_render(RetroAppInstance *app, DrawList *draw_list);
void app_destroy(RetroAppInstance *app);
RetroCloseResult app_request_close(RetroAppInstance *app);
void app_resolve_close(RetroAppInstance *app, RetroCloseResult result);
void app_reset_close_request(RetroAppInstance *app);
bool app_is_close_requested(const RetroAppInstance *app);
bool app_is_close_pending(const RetroAppInstance *app);
RetroCloseResult app_close_result(const RetroAppInstance *app);

#endif
