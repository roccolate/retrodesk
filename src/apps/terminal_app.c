#include "app/app_runtime.h"

#include "core/desktop.h"

#include <stdio.h>

static bool terminal_create(RetroAppInstance *instance, const RetroAppContext *ctx) {
    (void)instance;
    (void)ctx;
    return true;
}

static void terminal_event(RetroAppInstance *instance, const RetroEvent *event) {
    if (!instance || !event || event->type != RETRO_EVENT_KEY) return;
    if (event->data.key.ascii == 'x') app_request_close(instance);
}

static void terminal_render(RetroAppInstance *instance, RenderContext *ctx) {
    char line[96];
    RenderStyle text = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false};
    const DesktopCapabilities *cap = instance->ctx.capabilities;

    render_draw_text(ctx, 1, 2, "Terminal (placeholder app)", &text);
    render_draw_text(ctx, 2, 2, "This app shows detected runtime capabilities.", &text);

    if (cap) {
        snprintf(line, sizeof(line), "mouse=%s drag=%s resize=%s right_click=%s",
                 cap->mouse_basic ? "on" : "off", cap->drag_reliable ? "on" : "off",
                 cap->resize_events ? "on" : "off",
                 cap->right_click ? "on" : "off");
        render_draw_text(ctx, 4, 2, line, &text);

        snprintf(line, sizeof(line), "linux_tty_keyboard_only=%s",
                 cap->linux_tty_keyboard_only ? "yes" : "no");
        render_draw_text(ctx, 5, 2, line, &text);
    }

    render_draw_text(ctx, 7, 2, "Press x to close.", &text);
}

static void terminal_destroy(RetroAppInstance *instance) {
    (void)instance;
}

const RetroAppDescriptor *terminal_app_descriptor(void) {
    static const RetroAppDescriptor desc = {
        .app_id = "terminal",
        .display_name = "Terminal",
        .required_capabilities = PLATFORM_CAP_KEYBOARD_BASIC,
        .default_height = 10,
        .default_width = 62,
        .default_y = 7,
        .default_x = 10,
        .window_flags = WINDOW_FLAG_APP_OWNED,
        .create = terminal_create,
        .on_event = terminal_event,
        .on_render = terminal_render,
        .destroy = terminal_destroy,
    };
    return &desc;
}
