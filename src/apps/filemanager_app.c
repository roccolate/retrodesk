#include "app/app_runtime.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct FileManagerState {
    int selection;
} FileManagerState;

static bool fm_create(RetroAppInstance *instance, const RetroAppContext *ctx) {
    (void)ctx;
    FileManagerState *state = calloc(1, sizeof(*state));
    if (!state) return false;
    instance->state = state;
    return true;
}

static void fm_event(RetroAppInstance *instance, const RetroEvent *event) {
    FileManagerState *state = (FileManagerState *)instance->state;
    if (!state || event->type != RETRO_EVENT_KEY) return;
    int key = event->data.key.key_code;
    if (key == 'w' || key == 'W') {
        if (state->selection > 0) state->selection--;
    } else if (key == 's' || key == 'S') {
        if (state->selection < 2) state->selection++;
    } else if (event->data.key.ascii == 'x') {
        app_request_close(instance);
    }
}

static void fm_render(RetroAppInstance *instance, RenderContext *ctx) {
    FileManagerState *state = (FileManagerState *)instance->state;
    char line[96];
    RenderStyle text = {RENDER_COLOR_BLACK, RENDER_COLOR_WHITE, false, false};

    render_draw_text(ctx, 1, 2, "FileManager (stub runtime app)", &text);
    render_draw_text(ctx, 2, 2, "Use w/s to move selection, x to close.", &text);

    for (int i = 0; i < 3; ++i) {
        RenderStyle row = text;
        if (state && state->selection == i) row.reverse = true;
        snprintf(line, sizeof(line), "[%c] Item %d", state && state->selection == i ? '*' : ' ',
                 i + 1);
        render_draw_text(ctx, 4 + i, 4, line, &row);
    }
}

static void fm_destroy(RetroAppInstance *instance) {
    free(instance->state);
    instance->state = NULL;
}

const RetroAppDescriptor *filemanager_app_descriptor(void) {
    static const RetroAppDescriptor desc = {
        .app_id = "filemanager",
        .display_name = "File Manager",
        .required_capabilities = PLATFORM_CAP_KEYBOARD_BASIC,
        .default_height = 12,
        .default_width = 46,
        .default_y = 3,
        .default_x = 6,
        .window_flags = WINDOW_FLAG_APP_OWNED,
        .create = fm_create,
        .on_event = fm_event,
        .on_render = fm_render,
        .destroy = fm_destroy,
    };
    return &desc;
}
