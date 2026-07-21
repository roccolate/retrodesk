#include "app/app_runtime.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/desktop.h"
#include "core/key_chord.h"
#include "storage/retro_fs.h"
#include "ui/text_input.h"

enum {
    FM_MAX_ENTRIES = 10000,
    FM_VISIBLE_ROWS = 12,
    FM_PAGE_STEP = 10,
    FM_SELECTED_NAME_MAX = 256,
    FM_INPUT_MAX = 255,
};

typedef enum FileManagerPromptMode {
    FM_PROMPT_NONE = 0,
    FM_PROMPT_RENAME,
    FM_PROMPT_NEW_DIRECTORY,
    FM_PROMPT_NEW_FILE,
} FileManagerPromptMode;

typedef struct FileManagerItem {
    char *name;
    RetroFsKind kind;
    uint64_t size;
} FileManagerItem;

typedef struct FileManagerState {
    RetroFsPath cwd;
    FileManagerItem *items;
    size_t count;
    size_t selected;
    size_t scroll_offset;
    bool show_hidden;
    RetroFsError collect_error;
    TextInput *name_input;
    FileManagerPromptMode prompt_mode;
    char error[160];
} FileManagerState;

static bool fm_is_hidden_name(const char *name) {
    return name && name[0] == '.' && strcmp(name, "..") != 0;
}

static void fm_clear_items(FileManagerState *state) {
    if (!state) return;
    for (size_t i = 0; i < state->count; ++i) free(state->items[i].name);
    free(state->items);
    state->items = NULL;
    state->count = 0;
    state->selected = 0;
    state->scroll_offset = 0;
}

static void fm_clamp_view(FileManagerState *state) {
    if (!state) return;
    if (state->count == 0) {
        state->selected = 0;
        state->scroll_offset = 0;
        return;
    }

    if (state->selected >= state->count) state->selected = state->count - 1;
    if (state->selected < state->scroll_offset) {
        state->scroll_offset = state->selected;
    } else if (state->selected >= state->scroll_offset + FM_VISIBLE_ROWS) {
        state->scroll_offset = state->selected - FM_VISIBLE_ROWS + 1;
    }

    size_t max_scroll = state->count > FM_VISIBLE_ROWS
                            ? state->count - FM_VISIBLE_ROWS
                            : 0;
    if (state->scroll_offset > max_scroll) state->scroll_offset = max_scroll;
}

static bool fm_collect(const RetroFsEntry *entry, void *userdata) {
    FileManagerState *state = userdata;
    if (!state || !entry || !entry->name) return false;
    if (!state->show_hidden && fm_is_hidden_name(entry->name)) return true;

    FileManagerItem *next = realloc(state->items,
                                    (state->count + 1) * sizeof(*next));
    if (!next) {
        state->collect_error = RETRO_FS_OOM;
        return false;
    }
    state->items = next;
    state->items[state->count].name = strdup(entry->name);
    if (!state->items[state->count].name) {
        state->collect_error = RETRO_FS_OOM;
        return false;
    }
    state->items[state->count].kind = entry->kind;
    state->items[state->count].size = entry->size;
    state->count++;
    return true;
}

static void fm_set_error(FileManagerState *state, RetroFsError error) {
    if (!state) return;
    snprintf(state->error, sizeof(state->error), "Error: %s",
             retro_fs_error_string(error));
}

static void fm_set_name_error(FileManagerState *state) {
    if (!state) return;
    snprintf(state->error, sizeof(state->error),
             "Invalid name: enter one filename without / or \\ characters");
}

static bool fm_copy_selected_name(const FileManagerState *state, char *out,
                                  size_t out_size) {
    if (!state || !out || out_size == 0 || state->count == 0 ||
        state->selected >= state->count || !state->items[state->selected].name) {
        return false;
    }
    int written = snprintf(out, out_size, "%s", state->items[state->selected].name);
    return written >= 0 && (size_t)written < out_size;
}

static size_t fm_find_name(const FileManagerState *state, const char *name) {
    if (!state || !name) return state ? state->count : 0;
    for (size_t i = 0; i < state->count; ++i) {
        if (strcmp(state->items[i].name, name) == 0) return i;
    }
    return state->count;
}

static void fm_select_name(FileManagerState *state, const char *name) {
    if (!state || !name) return;
    size_t selected = fm_find_name(state, name);
    if (selected < state->count) state->selected = selected;
    fm_clamp_view(state);
}

static bool fm_reload(FileManagerState *state, bool preserve_selection) {
    if (!state) return false;

    size_t old_selected = state->selected;
    char selected_name[FM_SELECTED_NAME_MAX] = {0};
    bool have_selected_name =
        preserve_selection &&
        fm_copy_selected_name(state, selected_name, sizeof(selected_name));
    fm_clear_items(state);
    state->error[0] = '\0';
    state->collect_error = RETRO_FS_OK;

    RetroFsError error = retro_fs_list(&state->cwd, FM_MAX_ENTRIES,
                                       fm_collect, state);
    if (error == RETRO_FS_OK && state->collect_error != RETRO_FS_OK) {
        error = state->collect_error;
    }
    if (error != RETRO_FS_OK) {
        fm_clear_items(state);
        fm_set_error(state, error);
        return false;
    }

    if (state->count > 0 && preserve_selection) {
        size_t restored = have_selected_name
                              ? fm_find_name(state, selected_name)
                              : state->count;
        state->selected = restored < state->count
                              ? restored
                              : (old_selected < state->count
                                     ? old_selected
                                     : state->count - 1);
    }
    fm_clamp_view(state);
    return true;
}

static bool fm_parent(FileManagerState *state) {
    if (!state) return false;
    RetroFsPath parent = {0};
    RetroFsError error = retro_fs_path_parent(&state->cwd, &parent);
    if (error != RETRO_FS_OK) {
        fm_set_error(state, error);
        return false;
    }
    retro_fs_path_destroy(&state->cwd);
    state->cwd = parent;
    return fm_reload(state, false);
}

static bool fm_open_selected(RetroAppInstance *instance) {
    if (!instance || !instance->state) return false;
    FileManagerState *state = instance->state;
    if (state->count == 0 || state->selected >= state->count) return false;
    const char *name = state->items[state->selected].name;
    if (strcmp(name, "..") == 0) return fm_parent(state);

    RetroFsPath selected = {0};
    RetroFsError error = retro_fs_path_join(&state->cwd, name, &selected);
    if (error != RETRO_FS_OK) {
        fm_set_error(state, error);
        return false;
    }
    RetroFsVersion version = {0};
    error = retro_fs_stat(&selected, &version);
    if (error != RETRO_FS_OK) {
        retro_fs_path_destroy(&selected);
        fm_set_error(state, error);
        return false;
    }
    if (version.kind == RETRO_FS_KIND_DIRECTORY) {
        retro_fs_path_destroy(&state->cwd);
        state->cwd = selected;
        return fm_reload(state, false);
    }
    if (version.kind == RETRO_FS_KIND_REGULAR) {
        RetroAppInstance *opened = app_launch_with_path(
            instance->ctx.desktop, "notepad", retro_fs_path_cstr(&selected));
        retro_fs_path_destroy(&selected);
        if (!opened) fm_set_error(state, RETRO_FS_IO);
        return opened != NULL;
    }
    retro_fs_path_destroy(&selected);
    fm_set_error(state, RETRO_FS_UNSUPPORTED);
    return false;
}

static void fm_cancel_prompt(FileManagerState *state) {
    if (!state) return;
    state->prompt_mode = FM_PROMPT_NONE;
    text_input_clear(state->name_input);
}

static bool fm_normalize_name(const char *input, char *out, size_t out_size) {
    if (!input || !out || out_size == 0) return false;

    const char *start = input;
    while (*start && isspace((unsigned char)*start)) start++;
    const char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) end--;

    size_t length = (size_t)(end - start);
    if (length == 0 || length >= out_size) return false;
    if ((length == 1 && start[0] == '.') ||
        (length == 2 && start[0] == '.' && start[1] == '.')) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (start[i] == '/' || start[i] == '\\') return false;
    }

    memcpy(out, start, length);
    out[length] = '\0';
    return true;
}

static void fm_start_prompt(FileManagerState *state,
                            FileManagerPromptMode prompt_mode) {
    if (!state || !state->name_input || prompt_mode == FM_PROMPT_NONE) return;

    if (prompt_mode == FM_PROMPT_RENAME) {
        if (state->count == 0 || state->selected >= state->count ||
            strcmp(state->items[state->selected].name, "..") == 0) {
            snprintf(state->error, sizeof(state->error),
                     "Select a file or folder before renaming");
            return;
        }
        text_input_set_text(state->name_input,
                            state->items[state->selected].name);
    } else {
        text_input_clear(state->name_input);
    }

    state->error[0] = '\0';
    state->prompt_mode = prompt_mode;
}

static bool fm_commit_prompt(FileManagerState *state) {
    if (!state || state->prompt_mode == FM_PROMPT_NONE) return false;

    char name[FM_SELECTED_NAME_MAX];
    if (!fm_normalize_name(text_input_text(state->name_input), name,
                           sizeof(name))) {
        fm_set_name_error(state);
        return false;
    }

    RetroFsError error = RETRO_FS_INVALID_ARGUMENT;
    RetroFsPath source = {0};
    RetroFsPath destination = {0};

    if (state->prompt_mode == FM_PROMPT_RENAME) {
        if (state->count == 0 || state->selected >= state->count) {
            fm_set_error(state, RETRO_FS_NOT_FOUND);
            return false;
        }
        const char *old_name = state->items[state->selected].name;
        if (strcmp(old_name, "..") == 0) {
            fm_set_error(state, RETRO_FS_UNSUPPORTED);
            return false;
        }
        if (strcmp(old_name, name) == 0) {
            fm_cancel_prompt(state);
            state->error[0] = '\0';
            return true;
        }

        error = retro_fs_path_join(&state->cwd, old_name, &source);
        if (error == RETRO_FS_OK) {
            error = retro_fs_path_join(&state->cwd, name, &destination);
        }
        if (error == RETRO_FS_OK) {
            error = retro_fs_rename(&source, &destination);
        }
    } else {
        error = retro_fs_path_join(&state->cwd, name, &destination);
        if (error == RETRO_FS_OK) {
            error = state->prompt_mode == FM_PROMPT_NEW_DIRECTORY
                        ? retro_fs_create_directory(&destination)
                        : retro_fs_create_file(&destination);
        }
    }

    retro_fs_path_destroy(&source);
    retro_fs_path_destroy(&destination);
    if (error != RETRO_FS_OK) {
        fm_set_error(state, error);
        return false;
    }

    fm_cancel_prompt(state);
    if (!fm_reload(state, false)) return false;
    fm_select_name(state, name);
    state->error[0] = '\0';
    return true;
}

static bool fm_create(RetroAppInstance *instance, const RetroAppContext *ctx) {
    if (!instance || !ctx) return false;
    FileManagerState *state = calloc(1, sizeof(*state));
    if (!state) return false;

    state->name_input = text_input_create(FM_INPUT_MAX);
    if (!state->name_input) {
        free(state);
        return false;
    }

    const char *home = getenv("HOME");
    const char *start = ctx->resource_path && ctx->resource_path[0]
                            ? ctx->resource_path
                            : (home && *home ? home : ".");
    RetroFsError error = retro_fs_path_init(&state->cwd, start);
    if (error != RETRO_FS_OK) {
        text_input_destroy(state->name_input);
        free(state);
        return false;
    }
    instance->state = state;
    if (!fm_reload(state, false)) {
        /* A missing/unreadable start path should not make the desktop fail to boot. */
        state->error[0] = '\0';
        retro_fs_path_destroy(&state->cwd);
        if (retro_fs_path_init(&state->cwd, ".") != RETRO_FS_OK ||
            !fm_reload(state, false)) {
            fm_clear_items(state);
            retro_fs_path_destroy(&state->cwd);
            text_input_destroy(state->name_input);
            free(state);
            instance->state = NULL;
            return false;
        }
    }
    return true;
}

static void fm_move_selection(FileManagerState *state, size_t amount,
                              bool forward) {
    if (!state || state->count == 0 || amount == 0) return;
    if (!forward) {
        state->selected = amount > state->selected ? 0 : state->selected - amount;
    } else {
        size_t last = state->count - 1;
        state->selected = amount > last - state->selected
                              ? last
                              : state->selected + amount;
    }
    fm_clamp_view(state);
}

static void fm_prompt_event(FileManagerState *state, const RetroEvent *event) {
    if (!state || !event || state->prompt_mode == FM_PROMPT_NONE) return;
    int key = event->data.key.key_code;
    if (key == RETRO_KEY_ESC) {
        fm_cancel_prompt(state);
        state->error[0] = '\0';
        return;
    }
    if (key == RETRO_KEY_CR || key == RETRO_KEY_LF) {
        (void)fm_commit_prompt(state);
        return;
    }
    if (text_input_handle_key(state->name_input, &event->data.key)) {
        state->error[0] = '\0';
    }
}

static void fm_event(RetroAppInstance *instance, const RetroEvent *event) {
    if (!instance || !instance->state || !event || event->type != RETRO_EVENT_KEY) return;
    FileManagerState *state = instance->state;
    int key = event->data.key.key_code;
    unsigned char ch = event->data.key.ascii;

    if (state->prompt_mode != FM_PROMPT_NONE) {
        fm_prompt_event(state, event);
        return;
    }

    if (key == RETRO_KEY_F2) {
        fm_start_prompt(state, FM_PROMPT_RENAME);
    } else if (key == RETRO_KEY_F7) {
        fm_start_prompt(state, FM_PROMPT_NEW_DIRECTORY);
    } else if (key == RETRO_KEY_F8) {
        fm_start_prompt(state, FM_PROMPT_NEW_FILE);
    } else if (key == RETRO_KEY_UP || ch == 'k' || ch == 'K') {
        fm_move_selection(state, 1, false);
    } else if (key == RETRO_KEY_DOWN || ch == 'j' || ch == 'J') {
        fm_move_selection(state, 1, true);
    } else if (key == RETRO_KEY_PPAGE) {
        fm_move_selection(state, FM_PAGE_STEP, false);
    } else if (key == RETRO_KEY_NPAGE) {
        fm_move_selection(state, FM_PAGE_STEP, true);
    } else if (key == RETRO_KEY_HOME) {
        state->selected = 0;
        fm_clamp_view(state);
    } else if (key == RETRO_KEY_END) {
        state->selected = state->count ? state->count - 1 : 0;
        fm_clamp_view(state);
    } else if (key == RETRO_KEY_BS) {
        (void)fm_parent(state);
    } else if (key == RETRO_KEY_F5) {
        (void)fm_reload(state, true);
    } else if (ch == 'h' || ch == 'H') {
        state->show_hidden = !state->show_hidden;
        (void)fm_reload(state, true);
    } else if (key == RETRO_KEY_CR || key == RETRO_KEY_LF) {
        (void)fm_open_selected(instance);
    } else if (key == RETRO_KEY_ESC) {
        app_request_close(instance);
    }
}

static void fm_format_size(uint64_t size, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    static const char *const units[] = {"B", "K", "M", "G", "T"};
    double value = (double)size;
    size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < sizeof(units) / sizeof(units[0])) {
        value /= 1024.0;
        unit++;
    }
    if (unit == 0) {
        snprintf(out, out_size, "%.0f%s", value, units[unit]);
    } else {
        snprintf(out, out_size, "%.1f%s", value, units[unit]);
    }
}

static size_t fm_content_count(const FileManagerState *state) {
    if (!state || state->count == 0) return 0;
    return strcmp(state->items[0].name, "..") == 0 ? state->count - 1 : state->count;
}

static const char *fm_prompt_label(FileManagerPromptMode prompt_mode) {
    switch (prompt_mode) {
    case FM_PROMPT_RENAME:
        return "Rename to:";
    case FM_PROMPT_NEW_DIRECTORY:
        return "New folder:";
    case FM_PROMPT_NEW_FILE:
        return "New file:";
    case FM_PROMPT_NONE:
    default:
        return "";
    }
}

static void fm_render(RetroAppInstance *instance, DrawList *draw_list) {
    if (!instance || !instance->state || !draw_list || !instance->ctx.theme) return;
    FileManagerState *state = instance->state;
    const RetroTheme *theme = instance->ctx.theme;
    const RenderStyle *text = &theme->window_body;
    const RenderStyle *accent = &theme->shell_accent;
    const RenderStyle *selected = &theme->launcher_selected;

    draw_list_text(draw_list, 1, 2, retro_fs_path_cstr(&state->cwd), accent);
    draw_list_text(draw_list, 2, 2,
                   "Arrows/jk/Pg move | Enter open | Backspace parent", text);
    draw_list_text(draw_list, 3, 2,
                   "F2 rename | F7 new folder | F8 new file | F5 refresh | H hidden",
                   text);

    for (size_t row = 0; row < FM_VISIBLE_ROWS; ++row) {
        size_t index = state->scroll_offset + row;
        if (index >= state->count) break;

        char line[192];
        const FileManagerItem *item = &state->items[index];
        if (item->kind == RETRO_FS_KIND_DIRECTORY) {
            snprintf(line, sizeof(line), "[D] %.60s/", item->name);
        } else {
            char size[16];
            const char *marker = item->kind == RETRO_FS_KIND_REGULAR
                                     ? "F"
                                     : (item->kind == RETRO_FS_KIND_SYMLINK
                                            ? "L"
                                            : "?");
            fm_format_size(item->size, size, sizeof(size));
            snprintf(line, sizeof(line), "[%s] %-52.52s %8s",
                     marker, item->name, size);
        }
        draw_list_text(draw_list, 4 + (int)row, 2, line,
                       index == state->selected ? selected : text);
    }

    if (fm_content_count(state) == 0 && !state->error[0]) {
        draw_list_text(draw_list, 5, 4, "(empty directory)", text);
    }

    if (state->prompt_mode != FM_PROMPT_NONE) {
        draw_list_text(draw_list, 16, 2, fm_prompt_label(state->prompt_mode), accent);
        text_input_render(state->name_input, draw_list, 16, 16, 52, text, selected);
    }

    if (state->error[0]) {
        draw_list_text(draw_list, 17, 2, state->error, accent);
    } else {
        char status[160];
        size_t position = state->count ? state->selected + 1 : 0;
        snprintf(status, sizeof(status), "%zu items | %zu/%zu | hidden: %s",
                 fm_content_count(state), position, state->count,
                 state->show_hidden ? "on" : "off");
        draw_list_text(draw_list, 17, 2, status, accent);
    }
}

static void fm_destroy(RetroAppInstance *instance) {
    if (!instance) return;
    FileManagerState *state = instance->state;
    if (!state) return;
    fm_clear_items(state);
    text_input_destroy(state->name_input);
    retro_fs_path_destroy(&state->cwd);
    free(state);
    instance->state = NULL;
}

size_t filemanager_item_count_for_test(const RetroAppInstance *instance) {
    const FileManagerState *state = instance ? instance->state : NULL;
    return state ? state->count : 0;
}

const char *filemanager_selected_name_for_test(const RetroAppInstance *instance) {
    const FileManagerState *state = instance ? instance->state : NULL;
    if (!state || state->selected >= state->count) return NULL;
    return state->items[state->selected].name;
}

size_t filemanager_scroll_offset_for_test(const RetroAppInstance *instance) {
    const FileManagerState *state = instance ? instance->state : NULL;
    return state ? state->scroll_offset : 0;
}

bool filemanager_show_hidden_for_test(const RetroAppInstance *instance) {
    const FileManagerState *state = instance ? instance->state : NULL;
    return state && state->show_hidden;
}

bool filemanager_has_item_for_test(const RetroAppInstance *instance, const char *name) {
    const FileManagerState *state = instance ? instance->state : NULL;
    return state && fm_find_name(state, name) < state->count;
}

const RetroAppDescriptor *filemanager_app_descriptor(void) {
    static const RetroAppDescriptor desc = {
        .app_id = "filemanager",
        .display_name = "File Manager",
        .required_capabilities = PLATFORM_CAP_KEYBOARD_BASIC,
        .default_height = 20,
        .default_width = 72,
        .default_y = 2,
        .default_x = 2,
        .window_flags = WINDOW_FLAG_APP_OWNED,
        .create = fm_create,
        .on_event = fm_event,
        .on_render = fm_render,
        .destroy = fm_destroy,
    };
    return &desc;
}
