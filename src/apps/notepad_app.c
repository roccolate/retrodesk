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
    bool close_prompt;
    bool close_after_save;
    bool force_close;
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
        if (state) {
            snprintf(state->error, sizeof(state->error),
                     "Save As is required for Untitled documents");
        }
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
        if (error != RETRO_FS_OK ||
            !text_buffer_set_text(state->buffer, text ? text : "")) {
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

static void np_finish_close(RetroAppInstance *instance, NotepadState *state) {
    if (!instance || !state) return;
    state->close_prompt = false;
    state->close_after_save = false;
    state->force_close = true;
    app_request_close(instance);
}

static void np_handle_close_prompt(RetroAppInstance *instance,
                                   NotepadState *state,
                                   const RetroKeyEvent *key) {
    if (!instance || !state || !key) return;
    int code = key->key_code;
    unsigned char ch = key->ascii;

    if (code == RETRO_KEY_ESC) {
        state->close_prompt = false;
        state->close_after_save = false;
        state->error[0] = '\0';
        return;
    }

    if (ch == 'd' || ch == 'D') {
        state->dirty = false;
        np_finish_close(instance, state);
        return;
    }

    if (ch != 's' && ch != 'S') return;

    if (!state->has_path) {
        state->close_prompt = false;
        state->save_as = true;
        state->close_after_save = true;
        state->error[0] = '\0';
        return;
    }

    if (np_save(state)) {
        np_finish_close(instance, state);
    }
}

static void np_handle_save_as(RetroAppInstance *instance,
                              NotepadState *state,
                              const RetroKeyEvent *key) {
    if (!instance || !state || !key) return;
    int code = key->key_code;

    if (code == RETRO_KEY_ESC) {
        state->save_as = false;
        state->close_after_save = false;
        state->error[0] = '\0';
        return;
    }

    if (code == RETRO_KEY_CR || code == RETRO_KEY_LF) {
        RetroFsPath candidate = {0};
        if (retro_fs_path_init(&candidate,
                               text_input_text(state->path_input)) !=
            RETRO_FS_OK) {
            return;
        }
        RetroFsVersion existing = {0};
        if (retro_fs_stat(&candidate, &existing) == RETRO_FS_OK) {
            retro_fs_path_destroy(&candidate);
            snprintf(state->error, sizeof(state->error),
                     "Save As: destination exists");
            return;
        }

        retro_fs_path_destroy(&state->path);
        state->path = candidate;
        state->has_path = true;
        state->version = (RetroFsVersion){0};
        state->save_as = false;

        bool should_close = state->close_after_save;
        if (np_save(state)) {
            if (should_close) np_finish_close(instance, state);
        } else {
            state->close_after_save = false;
        }
        return;
    }

    (void)text_input_handle_key(state->path_input, key);
}

static void np_event(RetroAppInstance *instance, const RetroEvent *event) {
    if (!instance || !instance->state || !event ||
        event->type != RETRO_EVENT_KEY) {
        return;
    }
    NotepadState *state = instance->state;
    const RetroKeyEvent *key = &event->data.key;

    if (state->close_prompt) {
        np_handle_close_prompt(instance, state, key);
        return;
    }

    if (state->save_as) {
        np_handle_save_as(instance, state, key);
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_S) {
        state->close_after_save = false;
        if (state->has_path) {
            (void)np_save(state);
        } else {
            state->save_as = true;
        }
        return;
    }

    if (key->key_code == RETRO_KEY_F3) {
        state->close_after_save = false;
        state->save_as = true;
        return;
    }

    if (key->key_code == RETRO_KEY_ESC) {
        state->error[0] = '\0';
        return;
    }

    if (text_buffer_handle_key(state->buffer, key)) {
        state->dirty = true;
        state->force_close = false;
        state->error[0] = '\0';
    }
}

static void np_render_close_prompt(const NotepadState *state,
                                   DrawList *draw_list,
                                   const RenderStyle *text,
                                   const RenderStyle *accent,
                                   const RenderStyle *selected) {
    if (!state || !draw_list || !text || !accent || !selected) return;
    draw_list_hline(draw_list, 6, 8, 48, ' ', selected);
    draw_list_text(draw_list, 6, 10, "Unsaved changes", selected);
    draw_list_text(draw_list, 8, 10,
                   "Save changes before closing?", accent);
    draw_list_text(draw_list, 10, 10,
                   "[S] Save   [D] Discard   [Esc] Cancel", text);
}

static void np_render(RetroAppInstance *instance, DrawList *draw_list) {
    if (!instance || !instance->state || !draw_list) return;
    NotepadState *state = instance->state;
    const RetroTheme *theme = instance->ctx.theme;
    const RenderStyle *text = &theme->window_body;
    const RenderStyle *accent = &theme->shell_accent;
    const RenderStyle *cursor = &theme->launcher_selected;
    char title[192];
    const char *name = state->has_path
                           ? retro_fs_path_cstr(&state->path)
                           : "Untitled";
    snprintf(title, sizeof(title), "Notepad — %s%s",
             name, state->dirty ? " *" : "");
    draw_list_text(draw_list, 1, 2, title, accent);
    draw_list_text(draw_list, 2, 2,
                   "Ctrl+S save | F3 Save As | Ctrl+W close", text);

    if (state->close_prompt) {
        text_buffer_render(state->buffer, draw_list, 4, 2, 11, 64,
                           text, cursor);
        np_render_close_prompt(state, draw_list, text, accent, cursor);
    } else if (state->save_as) {
        draw_list_text(draw_list, 3, 2,
                       "Path (Enter save, Esc cancel):", accent);
        text_input_render(state->path_input, draw_list, 4, 2, 64,
                          text, cursor);
    } else {
        text_buffer_render(state->buffer, draw_list, 4, 2, 11, 64,
                           text, cursor);
    }

    if (state->error[0]) {
        draw_list_text(draw_list, 17, 2, state->error, accent);
    }
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
    if (!state->dirty || state->force_close) return true;
    state->close_prompt = true;
    state->close_after_save = false;
    state->error[0] = '\0';
    return false;
}

bool notepad_dirty_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return false;
    const NotepadState *state = instance->state;
    return state->dirty;
}

bool notepad_close_prompt_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return false;
    const NotepadState *state = instance->state;
    return state->close_prompt;
}

bool notepad_save_as_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return false;
    const NotepadState *state = instance->state;
    return state->save_as;
}

bool notepad_close_after_save_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return false;
    const NotepadState *state = instance->state;
    return state->close_after_save;
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
