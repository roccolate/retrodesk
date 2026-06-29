#include "app/app_runtime.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct NotepadState {
    size_t cursor;
    char content[96];
} NotepadState;

static bool notepad_create(RetroAppInstance *instance, const RetroAppContext *ctx) {
    (void)ctx;
    NotepadState *state = calloc(1, sizeof(*state));
    if (!state) return false;
    snprintf(state->content, sizeof(state->content),
             "RetroDesk notepad placeholder. Type printable chars.");
    instance->state = state;
    return true;
}

static void notepad_event(RetroAppInstance *instance, const RetroEvent *event) {
    NotepadState *state = (NotepadState *)instance->state;
    if (!state || event->type != RETRO_EVENT_KEY) return;

    int key = event->data.key.key_code;
    if (key == 'x' || key == 'X') {
        app_request_close(instance);
        return;
    }

    if (key == 8 || key == 127) {
        if (state->cursor > 0) {
            state->cursor--;
            state->content[state->cursor] = '\0';
        }
        return;
    }

    if (!event->data.key.is_printable) return;
    if (state->cursor >= sizeof(state->content) - 1) return;
    state->content[state->cursor++] = event->data.key.ascii;
    state->content[state->cursor] = '\0';
}

static void notepad_render(RetroAppInstance *instance, DrawList *draw_list) {
    NotepadState *state = (NotepadState *)instance->state;
    const RetroTheme *theme = instance->ctx.theme;
    const RenderStyle *text = &theme->window_body;
    const RenderStyle *accent = &theme->shell_accent;

    draw_list_text(draw_list, 1, 2, "Notepad (stub runtime app)", accent);
    draw_list_text(draw_list, 2, 2, "Type text, Backspace to erase, x to close.",
                   text);
    draw_list_text(draw_list, 4, 2, state->content, text);
}

static void notepad_destroy(RetroAppInstance *instance) {
    free(instance->state);
    instance->state = NULL;
}

const RetroAppDescriptor *notepad_app_descriptor(void) {
    static const RetroAppDescriptor desc = {
        .app_id = "notepad",
        .display_name = "Notepad",
        .required_capabilities = PLATFORM_CAP_KEYBOARD_BASIC,
        .default_height = 10,
        .default_width = 54,
        .default_y = 5,
        .default_x = 18,
        .window_flags = WINDOW_FLAG_APP_OWNED,
        .create = notepad_create,
        .on_event = notepad_event,
        .on_render = notepad_render,
        .destroy = notepad_destroy,
    };
    return &desc;
}
