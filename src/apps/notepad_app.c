#include "app/app_runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/clipboard.h"
#include "core/desktop.h"
#include "core/key_chord.h"
#include "core/utf8.h"
#include "storage/retro_fs.h"
#include "ui/menu_bar.h"
#include "ui/text_buffer.h"
#include "ui/text_input.h"

enum {
    NP_HISTORY_MAX_STATES = 100,
    NP_HISTORY_MAX_BYTES = 1024 * 1024,
    NP_DEFAULT_WRAP_COLUMNS = 64,
    NP_MIN_VIEW_COLUMNS = 1,
    NP_RECOVERY_PATH_CAP = 768,
    NP_RECOVERY_ATTEMPTS = 32,
};

typedef enum NotepadMenuAction {
    NP_MENU_NONE = 0,
    NP_MENU_NEW,
    NP_MENU_OPEN,
    NP_MENU_SAVE,
    NP_MENU_SAVE_AS,
    NP_MENU_CLOSE,
    NP_MENU_UNDO,
    NP_MENU_REDO,
    NP_MENU_CUT,
    NP_MENU_COPY,
    NP_MENU_PASTE,
    NP_MENU_SELECT_ALL,
    NP_MENU_FIND,
    NP_MENU_TOGGLE_WRAP,
} NotepadMenuAction;

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
    bool open_path;
    bool search_mode;
    bool search_has_match;
    bool wrap_mode;
    bool close_prompt;
    bool close_after_save;
    bool force_close;
    TextInput *path_input;
    MenuBar *menu_bar;
    NotepadMenuAction pending_menu_action;
    char *saved_text;
    size_t saved_length;
    NotepadHistoryStack undo;
    NotepadHistoryStack redo;
    bool dirty;
    char error[160];
} NotepadState;

static int np_window_width(const RetroAppInstance *instance) {
    if (!instance || instance->ctx.window_width <= 0) return 76;
    return instance->ctx.window_width;
}

static int np_window_height(const RetroAppInstance *instance) {
    if (!instance || instance->ctx.window_height <= 0) return 20;
    return instance->ctx.window_height;
}

static size_t np_view_columns(const RetroAppInstance *instance) {
    if (!instance || instance->ctx.window_width <= 0) {
        return NP_DEFAULT_WRAP_COLUMNS;
    }
    int columns = instance->ctx.window_width - 4;
    if (columns < NP_MIN_VIEW_COLUMNS) columns = NP_MIN_VIEW_COLUMNS;
    return (size_t)columns;
}

static int np_body_rows(const RetroAppInstance *instance, int body_y) {
    int rows = np_window_height(instance) - 2 - body_y;
    return rows > 0 ? rows : 0;
}

static size_t np_cursor_display_column(const NotepadState *state) {
    if (!state || !state->buffer) return 0;
    size_t row = text_buffer_cursor_row(state->buffer);
    const char *line = text_buffer_line(state->buffer, row);
    if (!line) return 0;
    size_t length = strlen(line);
    size_t byte_column = text_buffer_cursor_col(state->buffer);
    if (byte_column > length) byte_column = length;
    return retro_utf8_columns(line, length, 0, byte_column);
}

static void np_menu_callback(size_t menu_index, size_t item_index, void *user_data) {
    NotepadState *state = (NotepadState *)user_data;
    if (!state) return;
    state->pending_menu_action = NP_MENU_NONE;
    if (menu_index == 0) {
        switch (item_index) {
            case 0:
                state->pending_menu_action = NP_MENU_NEW;
                break;
            case 1:
                state->pending_menu_action = NP_MENU_OPEN;
                break;
            case 2:
                state->pending_menu_action = NP_MENU_SAVE;
                break;
            case 3:
                state->pending_menu_action = NP_MENU_SAVE_AS;
                break;
            case 5:
                state->pending_menu_action = NP_MENU_CLOSE;
                break;
            default:
                break;
        }
    } else if (menu_index == 1) {
        switch (item_index) {
            case 0:
                state->pending_menu_action = NP_MENU_UNDO;
                break;
            case 1:
                state->pending_menu_action = NP_MENU_REDO;
                break;
            case 2:
                state->pending_menu_action = NP_MENU_CUT;
                break;
            case 3:
                state->pending_menu_action = NP_MENU_COPY;
                break;
            case 4:
                state->pending_menu_action = NP_MENU_PASTE;
                break;
            case 5:
                state->pending_menu_action = NP_MENU_SELECT_ALL;
                break;
            case 6:
                state->pending_menu_action = NP_MENU_FIND;
                break;
            default:
                break;
        }
    } else if (menu_index == 2 && item_index == 0) {
        state->pending_menu_action = NP_MENU_TOGGLE_WRAP;
    }
}

static bool np_menu_create(NotepadState *state) {
    if (!state) return false;
    state->menu_bar = menu_bar_create();
    if (!state->menu_bar) return false;

    bool ok =
        menu_bar_add_menu(state->menu_bar, "File", 'F') &&
        menu_bar_add_menu(state->menu_bar, "Edit", 'E') &&
        menu_bar_add_menu(state->menu_bar, "View", 'V') &&
        menu_bar_add_item(state->menu_bar, 0, "New        Ctrl+N", 'N', np_menu_callback, state) &&
        menu_bar_add_item(state->menu_bar, 0, "Open...    Ctrl+O", 'O', np_menu_callback, state) &&
        menu_bar_add_item(state->menu_bar, 0, "Save       Ctrl+S", 'S', np_menu_callback, state) &&
        menu_bar_add_item(state->menu_bar, 0, "Save As... F3", 0, np_menu_callback, state) &&
        menu_bar_add_separator(state->menu_bar, 0) &&
        menu_bar_add_item(state->menu_bar, 0, "Close      Ctrl+W", 'C', np_menu_callback, state) &&
        menu_bar_add_item(state->menu_bar, 1, "Undo       Ctrl+Z", 'U', np_menu_callback, state) &&
        menu_bar_add_item(state->menu_bar, 1, "Redo       Ctrl+Y", 'R', np_menu_callback, state) &&
        menu_bar_add_item(state->menu_bar, 1, "Cut        Ctrl+X", 0, np_menu_callback, state) &&
        menu_bar_add_item(state->menu_bar, 1, "Copy       Ctrl+C", 'C', np_menu_callback, state) &&
        menu_bar_add_item(state->menu_bar, 1, "Paste      Ctrl+V", 'P', np_menu_callback, state) &&
        menu_bar_add_item(state->menu_bar, 1, "Select All Ctrl+A", 'S', np_menu_callback, state) &&
        menu_bar_add_item(state->menu_bar, 1, "Find...    Ctrl+F", 'F', np_menu_callback, state) &&
        menu_bar_add_item(state->menu_bar, 2, "Word Wrap  F4", 'W', np_menu_callback, state);
    if (!ok) {
        menu_bar_destroy(state->menu_bar);
        state->menu_bar = NULL;
        return false;
    }
    return true;
}

static int np_menu_dropdown_x(const NotepadState *state) {
    if (!state || !state->menu_bar || !menu_bar_is_open(state->menu_bar)) {
        return 1;
    }
    size_t open_menu = menu_bar_open_menu(state->menu_bar);
    int x = 3;
    for (size_t index = 0; index < open_menu; ++index) {
        x += (int)strlen(menu_bar_menu_label(state->menu_bar, index)) + 2;
    }
    return x;
}

static void np_set_error(NotepadState *state, RetroFsError error) {
    if (!state) return;
    snprintf(state->error, sizeof(state->error), "Error: %s", retro_fs_error_string(error));
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

static void np_handle_editor_key(RetroAppInstance *instance,
                        NotepadState *state,
                        const RetroKeyEvent *key) {
    if (!instance || !state || !key) return;
    bool may_edit = np_key_may_edit(key);
    NotepadHistoryEntry before = {0};
    bool captured = false;

    if (may_edit) {
        captured = np_history_capture(state, &before);
        if (!captured) np_clear_history(state);
    }

    bool consumed = state->wrap_mode
                        ? text_buffer_handle_key_wrapped(
                    state->buffer, key, np_view_columns(instance))
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
    if (!np_menu_create(state)) {
        free(state->saved_text);
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

static void np_begin_save_as(NotepadState *state, bool close_after_save) {
    if (!state) return;
    state->open_path = false;
    state->search_mode = false;
    state->search_has_match = false;
    state->save_as = true;
    state->close_after_save = close_after_save;
    text_input_clear(state->path_input);
    state->error[0] = '\0';
}

static void np_begin_open_path(NotepadState *state) {
    if (!state) return;
    state->save_as = false;
    state->close_after_save = false;
    state->search_mode = false;
    state->search_has_match = false;
    state->open_path = true;
    text_input_clear(state->path_input);
    state->error[0] = '\0';
}

static void np_begin_search(NotepadState *state) {
    if (!state) return;
    state->save_as = false;
    state->open_path = false;
    state->search_mode = true;
    state->search_has_match = false;
    text_input_clear(state->path_input);
    text_buffer_clear_selection(state->buffer);
    state->error[0] = '\0';
}

static void
np_handle_open_path(RetroAppInstance *instance, NotepadState *state, const RetroKeyEvent *key) {
    if (!instance || !state || !key) return;
    if (key->key_code == RETRO_KEY_ESC) {
        state->open_path = false;
        state->error[0] = '\0';
        return;
    }
    if (key->key_code == RETRO_KEY_CR || key->key_code == RETRO_KEY_LF) {
        const char *path = text_input_text(state->path_input);
        if (!path || !path[0]) {
            snprintf(state->error, sizeof(state->error), "Open: enter a UTF-8 text path");
            return;
        }
        if (!instance->ctx.desktop ||
            !app_launch_with_path(instance->ctx.desktop, "notepad", path)) {
            snprintf(state->error, sizeof(state->error), "Open: unable to open text path");
            return;
        }
        state->open_path = false;
        text_input_clear(state->path_input);
        state->error[0] = '\0';
        return;
    }
    (void)text_input_handle_key(state->path_input, key);
}

static void np_execute_menu_action(RetroAppInstance *instance, NotepadState *state) {
    if (!instance || !state) return;
    NotepadMenuAction action = state->pending_menu_action;
    state->pending_menu_action = NP_MENU_NONE;
    switch (action) {
        case NP_MENU_NEW:
            if (!instance->ctx.desktop || !app_launch(instance->ctx.desktop, "notepad")) {
                snprintf(state->error, sizeof(state->error), "New: desktop unavailable");
            }
            break;
        case NP_MENU_OPEN:
            np_begin_open_path(state);
            break;
        case NP_MENU_SAVE:
            state->close_after_save = false;
            if (state->has_path) {
                (void)np_save(state);
            } else {
                np_begin_save_as(state, false);
            }
            break;
        case NP_MENU_SAVE_AS:
            np_begin_save_as(state, false);
            break;
        case NP_MENU_CLOSE:
            (void)app_request_close(instance);
            break;
        case NP_MENU_UNDO:
            np_undo(state);
            break;
        case NP_MENU_REDO:
            np_redo(state);
            break;
        case NP_MENU_CUT:
            np_cut_selection(state);
            break;
        case NP_MENU_COPY:
            (void)np_copy_selection(state);
            break;
        case NP_MENU_PASTE:
            np_paste_clipboard(state);
            break;
        case NP_MENU_SELECT_ALL:
            text_buffer_select_all(state->buffer);
            state->error[0] = '\0';
            break;
        case NP_MENU_FIND:
            np_begin_search(state);
            break;
        case NP_MENU_TOGGLE_WRAP:
            state->wrap_mode = !state->wrap_mode;
            state->error[0] = '\0';
            break;
        case NP_MENU_NONE:
        default:
            break;
    }
}

static bool
np_route_menu_key(RetroAppInstance *instance, NotepadState *state, const RetroKeyEvent *key) {
    if (!instance || !state || !state->menu_bar || !key) return false;
    bool open = menu_bar_is_open(state->menu_bar);
    bool alt_menu = (key->modifiers & RETRO_MOD_ALT) != 0 && key->is_printable && key->ascii != 0;
    if (!open && key->key_code != RETRO_KEY_F11 && !alt_menu) return false;

    RetroKeyEvent routed = *key;
    if (routed.key_code == RETRO_KEY_F11) routed.key_code = RETRO_KEY_F10;
    if (alt_menu) routed.modifiers = RETRO_MOD_NONE;
    bool consumed = menu_bar_handle_key(state->menu_bar, &routed);
    if (state->pending_menu_action != NP_MENU_NONE) {
        np_execute_menu_action(instance, state);
    }
    return consumed;
}

static void np_event(RetroAppInstance *instance, const RetroEvent *event) {
    if (!instance || !instance->state || !event || event->type != RETRO_EVENT_KEY) {
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

    if (state->open_path) {
        np_handle_open_path(instance, state, key);
        return;
    }

    if (state->search_mode) {
        np_handle_search(state, key);
        return;
    }

    if (np_route_menu_key(instance, state, key)) return;

    if (key->key_code == RETRO_KEY_CTRL_N) {
        state->pending_menu_action = NP_MENU_NEW;
        np_execute_menu_action(instance, state);
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_O) {
        np_begin_open_path(state);
        return;
    }

    if (key->key_code == RETRO_KEY_CTRL_F) {
        np_begin_search(state);
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
            np_begin_save_as(state, false);
        }
        return;
    }

    if (key->key_code == RETRO_KEY_F3) {
        np_begin_save_as(state, false);
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

    np_handle_editor_key(instance, state, key);
}

static void np_render_buffer(const RetroAppInstance *instance,
                             const NotepadState *state,
                             DrawList *draw_list,
                             int y,
                             int rows,
                             const RenderStyle *text,
                             const RenderStyle *cursor,
                             const RenderStyle *selection) {
    if (!instance || !state || !draw_list || rows <= 0 || !text || !cursor || !selection) {
        return;
    }
    int columns = (int)np_view_columns(instance);
    if (state->wrap_mode) {
        text_buffer_render_wrapped_with_selection(
            state->buffer, draw_list, y, 2, rows, columns, text, cursor, selection);
    } else {
        text_buffer_render_with_selection(
            state->buffer, draw_list, y, 2, rows, columns, text, cursor, selection);
    }
}

static void np_render_close_prompt(const NotepadState *state,
                                   DrawList *draw_list,
                                   int window_width,
                                   int window_height,
                                   const RenderStyle *text,
                                   const RenderStyle *accent,
                                   const RenderStyle *selected) {
    if (!state || !draw_list || !text || !accent || !selected) return;
    int box_width = window_width - 6;
    if (box_width > 48) box_width = 48;
    if (box_width < 18) box_width = 18;
    int x = (window_width - box_width) / 2;
    if (x < 1) x = 1;
    int y = window_height / 2 - 2;
    if (y < 3) y = 3;
    if (y + 4 >= window_height - 1) y = window_height - 6;
    if (y < 2) y = 2;
    draw_list_hline(draw_list, y, x, box_width, ' ', selected);
    draw_list_text(draw_list, y, x + 2, "Unsaved changes", selected);
    draw_list_text(draw_list, y + 2, x + 2, "Save changes before closing?", accent);
    draw_list_text(draw_list, y + 4, x + 2, "[S] Save  [D] Discard  [Esc] Cancel", text);
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

    int window_width = np_window_width(instance);
    int window_height = np_window_height(instance);
    int inner_width = window_width - 2;
    if (inner_width < 1) inner_width = 1;
    int status_y = window_height - 2;
    int body_y = 3;
    int body_rows = np_body_rows(instance, body_y);
    int input_width = (int)np_view_columns(instance);
    if (input_width > 240) input_width = 240;

    menu_bar_render(state->menu_bar, draw_list, 1, 1, inner_width, text, cursor, accent, text);

    char title[192];
    const char *name = state->has_path ? retro_fs_path_cstr(&state->path) : "Untitled";
    snprintf(title, sizeof(title), "Notepad — %s%s", name, state->dirty ? " *" : "");
    if (status_y > 2) draw_list_text(draw_list, 2, 2, title, accent);

    if (state->close_prompt) {
        np_render_buffer(instance, state, draw_list, body_y, body_rows, text, cursor, &selection);
        np_render_close_prompt(state, draw_list, window_width, window_height, text, accent, cursor);
    } else if (state->save_as || state->open_path) {
        const char *prompt = state->save_as ? "Save As path (Enter save, Esc cancel):"
                                            : "Open path in new Notepad (Enter open, Esc cancel):";
        if (status_y > 3) draw_list_text(draw_list, 3, 2, prompt, accent);
        if (status_y > 4) {
            text_input_render(state->path_input, draw_list, 4, 2, input_width, text, cursor);
        }
    } else if (state->search_mode) {
        if (status_y > 3) {
            draw_list_text(draw_list, 3, 2, "Find (Enter next, Esc close):", accent);
        }
        if (status_y > 4) {
            text_input_render(state->path_input, draw_list, 4, 2, input_width, text, cursor);
        }
        int search_body_y = 6;
        np_render_buffer(instance,
                         state,
                         draw_list,
                         search_body_y,
                         np_body_rows(instance, search_body_y),
                         text,
                         cursor,
                         &selection);
    } else {
        np_render_buffer(instance, state, draw_list, body_y, body_rows, text, cursor, &selection);
    }

    if (status_y > 1) {
        draw_list_hline(draw_list, status_y, 1, inner_width, ' ', &theme->statusbar);
        char status[256];
        if (state->error[0]) {
            snprintf(status, sizeof(status), " %s", state->error);
        } else {
            snprintf(status,
                     sizeof(status),
                     " Ln %zu, Col %zu | UTF-8 | %s%s",
                     text_buffer_cursor_row(state->buffer) + 1,
                     np_cursor_display_column(state) + 1,
                     state->wrap_mode ? "WRAP" : "NO WRAP",
                     state->dirty ? " | MODIFIED" : "");
        }
        draw_list_text(draw_list, status_y, 1, status, &theme->statusbar);
    }

    if (state->menu_bar && menu_bar_is_open(state->menu_bar)) {
        size_t open_menu = menu_bar_open_menu(state->menu_bar);
        int dropdown_x = np_menu_dropdown_x(state);
        int dropdown_width = menu_bar_dropdown_width(state->menu_bar, open_menu);
        if (dropdown_x + dropdown_width > window_width - 1) {
            dropdown_x = window_width - 1 - dropdown_width;
        }
        if (dropdown_x < 1) dropdown_x = 1;
        (void)menu_bar_render_dropdown(state->menu_bar,
                                       draw_list,
                                       2,
                                       dropdown_x,
                                       text,
                                       cursor,
                                       accent,
                                       accent,
                                       &theme->window_frame_active);
    }
}

static const char *np_recovery_directory(void) {
    const char *directory = getenv("RETRODESK_RECOVERY_DIR");
    if (directory && directory[0]) return directory;
#if defined(_WIN32)
    directory = getenv("TEMP");
    if (directory && directory[0]) return directory;
    directory = getenv("TMP");
    if (directory && directory[0]) return directory;
#else
    directory = getenv("TMPDIR");
    if (directory && directory[0]) return directory;
#endif
    return ".";
}

static bool np_recovery_path(const RetroAppInstance *instance,
                             const NotepadState *state,
                             unsigned int attempt,
                             char *path, size_t path_size) {
    if (!instance || !state || !path || path_size == 0) return false;
    const char *directory = np_recovery_directory();
    size_t directory_length = strlen(directory);
    const char *separator =
        directory_length > 0 &&
                (directory[directory_length - 1] == '/' ||
                 directory[directory_length - 1] == '\\')
            ? ""
            : "/";
    long long timestamp = (long long)time(NULL);
    unsigned long long identity =
        (unsigned long long)(uintptr_t)state;
    int written = snprintf(
        path, path_size,
        "%s%sretrodesk-notepad-recovery-%lld-%llx-%u.txt",
        directory, separator, timestamp, identity, attempt);
    return written > 0 && (size_t)written < path_size;
}

static bool np_write_recovery(RetroAppInstance *instance,
                              NotepadState *state) {
    if (!instance || !state || !state->dirty || !state->buffer ||
        !instance->ctx.desktop) {
        return true;
    }

    size_t length = 0;
    char *text = text_buffer_to_text(state->buffer, &length);
    if (!text) return false;

    bool recovered = false;
    char path[NP_RECOVERY_PATH_CAP];
    for (unsigned int attempt = 0;
         attempt < NP_RECOVERY_ATTEMPTS; ++attempt) {
        if (!np_recovery_path(instance, state, attempt,
                              path, sizeof(path))) {
            break;
        }

        FILE *existing = fopen(path, "rb");
        if (existing) {
            fclose(existing);
            continue;
        }

        FILE *stream = fopen(path, "wb");
        if (!stream) continue;
        size_t stored = fwrite(text, 1, length, stream);
        bool flushed = fflush(stream) == 0;
        bool closed = fclose(stream) == 0;
        if (stored == length && flushed && closed) {
            fprintf(stderr, "\nRetroDesk recovered unsaved Notepad to %s\n",
                    path);
            fflush(stderr);
            recovered = true;
            break;
        }
        (void)remove(path);
    }

    free(text);
    return recovered;
}

static void np_destroy(RetroAppInstance *instance) {
    if (!instance) return;
    NotepadState *state = instance->state;
    if (!state) return;
    (void)np_write_recovery(instance, state);
    np_clear_history(state);
    menu_bar_destroy(state->menu_bar);
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

bool notepad_menu_open_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return false;
    const NotepadState *state = instance->state;
    return state->menu_bar && menu_bar_is_open(state->menu_bar);
}

bool notepad_open_path_for_test(const RetroAppInstance *instance) {
    if (!instance || !instance->state) return false;
    const NotepadState *state = instance->state;
    return state->open_path;
}

size_t notepad_view_columns_for_test(const RetroAppInstance *instance) {
    return np_view_columns(instance);
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
