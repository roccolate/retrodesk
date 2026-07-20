#include "app/app_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core/desktop.h"
#include "core/key_chord.h"
#include "storage/retro_fs.h"

enum {
    FM_MAX_ENTRIES = 10000,
    FM_VISIBLE_ROWS = 12,
    FM_PAGE_STEP = 10,
};

typedef struct FileManagerItem {
    char *name;
    mode_t mode;
    off_t size;
} FileManagerItem;

typedef struct FileManagerState {
    RetroFsPath cwd;
    FileManagerItem *items;
    size_t count;
    size_t selected;
    size_t scroll_offset;
    bool show_hidden;
    RetroFsError collect_error;
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
    state->items[state->count].mode = entry->mode;
    state->items[state->count].size = entry->size;
    state->count++;
    return true;
}

static void fm_set_error(FileManagerState *state, RetroFsError error) {
    if (!state) return;
    snprintf(state->error, sizeof(state->error), "Error: %s",
             retro_fs_error_string(error));
}

static char *fm_selected_name_dup(const FileManagerState *state) {
    if (!state || state->count == 0 || state->selected >= state->count) return NULL;
    return strdup(state->items[state->selected].name);
}

static size_t fm_find_name(const FileManagerState *state, const char *name) {
    if (!state || !name) return state ? state->count : 0;
    for (size_t i = 0; i < state->count; ++i) {
        if (strcmp(state->items[i].name, name) == 0) return i;
    }
    return state->count;
}

static bool fm_reload(FileManagerState *state, bool preserve_selection) {
    if (!state) return false;

    size_t old_selected = state->selected;
    char *selected_name = preserve_selection ? fm_selected_name_dup(state) : NULL;
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
        free(selected_name);
        return false;
    }

    if (state->count > 0 && preserve_selection) {
        size_t restored = fm_find_name(state, selected_name);
        state->selected = restored < state->count
                              ? restored
                              : (old_selected < state->count
                                     ? old_selected
                                     : state->count - 1);
    }
    fm_clamp_view(state);
    free(selected_name);
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
    if (S_ISDIR(version.mode)) {
        retro_fs_path_destroy(&state->cwd);
        state->cwd = selected;
        return fm_reload(state, false);
    }
    if (S_ISREG(version.mode)) {
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

static bool fm_create(RetroAppInstance *instance, const RetroAppContext *ctx) {
    if (!instance || !ctx) return false;
    FileManagerState *state = calloc(1, sizeof(*state));
    if (!state) return false;

    const char *home = getenv("HOME");
    const char *start = ctx->resource_path && ctx->resource_path[0]
                            ? ctx->resource_path
                            : (home && *home ? home : ".");
    RetroFsError error = retro_fs_path_init(&state->cwd, start);
    if (error != RETRO_FS_OK) {
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
            free(state);
            instance->state = NULL;
            return false;
        }
    }
    return true;
}

static void fm_move_selection(FileManagerState *state, int delta) {
    if (!state || state->count == 0 || delta == 0) return;
    if (delta < 0) {
        size_t amount = (size_t)(-delta);
        state->selected = amount > state->selected ? 0 : state->selected - amount;
    } else {
        size_t amount = (size_t)delta;
        size_t last = state->count - 1;
        state->selected = amount > last - state->selected
                              ? last
                              : state->selected + amount;
    }
    fm_clamp_view(state);
}

static void fm_event(RetroAppInstance *instance, const RetroEvent *event) {
    if (!instance || !instance->state || !event || event->type != RETRO_EVENT_KEY) return;
    FileManagerState *state = instance->state;
    int key = event->data.key.key_code;
    unsigned char ch = event->data.key.ascii;

    if (key == RETRO_KEY_UP || ch == 'k' || ch == 'K') {
        fm_move_selection(state, -1);
    } else if (key == RETRO_KEY_DOWN || ch == 'j' || ch == 'J') {
        fm_move_selection(state, 1);
    } else if (key == RETRO_KEY_PPAGE) {
        fm_move_selection(state, -FM_PAGE_STEP);
    } else if (key == RETRO_KEY_NPAGE) {
        fm_move_selection(state, FM_PAGE_STEP);
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

static void fm_format_size(off_t size, char *out, size_t out_size) {
    static const char *const units[] = {"B", "K", "M", "G", "T"};
    double value = size < 0 ? 0.0 : (double)size;
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

static void fm_render(RetroAppInstance *instance, DrawList *draw_list) {
    if (!instance || !instance->state || !draw_list) return;
    FileManagerState *state = instance->state;
    const RetroTheme *theme = instance->ctx.theme;
    const RenderStyle *text = &theme->window_body;
    const RenderStyle *accent = &theme->shell_accent;
    const RenderStyle *selected = &theme->launcher_selected;

    draw_list_text(draw_list, 1, 2, retro_fs_path_cstr(&state->cwd), accent);
    draw_list_text(draw_list, 2, 2,
                   "Arrows/jk/PgUp/PgDn | Enter open | Backspace parent | F5 refresh | H hidden",
                   text);

    for (size_t row = 0; row < FM_VISIBLE_ROWS; ++row) {
        size_t index = state->scroll_offset + row;
        if (index >= state->count) break;

        char line[192];
        const FileManagerItem *item = &state->items[index];
        if (S_ISDIR(item->mode)) {
            snprintf(line, sizeof(line), "[D] %.60s/", item->name);
        } else {
            char size[16];
            fm_format_size(item->size, size, sizeof(size));
            snprintf(line, sizeof(line), "[F] %-52.52s %8s", item->name, size);
        }
        draw_list_text(draw_list, 4 + (int)row, 2, line,
                       index == state->selected ? selected : text);
    }

    if (fm_content_count(state) == 0 && !state->error[0]) {
        draw_list_text(draw_list, 5, 4, "(empty directory)", text);
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
