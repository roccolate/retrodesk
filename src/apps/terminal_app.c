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
    int key = event->data.key.key_code;
    if (key == 27) app_request_close(instance);
}

static void terminal_render(RetroAppInstance *instance, DrawList *draw_list) {
    char line[96];
    const RetroTheme *theme = instance->ctx.theme;
    const RenderStyle *text = &theme->window_body;
    const RenderStyle *accent = &theme->shell_accent;
    const DesktopCapabilities *cap = instance->ctx.capabilities;

    draw_list_text(draw_list, 1, 2, "Terminal (placeholder app)", accent);
    draw_list_text(draw_list, 2, 2,
                   "This app shows detected runtime capabilities.", text);

    if (!cap) {
        draw_list_text(draw_list, 4, 2, "(capabilities unavailable)", text);
    } else {
        snprintf(line, sizeof(line), "mouse=%s drag=%s resize=%s right_click=%s",
                 cap->mouse_basic ? "on" : "off", cap->drag_reliable ? "on" : "off",
                 cap->resize_events ? "on" : "off",
                 cap->right_click ? "on" : "off");
        draw_list_text(draw_list, 4, 2, line, text);

        snprintf(line, sizeof(line), "linux_tty_keyboard_only=%s",
                 cap->linux_tty_keyboard_only ? "yes" : "no");
        draw_list_text(draw_list, 5, 2, line, text);
    }

    draw_list_text(draw_list, 7, 2, "Press Esc to close.", text);
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
