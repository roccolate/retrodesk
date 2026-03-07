#ifndef RETRODESK_APP_APP_RUNTIME_H
#define RETRODESK_APP_APP_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>

#include "core/event.h"
#include "platform/platform.h"
#include "render/render.h"
#include "ui/theme.h"
#include "wm/window_manager.h"

typedef struct Desktop Desktop;
typedef struct DesktopCapabilities DesktopCapabilities;

typedef struct RetroAppInstance RetroAppInstance;
typedef struct AppRegistry AppRegistry;

typedef struct RetroAppContext {
    Desktop *desktop;
    WindowId window_id;
    const DesktopCapabilities *capabilities;
    const RetroTheme *theme;
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
    bool (*create)(RetroAppInstance *instance, const RetroAppContext *ctx);
    void (*on_event)(RetroAppInstance *instance, const RetroEvent *event);
    void (*on_render)(RetroAppInstance *instance, DrawList *draw_list);
    void (*destroy)(RetroAppInstance *instance);
} RetroAppDescriptor;

struct RetroAppInstance {
    const RetroAppDescriptor *descriptor;
    RetroAppContext ctx;
    void *state;
    bool close_requested;
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

RetroAppInstance *app_launch(Desktop *desktop, const char *app_id);
void app_handle_event(RetroAppInstance *app, const RetroEvent *event);
void app_render(RetroAppInstance *app, DrawList *draw_list);
void app_destroy(RetroAppInstance *app);
void app_request_close(RetroAppInstance *app);
bool app_is_close_requested(const RetroAppInstance *app);

#endif
