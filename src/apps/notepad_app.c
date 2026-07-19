#include "app/app_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/key_chord.h"
#include "storage/retro_fs.h"
#include "ui/text_input.h"
#include "ui/text_buffer.h"

typedef struct NotepadState {
    TextBuffer *buffer;
    RetroFsPath path;
    RetroFsVersion version;
    bool has_path;
    bool save_as;
    TextInput *path_input;
    bool dirty;
    char error[160];
} NotepadState;

static void np_set_error(NotepadState *state, RetroFsError error) {
    if (!state) return;
    snprintf(state->error, sizeof(state->error), "Error: %s",
             retro_fs_error_string(error));
}

static bool np_save(NotepadState *state) {
    if (!state || !state->buffer || !state->has_path) {
        if (state) snprintf(state->error, sizeof(state->error),
                            "Save As is required for Untitled documents");
        return false;
    }
    size_t length = 0;
    char *text = text_buffer_to_text(state->buffer, &length);
    if (!text) {
        np_set_error(state, RETRO_FS_OOM);
        return false;
    }
    RetroFsVersion written = {0};
    RetroFsError error = retro_fs_write_atomic(
        &state->path, text, length,
        (state->version.inode != 0) ? &state->version : NULL, &written);
    free(text);
    if (error != RETRO_FS_OK) {
        np_set_error(state, error);
        return false;
    }
    state->version = written;
    state->dirty = false;
    state->error[0] = '\0';
    return true;
}

static bool np_create(RetroAppInstance *instance, const RetroAppContext *ctx) {
    if (!instance || !ctx) return false;
    NotepadState *state = calloc(1, sizeof(*state));
    if (!state) return false;
    state->buffer = text_buffer_create();
    if (!state->buffer) {
        free(state);
        return false;
    }
    state->path_input = text_input_create(240);
    if (!state->path_input) {
        text_buffer_destroy(state->buffer);
        free(state);
        return false;
    }

    if (ctx->resource_path && *ctx->resource_path) {
        RetroFsError error = retro_fs_path_init(&state->path, ctx->resource_path);
        if (error != RETRO_FS_OK) {
            text_buffer_destroy(state->buffer);
            text_input_destroy(state->path_input);
            free(state);
            return false;
        }
        char *text = NULL;
        size_t length = 0;
        error = retro_fs_read_text(&state->path, &text, &length, &state->version);
        if (error != RETRO_FS_OK || !text_buffer_set_text(state->buffer, text ? text : "")) {
            free(text);
            retro_fs_path_destroy(&state->path);
            text_buffer_destroy(state->buffer);
            text_input_destroy(state->path_input);
            free(state);
            return false;
        }
        free(text);
        state->has_path = true;
    }
    instance->state = state;
    return true;
}

static void np_event(RetroAppInstance *instance, const RetroEvent *event) {
    if (!instance || !instance->state || !event || event->type != RETRO_EVENT_KEY) return;
    NotepadState *state = instance->state;
    int key = event->data.key.key_code;
    if (state->save_as) {
        if (key == RETRO_KEY_ESC) { state->save_as = false; return; }
        if (key == RETRO_KEY_CR) {
            RetroFsPath candidate = {0};
            if (retro_fs_path_init(&candidate, text_input_text(state->path_input)) != RETRO_FS_OK) return;
            RetroFsVersion existing = {0};
            if (retro_fs_stat(&candidate, &existing) == RETRO_FS_OK) {
                retro_fs_path_destroy(&candidate);
                snprintf(state->error, sizeof(state->error), "Save As: destination exists");
                return;
            }
            retro_fs_path_destroy(&state->path);
            state->path = candidate;
            state->has_path = true;
            state->version = (RetroFsVersion){0};
            state->save_as = false;
            (void)np_save(state);
            return;
        }
        (void)text_input_handle_key(state->path_input, &event->data.key);
        return;
    }
    if (key == RETRO_KEY_CTRL_S) {
        if (state->has_path) (void)np_save(state);
        else state->save_as = true;
        return;
    }
    if (key == RETRO_KEY_ESC) {
        app_request_close(instance);
        return;
    }
    if (key == RETRO_KEY_F3) {
        state->save_as = true;
        return;
    }
    if (text_buffer_handle_key(state->buffer, &event->data.key)) {
        state->dirty = true;
        state->error[0] = '\0';
    }
}

static void np_render(RetroAppInstance *instance, DrawList *draw_list) {
    if (!instance || !instance->state || !draw_list) return;
    NotepadState *state = instance->state;
    const RetroTheme *theme = instance->ctx.theme;
    const RenderStyle *text = &theme->window_body;
    const RenderStyle *accent = &theme->shell_accent;
    const RenderStyle *cursor = &theme->launcher_selected;
    char title[192];
    const char *name = state->has_path ? retro_fs_path_cstr(&state->path) : "Untitled";
    snprintf(title, sizeof(title), "Notepad — %s%s", name, state->dirty ? " *" : "");
    draw_list_text(draw_list, 1, 2, title, accent);
    draw_list_text(draw_list, 2, 2, "Ctrl+S save | F3 Save As | Esc close", text);
    if (state->save_as) {
        draw_list_text(draw_list, 3, 2, "Path (Enter save, Esc cancel):", accent);
        text_input_render(state->path_input, draw_list, 4, 2, 64, text, cursor);
    } else text_buffer_render(state->buffer, draw_list, 4, 2, 11, 64, text, cursor);
    if (state->error[0]) draw_list_text(draw_list, 17, 2, state->error, accent);
}

static void np_destroy(RetroAppInstance *instance) {
    if (!instance) return;
    NotepadState *state = instance->state;
    if (!state) return;
    text_buffer_destroy(state->buffer);
    text_input_destroy(state->path_input);
    retro_fs_path_destroy(&state->path);
    free(state);
    instance->state = NULL;
}

static bool np_can_close(RetroAppInstance *instance) {
    if (!instance || !instance->state) return true;
    NotepadState *state = instance->state;
    if (!state->dirty) return true;
    snprintf(state->error, sizeof(state->error),
             "Unsaved changes: Ctrl+S before closing");
    return false;
}

const RetroAppDescriptor *notepad_app_descriptor(void) {
    static const RetroAppDescriptor desc = {
        .app_id = "notepad",
        .display_name = "Notepad",
        .required_capabilities = PLATFORM_CAP_KEYBOARD_BASIC,
        .default_height = 20,
        .default_width = 76,
        .default_y = 3,
        .default_x = 8,
        .window_flags = WINDOW_FLAG_APP_OWNED,
        .allow_multiple = true,
        .create = np_create,
        .on_event = np_event,
        .on_render = np_render,
        .can_close = np_can_close,
        .destroy = np_destroy,
    };
    return &desc;
}
