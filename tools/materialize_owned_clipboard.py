#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

CLIPBOARD_H = r'''#ifndef RETRODESK_CORE_CLIPBOARD_H
#define RETRODESK_CORE_CLIPBOARD_H

#include <stdbool.h>
#include <stddef.h>

/* Desktop-owned, backend-neutral clipboard. Stored text is validated UTF-8.
   Returned text remains owned by the service and is invalidated by the next
   successful set or clear operation. */
typedef struct RetroClipboard RetroClipboard;

RetroClipboard *retro_clipboard_create(void);
void retro_clipboard_destroy(RetroClipboard *clipboard);
bool retro_clipboard_set_text(RetroClipboard *clipboard,
                              const char *text, size_t length);
const char *retro_clipboard_text(const RetroClipboard *clipboard,
                                 size_t *length);
bool retro_clipboard_has_text(const RetroClipboard *clipboard);
void retro_clipboard_clear(RetroClipboard *clipboard);

#endif
'''

CLIPBOARD_C = r'''#include "core/clipboard.h"

#include <stdlib.h>
#include <string.h>

#include "core/utf8.h"

struct RetroClipboard {
    char *text;
    size_t length;
};

RetroClipboard *retro_clipboard_create(void) {
    return calloc(1, sizeof(RetroClipboard));
}

void retro_clipboard_destroy(RetroClipboard *clipboard) {
    if (!clipboard) return;
    free(clipboard->text);
    clipboard->text = NULL;
    clipboard->length = 0;
    free(clipboard);
}

bool retro_clipboard_set_text(RetroClipboard *clipboard,
                              const char *text, size_t length) {
    if (!clipboard || (!text && length != 0) ||
        (length > 0 && memchr(text, '\0', length) != NULL) ||
        !retro_utf8_validate(text, length)) {
        return false;
    }

    char *copy = malloc(length + 1);
    if (!copy) return false;
    if (length > 0) memcpy(copy, text, length);
    copy[length] = '\0';

    free(clipboard->text);
    clipboard->text = copy;
    clipboard->length = length;
    return true;
}

const char *retro_clipboard_text(const RetroClipboard *clipboard,
                                 size_t *length) {
    if (length) *length = clipboard ? clipboard->length : 0;
    return clipboard && clipboard->text ? clipboard->text : "";
}

bool retro_clipboard_has_text(const RetroClipboard *clipboard) {
    return clipboard && clipboard->text && clipboard->length > 0;
}

void retro_clipboard_clear(RetroClipboard *clipboard) {
    if (!clipboard) return;
    free(clipboard->text);
    clipboard->text = NULL;
    clipboard->length = 0;
}
'''

CLIPBOARD_TEST = r'''#include "test_harness.h"

#include <string.h>

#include "core/clipboard.h"

int main(void) {
    RetroClipboard *first = retro_clipboard_create();
    RetroClipboard *second = retro_clipboard_create();
    TEST_REQUIRE(first != NULL);
    TEST_REQUIRE(second != NULL);
    TEST_REQUIRE(!retro_clipboard_has_text(first));
    TEST_REQUIRE(!retro_clipboard_has_text(second));

    const char *utf8 = "niño\npingüino";
    TEST_REQUIRE(retro_clipboard_set_text(first, utf8, strlen(utf8)));
    TEST_REQUIRE(retro_clipboard_has_text(first));
    TEST_REQUIRE(!retro_clipboard_has_text(second));

    size_t length = 0;
    const char *stored = retro_clipboard_text(first, &length);
    TEST_REQUIRE(length == strlen(utf8));
    TEST_REQUIRE(strcmp(stored, utf8) == 0);
    TEST_REQUIRE(strcmp(retro_clipboard_text(second, NULL), "") == 0);

    const char invalid_utf8[] = {(char)0xc3, '\0'};
    TEST_REQUIRE(!retro_clipboard_set_text(first, invalid_utf8, 1));
    TEST_REQUIRE(strcmp(retro_clipboard_text(first, NULL), utf8) == 0);

    const char embedded_nul[] = {'a', '\0', 'b'};
    TEST_REQUIRE(!retro_clipboard_set_text(first, embedded_nul,
                                            sizeof(embedded_nul)));
    TEST_REQUIRE(strcmp(retro_clipboard_text(first, NULL), utf8) == 0);

    TEST_REQUIRE(retro_clipboard_set_text(first, "", 0));
    TEST_REQUIRE(!retro_clipboard_has_text(first));
    TEST_REQUIRE(strcmp(retro_clipboard_text(first, &length), "") == 0);
    TEST_REQUIRE(length == 0);

    TEST_REQUIRE(!retro_clipboard_set_text(NULL, "x", 1));
    TEST_REQUIRE(!retro_clipboard_has_text(NULL));
    TEST_REQUIRE(strcmp(retro_clipboard_text(NULL, &length), "") == 0);
    TEST_REQUIRE(length == 0);
    retro_clipboard_clear(NULL);
    retro_clipboard_destroy(NULL);

    retro_clipboard_destroy(first);
    retro_clipboard_destroy(second);
    return 0;
}
'''


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one marker, found {count}")
    return text.replace(old, new, 1)


def replace_function(text: str, start: str, end: str,
                     replacement: str, label: str) -> str:
    start_index = text.find(start)
    if start_index < 0:
        raise RuntimeError(f"{label}: start marker missing")
    end_index = text.find(end, start_index)
    if end_index < 0:
        raise RuntimeError(f"{label}: end marker missing")
    return text[:start_index] + replacement + text[end_index:]


def update_app_runtime_h(text: str) -> str:
    text = replace_once(
        text,
        '#include "core/event.h"\n',
        '#include "core/clipboard.h"\n#include "core/event.h"\n',
        "clipboard runtime include",
    )
    return replace_once(
        text,
        "    Desktop *desktop;\n    WindowId window_id;\n",
        "    Desktop *desktop;\n    RetroClipboard *clipboard; /* borrowed from Desktop */\n    WindowId window_id;\n",
        "clipboard app context",
    )


def update_desktop(text: str) -> str:
    text = replace_once(
        text,
        "    AppRegistry *app_registry;\n    WindowManager *wm;\n",
        "    AppRegistry *app_registry;\n    RetroClipboard *clipboard;\n    WindowManager *wm;\n",
        "desktop clipboard member",
    )
    text = replace_once(
        text,
        "    desktop->app_registry = app_registry_create();\n    if (!desktop->app_registry) goto fail;\n",
        "    desktop->app_registry = app_registry_create();\n    if (!desktop->app_registry) goto fail;\n\n    desktop->clipboard = retro_clipboard_create();\n    if (!desktop->clipboard) goto fail;\n",
        "desktop clipboard creation",
    )
    text = replace_once(
        text,
        "    instance->ctx.desktop = desktop;\n    instance->ctx.capabilities = &desktop->capabilities;\n",
        "    instance->ctx.desktop = desktop;\n    instance->ctx.clipboard = desktop->clipboard;\n    instance->ctx.capabilities = &desktop->capabilities;\n",
        "app clipboard injection",
    )
    return replace_once(
        text,
        "    app_registry_destroy(desktop->app_registry);\n    desktop->app_registry = NULL;\n",
        "    app_registry_destroy(desktop->app_registry);\n    desktop->app_registry = NULL;\n    retro_clipboard_destroy(desktop->clipboard);\n    desktop->clipboard = NULL;\n",
        "desktop clipboard destruction",
    )


def update_notepad(text: str) -> str:
    text = replace_once(
        text,
        "typedef struct NotepadState {\n    TextBuffer *buffer;\n",
        "typedef struct NotepadState {\n    TextBuffer *buffer;\n    RetroClipboard *clipboard; /* borrowed */\n",
        "notepad clipboard member",
    )
    text = replace_once(
        text,
        "    bool copied = retro_clipboard_set_text(selected, length);\n",
        "    bool copied = retro_clipboard_set_text(state->clipboard, selected, length);\n",
        "notepad clipboard copy",
    )
    text = replace_once(
        text,
        "    if (!state || !retro_clipboard_has_text()) return;\n    size_t length = 0;\n    const char *clipboard = retro_clipboard_text(&length);\n",
        "    if (!state || !retro_clipboard_has_text(state->clipboard)) return;\n    size_t length = 0;\n    const char *clipboard = retro_clipboard_text(state->clipboard, &length);\n",
        "notepad clipboard paste",
    )
    return replace_once(
        text,
        "    NotepadState *state = calloc(1, sizeof(*state));\n    if (!state) return false;\n",
        "    NotepadState *state = calloc(1, sizeof(*state));\n    if (!state) return false;\n    state->clipboard = ctx->clipboard;\n",
        "notepad clipboard binding",
    )


def update_runtime_test(text: str) -> str:
    helper_old = '''static RetroAppInstance create_untitled_notepad(void) {
    const RetroAppDescriptor *desc = notepad_app_descriptor();
    TEST_REQUIRE(desc != NULL);
    RetroAppContext ctx = {
        .desktop = NULL,
        .theme = retro_theme_get(RETRO_THEME_XP),
        .resource_path = NULL,
    };
    RetroAppInstance instance = {.descriptor = desc, .ctx = ctx};
    TEST_REQUIRE(desc->create(&instance, &ctx));
    return instance;
}
'''
    helper_new = '''static RetroAppInstance create_untitled_notepad_with_clipboard(
    RetroClipboard *clipboard) {
    const RetroAppDescriptor *desc = notepad_app_descriptor();
    TEST_REQUIRE(desc != NULL);
    RetroAppContext ctx = {
        .desktop = NULL,
        .clipboard = clipboard,
        .theme = retro_theme_get(RETRO_THEME_XP),
        .resource_path = NULL,
    };
    RetroAppInstance instance = {.descriptor = desc, .ctx = ctx};
    TEST_REQUIRE(desc->create(&instance, &ctx));
    return instance;
}

static RetroAppInstance create_untitled_notepad(void) {
    return create_untitled_notepad_with_clipboard(NULL);
}
'''
    text = replace_once(text, helper_old, helper_new,
                        "notepad clipboard test helper")

    selection = r'''static void test_notepad_selection_clipboard_between_instances(void) {
    RetroClipboard *clipboard = retro_clipboard_create();
    TEST_REQUIRE(clipboard != NULL);
    RetroAppInstance first = create_untitled_notepad_with_clipboard(clipboard);
    RetroAppInstance second = create_untitled_notepad_with_clipboard(clipboard);

    type_notepad_char(&first, 'n');
    type_notepad_char(&first, 'i');
    type_notepad_codepoint(&first, 0x00F1u);
    type_notepad_char(&first, 'o');
    TEST_REQUIRE(strcmp(notepad_line_for_test(&first, 0), "niño") == 0);

    size_t history_before_copy = notepad_undo_count_for_test(&first);
    RetroEvent select_all = key_event(RETRO_KEY_CTRL_A, '\0');
    RetroEvent copy = key_event(RETRO_KEY_CTRL_C, '\0');
    app_handle_event(&first, &select_all);
    app_handle_event(&first, &copy);
    TEST_REQUIRE(notepad_undo_count_for_test(&first) == history_before_copy);
    TEST_REQUIRE(retro_clipboard_has_text(clipboard));
    TEST_REQUIRE(strcmp(retro_clipboard_text(clipboard, NULL), "niño") == 0);

    RetroEvent paste = key_event(RETRO_KEY_CTRL_V, '\0');
    app_handle_event(&second, &paste);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&second, 0), "niño") == 0);
    TEST_REQUIRE(notepad_undo_count_for_test(&second) == 1);
    TEST_REQUIRE(notepad_dirty_for_test(&second));

    RetroEvent undo = key_event(RETRO_KEY_CTRL_Z, '\0');
    app_handle_event(&second, &undo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&second, 0), "") == 0);
    TEST_REQUIRE(!notepad_dirty_for_test(&second));

    RetroEvent cut = key_event(RETRO_KEY_CTRL_X, '\0');
    app_handle_event(&first, &cut);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&first, 0), "") == 0);
    TEST_REQUIRE(strcmp(retro_clipboard_text(clipboard, NULL), "niño") == 0);
    app_handle_event(&first, &undo);
    TEST_REQUIRE(strcmp(notepad_line_for_test(&first, 0), "niño") == 0);

    first.descriptor->destroy(&first);
    second.descriptor->destroy(&second);
    retro_clipboard_destroy(clipboard);
}

'''
    text = replace_function(
        text,
        "static void test_notepad_selection_clipboard_between_instances(void) {",
        "static void test_notepad_utf8_find(void) {",
        selection,
        "selection clipboard test",
    )

    text = replace_once(
        text,
        "static void test_notepad_utf8_find(void) {\n    retro_clipboard_clear();\n    RetroAppInstance instance = create_untitled_notepad();\n",
        "static void test_notepad_utf8_find(void) {\n    RetroClipboard *clipboard = retro_clipboard_create();\n    TEST_REQUIRE(clipboard != NULL);\n    RetroAppInstance instance = create_untitled_notepad_with_clipboard(clipboard);\n",
        "find clipboard setup",
    )
    text = replace_once(
        text,
        "static void test_notepad_word_wrap_navigation(void) {\n    RetroAppInstance instance = create_untitled_notepad();\n",
        "static void test_notepad_word_wrap_navigation(void) {\n    RetroClipboard *clipboard = retro_clipboard_create();\n    TEST_REQUIRE(clipboard != NULL);\n    RetroAppInstance instance = create_untitled_notepad_with_clipboard(clipboard);\n",
        "wrap clipboard setup",
    )

    text = text.replace("retro_clipboard_text(NULL)",
                        "retro_clipboard_text(clipboard, NULL)")
    text = text.replace("retro_clipboard_has_text()",
                        "retro_clipboard_has_text(clipboard)")
    text = text.replace("retro_clipboard_clear();",
                        "retro_clipboard_destroy(clipboard);")

    desktop_test = r'''static void test_desktop_clipboards_are_isolated(void) {
    PlatformBackend *first_platform = NULL;
    PlatformBackend *second_platform = NULL;
    Desktop *first_desktop = create_test_desktop(&first_platform);
    Desktop *second_desktop = create_test_desktop(&second_platform);
    TEST_REQUIRE(first_desktop != NULL);
    TEST_REQUIRE(second_desktop != NULL);

    RetroAppInstance *first = app_launch(first_desktop, "notepad");
    RetroAppInstance *second = app_launch(second_desktop, "notepad");
    TEST_REQUIRE(first != NULL);
    TEST_REQUIRE(second != NULL);

    type_notepad_char(first, 'x');
    RetroEvent select_all = key_event(RETRO_KEY_CTRL_A, '\0');
    RetroEvent copy = key_event(RETRO_KEY_CTRL_C, '\0');
    RetroEvent paste = key_event(RETRO_KEY_CTRL_V, '\0');
    RetroEvent escape = key_event(RETRO_KEY_ESC, '\0');
    app_handle_event(first, &select_all);
    app_handle_event(first, &copy);

    app_handle_event(second, &paste);
    TEST_REQUIRE(strcmp(notepad_line_for_test(second, 0), "") == 0);

    type_notepad_char(second, 'y');
    app_handle_event(second, &select_all);
    app_handle_event(second, &copy);

    app_handle_event(first, &escape);
    app_handle_event(first, &paste);
    TEST_REQUIRE(strcmp(notepad_line_for_test(first, 0), "xx") == 0);

    destroy_test_desktop(first_desktop, first_platform);
    destroy_test_desktop(second_desktop, second_platform);
}

'''
    text = replace_once(
        text,
        "static int g_key_sink_f2_count;\n",
        desktop_test + "static int g_key_sink_f2_count;\n",
        "desktop clipboard isolation test",
    )
    return replace_once(
        text,
        "    test_notepad_shift_selection_replacement();\n",
        "    test_notepad_shift_selection_replacement();\n    test_desktop_clipboards_are_isolated();\n",
        "desktop clipboard test registration",
    )


def update_readme(text: str) -> str:
    return text.replace(
        "│   ├── clipboard.c        # Shared validated UTF-8 clipboard",
        "│   ├── clipboard.c        # Desktop-owned validated UTF-8 clipboard",
        1,
    )


def main() -> int:
    updates = {
        "src/core/clipboard.h": lambda _: CLIPBOARD_H,
        "src/core/clipboard.c": lambda _: CLIPBOARD_C,
        "src/app/app_runtime.h": update_app_runtime_h,
        "src/core/desktop.c": update_desktop,
        "src/apps/notepad_app.c": update_notepad,
        "tests/clipboard_test.c": lambda _: CLIPBOARD_TEST,
        "tests/desktop_runtime_test.c": update_runtime_test,
        "README.md": update_readme,
    }

    rendered: dict[Path, str] = {}
    try:
        for relative, transform in updates.items():
            path = ROOT / relative
            original = path.read_text(encoding="utf-8")
            changed = transform(original)
            if changed == original:
                raise RuntimeError(f"{relative}: transformation made no change")
            rendered[path] = changed
    except (OSError, RuntimeError) as exc:
        print(f"ERROR: {exc}")
        return 1

    for path, content in rendered.items():
        path.write_text(content, encoding="utf-8")
        print(path.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
