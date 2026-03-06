#ifndef RETRODESK_APP_APP_RUNTIME_H
#define RETRODESK_APP_APP_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>

#include "core/event.h"
#include "platform/platform.h"
#include "render/render.h"
#include "wm/window_manager.h"

typedef struct Desktop Desktop;
typedef struct DesktopCapabilities DesktopCapabilities;

typedef struct RetroAppInstance RetroAppInstance;

typedef struct RetroAppContext {
    Desktop *desktop;
    WindowId window_id;
    const DesktopCapabilities *capabilities;
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
    void (*on_render)(RetroAppInstance *instance, RenderContext *ctx);
    void (*destroy)(RetroAppInstance *instance);
} RetroAppDescriptor;

struct RetroAppInstance {
    const RetroAppDescriptor *descriptor;
    RetroAppContext ctx;
    void *state;
    bool close_requested;
};

bool app_register(const RetroAppDescriptor *desc);
void app_registry_reset(void);
const RetroAppDescriptor *app_find(const char *app_id);
size_t app_registry_count(void);
const RetroAppDescriptor *app_descriptor_at(size_t index);

RetroAppInstance *app_launch(Desktop *desktop, const char *app_id);
void app_handle_event(RetroAppInstance *app, const RetroEvent *event);
void app_render(RetroAppInstance *app, RenderContext *ctx);
void app_destroy(RetroAppInstance *app);
void app_request_close(RetroAppInstance *app);
bool app_is_close_requested(const RetroAppInstance *app);

#endif
