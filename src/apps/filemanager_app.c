#include "app/app_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core/desktop.h"
#include "core/key_chord.h"
#include "storage/retro_fs.h"

enum { FM_MAX_ENTRIES = 10000 };

typedef struct FileManagerItem {
    char *name;
    mode_t mode;
} FileManagerItem;

typedef struct FileManagerState {
    RetroFsPath cwd;
    FileManagerItem *items;
    size_t count;
    size_t selected;
    char error[160];
} FileManagerState;

static void fm_clear_items(FileManagerState *state) {
    if (!state) return;
    for (size_t i = 0; i < state->count; ++i) free(state->items[i].name);
    free(state->items);
    state->items = NULL;
    state->count = 0;
    state->selected = 0;
}

static bool fm_collect(const RetroFsEntry *entry, void *userdata) {
    FileManagerState *state = userdata;
    if (!state || !entry || !entry->name) return false;
    FileManagerItem *next = realloc(state->items,
                                    (state->count + 1) * sizeof(*next));
    if (!next) return false;
    state->items = next;
    state->items[state->count].name = strdup(entry->name);
    if (!state->items[state->count].name) return false;
    state->items[state->count].mode = entry->mode;
    state->count++;
    return true;
}

static void fm_set_error(FileManagerState *state, RetroFsError error) {
    if (!state) return;
    snprintf(state->error, sizeof(state->error), "Error: %s",
             retro_fs_error_string(error));
}

static bool fm_reload(FileManagerState *state) {
    if (!state) return false;
    size_t old_selected = state->selected;
    fm_clear_items(state);
    state->error[0] = '\0';
    RetroFsError error = retro_fs_list(&state->cwd, FM_MAX_ENTRIES,
                                       fm_collect, state);
    if (error != RETRO_FS_OK) {
        fm_clear_items(state);
        fm_set_error(state, error);
        return false;
    }
    if (state->count > 0) {
        state->selected = old_selected < state->count ? old_selected : state->count - 1;
    }
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
    return fm_reload(state);
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
        return fm_reload(state);
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
    RetroFsError error = retro_fs_path_init(&state->cwd, home && *home ? home : ".");
    if (error != RETRO_FS_OK) {
        free(state);
        return false;
    }
    instance->state = state;
    if (!fm_reload(state)) {
        /* A missing/unreadable HOME should not make the desktop fail to boot. */
        state->error[0] = '\0';
        retro_fs_path_destroy(&state->cwd);
        if (retro_fs_path_init(&state->cwd, ".") != RETRO_FS_OK || !fm_reload(state)) {
            free(state);
            instance->state = NULL;
            return false;
        }
    }
    return true;
}

static void fm_event(RetroAppInstance *instance, const RetroEvent *event) {
    if (!instance || !instance->state || !event || event->type != RETRO_EVENT_KEY) return;
    FileManagerState *state = instance->state;
    int key = event->data.key.key_code;
    if (key == RETRO_KEY_UP || key == 'k' || key == 'K') {
        if (state->selected > 0) state->selected--;
    } else if (key == RETRO_KEY_DOWN || key == 'j' || key == 'J') {
        if (state->selected + 1 < state->count) state->selected++;
    } else if (key == RETRO_KEY_HOME) {
        state->selected = 0;
    } else if (key == RETRO_KEY_END) {
        state->selected = state->count ? state->count - 1 : 0;
    } else if (key == RETRO_KEY_BS) {
        (void)fm_parent(state);
    } else if (key == RETRO_KEY_F5) {
        (void)fm_reload(state);
    } else if (key == RETRO_KEY_CR || key == RETRO_KEY_LF) {
        (void)fm_open_selected(instance);
    } else if (key == RETRO_KEY_ESC) {
        app_request_close(instance);
    }
}

static void fm_render(RetroAppInstance *instance, DrawList *draw_list) {
    if (!instance || !instance->state || !draw_list) return;
    FileManagerState *state = instance->state;
    const RetroTheme *theme = instance->ctx.theme;
    const RenderStyle *text = &theme->window_body;
    const RenderStyle *accent = &theme->shell_accent;
    const RenderStyle *selected = &theme->launcher_selected;
    draw_list_text(draw_list, 1, 2, retro_fs_path_cstr(&state->cwd), accent);
    draw_list_text(draw_list, 2, 2, "Arrows/jk move | Enter open | Backspace parent | F5 refresh",
                   text);
    size_t visible = state->count < 12 ? state->count : 12;
    for (size_t i = 0; i < visible; ++i) {
        char line[192];
        bool dir = S_ISDIR(state->items[i].mode);
        snprintf(line, sizeof(line), "%c %s%s", i == state->selected ? '>' : ' ',
                 state->items[i].name, dir ? "/" : "");
        draw_list_text(draw_list, 4 + (int)i, 2, line,
                       i == state->selected ? selected : text);
    }
    if (state->error[0]) draw_list_text(draw_list, 17, 2, state->error, accent);
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
