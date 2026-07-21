#include "app/app_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/clipboard.h"
#include "core/key_chord.h"
#include "storage/retro_fs.h"
#include "ui/text_buffer.h"
#include "ui/text_input.h"

enum {
    NP_HISTORY_MAX_STATES = 100,
    NP_HISTORY_MAX_BYTES = 1024 * 1024,
    NP_WRAP_COLUMNS = 64,
};

typedef struct NotepadHistoryEntry {
    char *text;
    size_t length;
    size_t cursor_row;
    size_t cursor_col;
} NotepadHistoryEntry;

typedef struct NotepadHistoryStack {
    NotepadHistoryEntry entries[NP_HISTORY_MAX_STATES];
    size_t count;
    size_t total_bytes;
} NotepadHistoryStack;

typedef struct NotepadState {
    TextBuffer *buffer;
    RetroClipboard *clipboard; /* borrowed */
    RetroFsPath path;
    RetroFsVersion version;
    bool has_path;
    bool save_as;
    bool search_mode;
    bool search_has_match;
    bool wrap_mode;
    bool close_prompt;
    bool close_after_save;
    bool force_close;
    TextInput *path_input;
    char *saved_text;
    size_t saved_length;
    NotepadHistoryStack undo;
    NotepadHistoryStack redo;
    bool dirty;
    char error[160];
} NotepadState;

static void np_set_error(NotepadState *state, RetroFsError error) {
    if (!state) return;
    snprintf(state->error, sizeof(state->error), "Error: %s",
             retro_fs_error_string(error));
}

static void np_history_entry_destroy(NotepadHistoryEntry *entry) {
    if (!entry) return;
    free(entry->text);
    *entry = (NotepadHistoryEntry){0};
}

static void np_history_stack_clear(NotepadHistoryStack *stack) {
    if (!stack) return;
    for (size_t i = 0; i < stack->count; ++i) {
        np_history_entry_destroy(&stack->entries[i]);
    }
    stack->count = 0;
    stack->total_bytes = 0;
}

static void np_history_stack_drop_oldest(NotepadHistoryStack *stack) {
    if (!stack || stack->count == 0) return;
    size_t removed = stack->entries[0].length;
    np_history_entry_destroy(&stack->entries[0]);
    if (stack->count > 1) {
        memmove(&stack->entries[0], &stack->entries[1],
                (stack->count - 1) * sizeof(stack->entries[0]));
    }
    stack->count--;
    stack->entries[stack->count] = (NotepadHistoryEntry){0};
    stack->total_bytes = removed <= stack->total_bytes
                             ? stack->total_bytes - removed
                             : 0;
}

static void np_history_stack_push(NotepadHistoryStack *stack,
                                  NotepadHistoryEntry *entry) {
    if (!stack || !entry || !entry->text) return;
    if (entry->length > NP_HISTORY_MAX_BYTES) {
        np_history_entry_destroy(entry);
        return;
    }
    while (stack->count >= NP_HISTORY_MAX_STATES ||
           stack->total_bytes + entry->length > NP_HISTORY_MAX_BYTES) {
        np_history_stack_drop_oldest(stack);
    }
    stack->entries[stack->count++] = *entry;
    stack->total_bytes += entry->length;
    *entry = (NotepadHistoryEntry){0};
}

static bool np_history_stack_pop(NotepadHistoryStack *stack,
                                 NotepadHistoryEntry *entry) {
    if (!stack || !entry || stack->count == 0) return false;
    stack->count--;
    *entry = stack->entries[stack->count];
    stack->entries[stack->count] = (NotepadHistoryEntry){0};
    stack->total_bytes = entry->length <= stack->total_bytes
                             ? stack->total_bytes - entry->length
                             : 0;
    return true;
}

static void np_clear_history(NotepadState *state) {
    if (!state) return;
    np_history_stack_clear(&state->undo);
    np_history_stack_clear(&state->redo);
}

static bool np_history_capture(const NotepadState *state,
                               NotepadHistoryEntry *entry) {
    if (!state || !state->buffer || !entry) return false;
    size_t length = 0;
    char *text = text_buffer_to_text(state->buffer, &length);
    if (!text) return false;
    if (length > NP_HISTORY_MAX_BYTES) {
        free(text);
        return false;
    }
    entry->text = text;
    entry->length = length;
    entry->cursor_row = text_buffer_cursor_row(state->buffer);
    entry->cursor_col = text_buffer_cursor_col(state->buffer);
    return true;
}

static bool np_text_matches(const char *left, size_t left_length,
                            const char *right, size_t right_length) {
    if (left_length != right_length) return false;
    if (left_length == 0) return true;
    return left && right && memcmp(left, right, left_length) == 0;
}

static void np_refresh_dirty(NotepadState *state) {
    if (!state || !state->buffer) return;
    size_t length = 0;
    char *text = text_buffer_to_text(state->buffer, &length);
    if (!text) {
        state->dirty = true;
        return;
    }
    state->dirty = !np_text_matches(text, length,
                                    state->saved_text,
                                    state->saved_length);
    free(text);
}

static bool np_set_saved_snapshot(NotepadState *state,
                                  char *text, size_t length) {
    if (!state || !text) return false;
    free(state->saved_text);
    state->saved_text = text;
    state->saved_length = length;
    state->dirty = false;
    return true;
}

static bool np_capture_saved_snapshot(NotepadState *state) {
    if (!state || !state->buffer) return false;
    size_t length = 0;
    char *text = text_buffer_to_text(state->buffer, &length);
    if (!text) return false;
    return np_set_saved_snapshot(state, text, length);
}

static bool np_history_entry_matches_buffer(const NotepadState *state,
                                            const NotepadHistoryEntry *entry) {
    if (!state || !entry || !entry->text) return true;
    size_t length = 0;
    char *text = text_buffer_to_text(state->buffer, &length);
    if (!text) return false;
    bool matches = np_text_matches(text, length,
                                   entry->text, entry->length);
    free(text);
    return matches;
}

static bool np_restore_history_entry(NotepadState *state,
                                     const NotepadHistoryEntry *entry) {
    if (!state || !entry || !entry->text) return false;
    if (!text_buffer_set_text(state->buffer, entry->text)) return false;
    text_buffer_set_cursor(state->buffer,
                           entry->cursor_row, entry->cursor_col);
    state->force_close = false;
    state->error[0] = '\0';
    np_refresh_dirty(state);
    return true;
}

static void np_undo(NotepadState *state) {
    if (!state || state->undo.count == 0) return;
    NotepadHistoryEntry current = {0};
    if (!np_history_capture(state, &current)) return;

    NotepadHistoryEntry target = {0};
    if (!np_history_stack_pop(&state->undo, &target)) {
        np_history_entry_destroy(&current);
        return;
    }
    if (!np_restore_history_entry(state, &target)) {
        np_history_stack_push(&state->undo, &target);
        np_history_entry_destroy(&current);
        return;
    }
    np_history_stack_push(&state->redo, &current);
    np_history_entry_destroy(&target);
}

static void np_redo(NotepadState *state) {
    if (!state || state->redo.count == 0) return;
    NotepadHistoryEntry current = {0};
    if (!np_history_capture(state, &current)) return;

    NotepadHistoryEntry target = {0};
    if (!np_history_stack_pop(&state->redo, &target)) {
        np_history_entry_destroy(&current);
        return;
    }
    if (!np_restore_history_entry(state, &target)) {
        np_history_stack_push(&state->redo, &target);
        np_history_entry_destroy(&current);
        return;
    }
    np_history_stack_push(&state->undo, &current);
    np_history_entry_destroy(&target);
}

static bool np_key_may_edit(const RetroKeyEvent *key) {
    if (!key) return false;
    switch (key->key_code) {
        case RETRO_KEY_LF:
        case RETRO_KEY_CR:
        case RETRO_KEY_BS:
        case RETRO_KEY_DEL:
        case RETRO_KEY_CTRL_K:
        case RETRO_KEY_CTRL_D:
        case RETRO_KEY_DC:
            return true;
        default:
            return key->is_printable;
    }
}

static void np_handle_editor_key(NotepadState *state,
                                 const RetroKeyEvent *key) {
    if (!state || !key) return;
    bool may_edit = np_key_may_edit(key);
    NotepadHistoryEntry before = {0};
    bool captured = false;

    if (may_edit) {
        captured = np_history_capture(state, &before);
        if (!captured) np_clear_history(state);
    }

    bool consumed = state->wrap_mode
                        ? text_buffer_handle_key_wrapped(
                              state->buffer, key, NP_WRAP_COLUMNS)
                        : text_buffer_handle_key(state->buffer, key);
    if (!consumed) {
        np_history_entry_destroy(&before);
        return;
    }

    if (may_edit) {
        bool changed = !captured ||
                       !np_history_entry_matches_buffer(state, &before);
        if (changed) {
            if (captured) {
                np_history_stack_push(&state->undo, &before);
                np_history_stack_clear(&state->redo);
            }
            state->force_close = false;
            state->error[0] = '\0';
            np_refresh_dirty(state);
        }
    }
    np_history_entry_destroy(&before);
}

static void np_finish_manual_edit(NotepadState *state,
                                  NotepadHistoryEntry *before,
                                  bool captured, bool changed) {
    if (!state || !before) return;
    if (changed) {
        if (captured) {
            np_history_stack_push(&state->undo, before);
            np_history_stack_clear(&state->redo);
        }
        state->force_close = false;
        state->error[0] = '\0';
        np_refresh_dirty(state);
    }
    np_history_entry_destroy(before);
}

static bool np_copy_selection(NotepadState *state) {
    if (!state || !text_buffer_has_selection(state->buffer)) return false;
    size_t length = 0;
    char *selected = text_buffer_selected_text(state->buffer, &length);
    if (!selected) return false;
    bool copied = retro_clipboard_set_text(state->clipboard, selected, length);
    free(selected);
    if (copied) {
        state->error[0] = '\0';
    } else {
        snprintf(state->error, sizeof(state->error),
                 "Clipboard: unable to copy selection");
    }
    return copied;
}

static void np_cut_selection(NotepadState *state) {
    if (!state || !np_copy_selection(state)) return;
    NotepadHistoryEntry before = {0};
    bool captured = np_history_capture(state, &before);
    if (!captured) np_clear_history(state);
    bool changed = text_buffer_delete_selection(state->buffer);
    np_finish_manual_edit(state, &before, captured, changed);
}

static void np_paste_clipboard(NotepadState *state) {
    if (!state || !retro_clipboard_has_text(state->clipboard)) return;
    size_t length = 0;
    const char *clipboard = retro_clipboard_text(state->clipboard, &length);
    NotepadHistoryEntry before = {0};
    bool captured = np_history_capture(state, &before);
    if (!captured) np_clear_history(state);
    bool changed = text_buffer_insert_text(state->buffer,
                                           clipboard, length);
    np_finish_manual_edit(state, &before, captured, changed);
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
        state->version.valid ? &state->version : NULL, &written);
    if (error != RETRO_FS_OK) {
        free(text);
        np_set_error(state, error);
        return false;
    }
    state->version = written;
    (void)np_set_saved_snapshot(state, text, length);
    state->error[0] = '\0';
    return true;
}

static bool np_create(RetroAppInstance *instance, const RetroAppContext *ctx) {
    if (!instance || !ctx) return false;
    NotepadState *state = calloc(1, sizeof(*state));
    if (!state) return false;
    state->clipboard = ctx->clipboard;
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
        RetroFsError error = retro_fs_path_init(&state->path,
                                                ctx->resource_path);
        if (error != RETRO_FS_OK) {
            text_buffer_destroy(state->buffer);
            text_input_destroy(state->path_input);
            free(state);
            return false;
        }
        char *text = NULL;
        size_t length = 0;
        error = retro_fs_read_text(&state->path, &text, &length,
                                   &state->version);
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

    if (!np_capture_saved_snapshot(state)) {
        retro_fs_path_destroy(&state->path);
        text_buffer_destroy(state->buffer);
        text_input_destroy(state->path_input);
        free(state);
        return false;
    }
    instance->state = state;
    return true;
}

static void np_finish_close(RetroAppInstance *instance,
                            NotepadState *state) {
    if (!instance || !state) return;
    state->close_prompt = false;
    state->close_after_save = false;
    state->force_close = true;
    app_resolve_close(instance, RETRO_CLOSE_ALLOWED);
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
        app_resolve_close(instance, RETRO_CLOSE_CANCELLED);
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
        state->search_mode = false;
        text_input_clear(state->path_input);
        state->save_as = true;
        state->close_after_save = true;
        state->error[0] = '\0';
        return;
    }

    if (np_save(state)) np_finish_close(instance, state);
}

static bool np_find_next(NotepadState *state) {
    if (!state || !state->buffer || !state->path_input) return false;
    const char *query = text_input_text(state->path_input);
    size_t query_length = text_input_length(state->path_input);
    if (query_length == 0) {
        snprintf(state->error, sizeof(state->error),
                 "Find: enter text to search");
        return false;
    }

    TextBufferMatch match = {0};
    size_t start_row = text_buffer_cursor_row(state->buffer);
    size_t start_col = text_buffer_cursor_col(state->buffer);
    if (!text_buffer_find_next(state->buffer, query, query_length, true,
                               start_row, start_col, true, &match)) {
        snprintf(state->error, sizeof(state->error),
                 "Find: no match");
        state->search_has_match = false;
        return false;
    }
    if (!text_buffer_select_match(state->buffer, &match)) return false;
    state->search_has_match = true;
    state->error[0] = '\0';
    return true;
}

static void np_handle_search(NotepadState *state,
                             const RetroKeyEvent *key) {
    if (!state || !key) return;
    if (key->key_code == RETRO_KEY_ESC) {
        state->search_mode = false;
        state->search_has_match = false;
        state->error[0] = '\0';
        return;
    }
    if (key->key_code == RETRO_KEY_CR || key->key_code == RETRO_KEY_LF) {
        (void)np_find_next(state);
        return;
    }
    if (text_input_handle_key(state->path_input, key)) {
        state->search_has_match = false;
        text_buffer_clear_selection(state->buffer);
        state->error[0] = '\0';
    }
}

static void np_handle_save_as(RetroAppInstance *instance,
                              NotepadState *state,
                              const RetroKeyEvent *key) {
    if (!instance || !state || !key) return;
    int code = key->key_code;

    if (code == RETRO_KEY_ESC) {
        bool was_closing = state->close_after_save;
        state->save_as = false;
        state->close_after_save = false;
        state->error[0] = '\0';
        if (was_closing) {
            app_resolve_close(instance, RETRO_CLOSE_CANCELLED);
        }
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
            if (should_close) {
                app_resolve_close(instance, RETRO_CLOSE_CANCELLED);
            }
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

    if (state->search_mode) {
        np_handle_search(state, key);
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_F) {
        state->search_mode = true;
        state->search_has_match = false;
        text_input_clear(state->path_input);
        text_buffer_clear_selection(state->buffer);
        state->error[0] = '\0';
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_A) {
        text_buffer_select_all(state->buffer);
        state->error[0] = '\0';
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_C) {
        (void)np_copy_selection(state);
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_X) {
        np_cut_selection(state);
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_V) {
        np_paste_clipboard(state);
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_Z) {
        np_undo(state);
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_Y) {
        np_redo(state);
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_S) {
        state->close_after_save = false;
        if (state->has_path) {
            (void)np_save(state);
        } else {
            state->search_mode = false;
            text_input_clear(state->path_input);
            state->save_as = true;
        }
        return;
    }

    if (key->key_code == RETRO_KEY_F3) {
        state->close_after_save = false;
        state->search_mode = false;
        text_input_clear(state->path_input);
        state->save_as = true;
        return;
    }

    if (key->key_code == RETRO_KEY_F4) {
        state->wrap_mode = !state->wrap_mode;
        state->error[0] = '\0';
        return;
    }

    if (key->key_code == RETRO_KEY_ESC) {
        if (text_buffer_has_selection(state->buffer)) {
            text_buffer_clear_selection(state->buffer);
        }
        state->error[0] = '\0';
        return;
    }

    np_handle_editor_key(state, key);
}

static void np_render_buffer(const NotepadState *state,
                             DrawList *draw_list,
                             int y, int rows,
                             const RenderStyle *text,
                             const RenderStyle *cursor,
                             const RenderStyle *selection) {
    if (!state || !draw_list || !text || !cursor || !selection) return;
    if (state->wrap_mode) {
        text_buffer_render_wrapped_with_selection(
            state->buffer, draw_list, y, 2, rows, NP_WRAP_COLUMNS,
            text, cursor, selection);
    } else {
        text_buffer_render_with_selection(
            state->buffer, draw_list, y, 2, rows, NP_WRAP_COLUMNS,
            text, cursor, selection);
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
    RenderStyle selection = theme->launcher_selected;
    selection.bold = false;
    char title[192];
    const char *name = state->has_path
                           ? retro_fs_path_cstr(&state->path)
                           : "Untitled";
    snprintf(title, sizeof(title), "Notepad — %s%s",
             name, state->dirty ? " *" : "");
    draw_list_text(draw_list, 1, 2, title, accent);
    draw_list_text(draw_list, 2, 2,
                   "Ctrl+S save | F3 Save As | Ctrl+Z/Y undo/redo | Ctrl+W close",
                   text);
    if (!state->save_as && !state->close_prompt && !state->search_mode) {
        char edit_hint[128];
        snprintf(edit_hint, sizeof(edit_hint),
                 "Shift+arrows select | Ctrl+A/C/X/V | Ctrl+F | F4 wrap:%s",
                 state->wrap_mode ? "on" : "off");
        draw_list_text(draw_list, 3, 2, edit_hint, text);
    }

    if (state->close_prompt) {
        np_render_buffer(state, draw_list, 4, 11,
                         text, cursor, &selection);
        np_render_close_prompt(state, draw_list, text, accent, cursor);
    } else if (state->save_as) {
        draw_list_text(draw_list, 3, 2,
                       "Path (Enter save, Esc cancel):", accent);
        text_input_render(state->path_input, draw_list, 4, 2, 64,
                          text, cursor);
    } else if (state->search_mode) {
        draw_list_text(draw_list, 3, 2,
                       "Find (Enter next, Esc close):", accent);
        text_input_render(state->path_input, draw_list, 4, 2, 64,
                          text, cursor);
        np_render_buffer(state, draw_list, 6, 9,
                         text, cursor, &selection);
    } else {
        np_render_buffer(state, draw_list, 4, 11,
                         text, cursor, &selection);
    }

    if (state->error[0]) {
        draw_list_text(draw_list, 17, 2, state->error, accent);
    }
}

static void np_destroy(RetroAppInstance *instance) {
    if (!instance) return;
    NotepadState *state = instance->state;
    if (!state) return;
    np_clear_history(state);
    free(state->saved_text);
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

size_t notepad_undo_count_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return 0;
    const NotepadState *state = instance->state;
    return state->undo.count;
}

size_t notepad_redo_count_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return 0;
    const NotepadState *state = instance->state;
    return state->redo.count;
}

const char *notepad_line_for_test(const RetroAppInstance *instance,
                                  size_t row) {
    if (!instance || !instance->state) return "";
    const NotepadState *state = instance->state;
    return text_buffer_line(state->buffer, row);
}

size_t notepad_cursor_col_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return 0;
    const NotepadState *state = instance->state;
    return text_buffer_cursor_col(state->buffer);
}

size_t notepad_cursor_row_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return 0;
    const NotepadState *state = instance->state;
    return text_buffer_cursor_row(state->buffer);
}

bool notepad_search_mode_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return false;
    const NotepadState *state = instance->state;
    return state->search_mode;
}

bool notepad_wrap_mode_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return false;
    const NotepadState *state = instance->state;
    return state->wrap_mode;
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
