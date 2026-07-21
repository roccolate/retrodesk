#!/usr/bin/env python3
"""Materialize the first RetroDesk core-hardening slice.

The script is transactional: all source markers are validated before any file is
written. It is intended to run once in CI and then remove itself.
"""

from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one marker, found {count}")
    return text.replace(old, new, 1)


def update_desktop(text: str) -> str:
    text = replace_once(
        text,
        """        if (active != WINDOW_ID_INVALID && active != desktop->shell_window_id) {
            wm_close_window(desktop->wm, active);
        }
""",
        """        if (active != WINDOW_ID_INVALID && active != desktop->shell_window_id) {
            (void)desktop_request_app_close(desktop, active);
        }
""",
        "launcher close must use app lifecycle",
    )

    text = replace_once(
        text,
        '"F1/F2 launcher | F6 focus | F9 move/resize | Ctrl+W close | Ctrl+Q quit",',
        '"F1/F10 launcher | F6 focus | F9 move/resize | Ctrl+W close | Ctrl+Q quit",',
        "shell shortcut hint",
    )

    text = replace_once(
        text,
        """        if (key->key_code == RETRO_KEY_F2) {
            desktop_launcher_close(desktop);
            return true;
        }
""",
        """        if (key->key_code == RETRO_KEY_F10) {
            desktop_launcher_close(desktop);
            return true;
        }
""",
        "launcher close accelerator",
    )

    text = replace_once(
        text,
        """    if (key->key_code == RETRO_KEY_F2) {
        desktop_launcher_open(desktop);
        return true;
    }

    if (key->key_code == RETRO_KEY_F1) {
        desktop_launcher_open(desktop);
        return true;
    }
""",
        """    if (key->key_code == RETRO_KEY_F1 ||
        key->key_code == RETRO_KEY_F10) {
        desktop_launcher_open(desktop);
        return true;
    }
""",
        "global launcher accelerators",
    )

    old_run = """int desktop_run(Desktop *desktop) {
    if (!desktop) return EXIT_FAILURE;

    if (desktop->config.bench_mode) {
        desktop_update_status(desktop);
        desktop_render_frame(desktop);
        return EXIT_SUCCESS;
    }

    desktop->running = true;
    while (desktop->running) {
        RetroEvent event = {0};
        RetroPollResult poll_result = platform_poll_event(
            desktop->platform, &event, desktop->config.input_timeout_ms);

        if (poll_result == RETRO_POLL_EVENT) {
            bool consumed = false;
            if (event.type == RETRO_EVENT_KEY) {
                consumed = desktop_handle_key_command(desktop, &event.data.key);
            } else if (event.type == RETRO_EVENT_MOUSE) {
                consumed = desktop_handle_taskbar_mouse(
                    desktop, &event.data.mouse);
            }
            if (!consumed) {
                wm_handle_event(desktop->wm, &event);
            }
            desktop_cleanup_apps(desktop);
            desktop_request_redraw(desktop);
        } else if (poll_result == RETRO_POLL_CLOSED || poll_result == RETRO_POLL_ERROR) {
            desktop->running = false;
            return poll_result == RETRO_POLL_CLOSED ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        desktop_update_status(desktop);
        if (desktop->needs_redraw) {
            desktop_render_frame(desktop);
        }
    }

    return EXIT_SUCCESS;
}
"""
    new_run = """static bool desktop_dispatch_event(Desktop *desktop,
                                   const RetroEvent *event) {
    if (!desktop || !event) return false;

    bool consumed = false;
    if (event->type == RETRO_EVENT_KEY) {
        consumed = desktop_handle_key_command(desktop, &event->data.key);
    } else if (event->type == RETRO_EVENT_MOUSE) {
        consumed = desktop_handle_taskbar_mouse(desktop, &event->data.mouse);
    }
    if (!consumed) {
        (void)wm_handle_event(desktop->wm, event);
    }
    desktop_cleanup_apps(desktop);
    desktop_request_redraw(desktop);
    return true;
}

#ifdef RETRODESK_ENABLE_TEST_HOOKS
bool desktop_dispatch_event_for_test(Desktop *desktop,
                                     const RetroEvent *event) {
    return desktop_dispatch_event(desktop, event);
}

RetroAppInstance *desktop_app_instance_for_test(Desktop *desktop,
                                                const char *app_id) {
    if (!desktop || !app_id) return NULL;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        RetroAppInstance *instance = desktop->apps[i].app;
        if (instance && instance->descriptor &&
            instance->descriptor->app_id &&
            strcmp(instance->descriptor->app_id, app_id) == 0) {
            return instance;
        }
    }
    return NULL;
}
#endif

int desktop_run(Desktop *desktop) {
    if (!desktop) return EXIT_FAILURE;

    if (desktop->config.bench_mode) {
        desktop_update_status(desktop);
        desktop_render_frame(desktop);
        return EXIT_SUCCESS;
    }

    desktop->running = true;
    while (desktop->running) {
        RetroEvent event = {0};
        RetroPollResult poll_result = platform_poll_event(
            desktop->platform, &event, desktop->config.input_timeout_ms);

        if (poll_result == RETRO_POLL_EVENT) {
            (void)desktop_dispatch_event(desktop, &event);
        } else if (poll_result == RETRO_POLL_CLOSED ||
                   poll_result == RETRO_POLL_ERROR) {
            desktop->running = false;
            return poll_result == RETRO_POLL_CLOSED
                       ? EXIT_SUCCESS
                       : EXIT_FAILURE;
        }

        desktop_update_status(desktop);
        if (desktop->needs_redraw) {
            desktop_render_frame(desktop);
        }
    }

    return EXIT_SUCCESS;
}
"""
    return replace_once(text, old_run, new_run, "desktop event dispatcher")


def update_desktop_header(text: str) -> str:
    return replace_once(
        text,
        """#ifdef RETRODESK_ENABLE_TEST_HOOKS
bool desktop_register_app_for_test(Desktop *desktop, const RetroAppDescriptor *desc);
#endif
""",
        """#ifdef RETRODESK_ENABLE_TEST_HOOKS
bool desktop_register_app_for_test(Desktop *desktop,
                                   const RetroAppDescriptor *desc);
bool desktop_dispatch_event_for_test(Desktop *desktop,
                                     const RetroEvent *event);
RetroAppInstance *desktop_app_instance_for_test(Desktop *desktop,
                                                const char *app_id);
#endif
""",
        "desktop test hooks",
    )


def update_filemanager(text: str) -> str:
    marker = """bool filemanager_has_item_for_test(const RetroAppInstance *instance, const char *name) {
    const FileManagerState *state = instance ? instance->state : NULL;
    return state && fm_find_name(state, name) < state->count;
}

"""
    replacement = marker + """bool filemanager_prompt_active_for_test(
    const RetroAppInstance *instance) {
    const FileManagerState *state = instance ? instance->state : NULL;
    return state && state->prompt_mode != FM_PROMPT_NONE;
}

"""
    return replace_once(text, marker, replacement, "filemanager prompt probe")


def update_apps_internal(text: str) -> str:
    return replace_once(
        text,
        """bool filemanager_has_item_for_test(const RetroAppInstance *instance,
                                   const char *name);
""",
        """bool filemanager_has_item_for_test(const RetroAppInstance *instance,
                                   const char *name);
bool filemanager_prompt_active_for_test(
    const RetroAppInstance *instance);
""",
        "filemanager prompt probe declaration",
    )


def update_runtime_test(text: str) -> str:
    insert_before = """#if !defined(_WIN32)
static void create_fixture_file(const char *directory, const char *name) {
"""
    tests = r'''static Desktop *create_test_desktop(PlatformBackend **out_platform) {
    PlatformBackend *platform = platform_stub_new(true);
    TEST_REQUIRE(platform != NULL);
    DesktopConfig cfg = {
        .platform = platform,
        .input_timeout_ms = 20,
        .bench_mode = false,
        .render_backend = RENDER_BACKEND_CURSES,
        .theme_kind = RETRO_THEME_XP,
    };
    Desktop *desktop = desktop_create(&cfg);
    TEST_REQUIRE(desktop != NULL);
    if (out_platform) *out_platform = platform;
    return desktop;
}

static void destroy_test_desktop(Desktop *desktop,
                                 PlatformBackend *platform) {
    desktop_shutdown(desktop);
    platform_destroy(platform);
}

static void test_desktop_f2_reaches_filemanager(void) {
    PlatformBackend *platform = NULL;
    Desktop *desktop = create_test_desktop(&platform);
    RetroAppInstance *files = desktop_app_instance_for_test(
        desktop, "filemanager");
    TEST_REQUIRE(files != NULL);
    WindowId files_window = files->ctx.window_id;
    TEST_REQUIRE(desktop_active_window(desktop) == files_window);

    RetroEvent rename = key_event(RETRO_KEY_F2, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &rename));
    TEST_REQUIRE(filemanager_prompt_active_for_test(files));
    TEST_REQUIRE(desktop_active_window(desktop) == files_window);

    RetroEvent cancel = key_event(RETRO_KEY_ESC, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &cancel));
    TEST_REQUIRE(!filemanager_prompt_active_for_test(files));
    destroy_test_desktop(desktop, platform);
}

static void test_launcher_close_respects_dirty_notepad(void) {
    PlatformBackend *platform = NULL;
    Desktop *desktop = create_test_desktop(&platform);
    RetroAppInstance *notepad = app_launch(desktop, "notepad");
    TEST_REQUIRE(notepad != NULL);
    type_notepad_char(notepad, 'x');
    TEST_REQUIRE(notepad_dirty_for_test(notepad));
    size_t app_count = desktop_app_count(desktop);

    RetroEvent launcher = key_event(RETRO_KEY_F10, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &launcher));
    for (int i = 0; i < 3; ++i) {
        RetroEvent down = key_event('j', 'j');
        TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &down));
    }
    RetroEvent enter = key_event(RETRO_KEY_CR, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &enter));

    TEST_REQUIRE(desktop_app_count(desktop) == app_count);
    TEST_REQUIRE(desktop_app_instance_for_test(desktop, "notepad") == notepad);
    TEST_REQUIRE(notepad_close_prompt_for_test(notepad));
    TEST_REQUIRE(notepad_dirty_for_test(notepad));
    destroy_test_desktop(desktop, platform);
}

'''
    text = replace_once(
        text,
        insert_before,
        tests + insert_before,
        "desktop routing tests",
    )
    return replace_once(
        text,
        """    test_notepad_shift_selection_replacement();
#if !defined(_WIN32)
""",
        """    test_notepad_shift_selection_replacement();
    test_desktop_f2_reaches_filemanager();
    test_launcher_close_respects_dirty_notepad();
#if !defined(_WIN32)
""",
        "desktop routing test registration",
    )


def update_readme(text: str) -> str:
    return text.replace("| `F1` or `F2` | Open the Launcher |",
                        "| `F1` or `F10` | Open the Launcher |", 1)


def update_shortcuts(text: str) -> str:
    text = text.replace("| `F1` | Open the Launcher |\n| `F2` | Open the Launcher; close it when it is already focused |",
                        "| `F1` | Open the Launcher |\n| `F10` | Open the Launcher; close it when it is already focused |", 1)
    return text.replace("| `F2` | Close the Launcher |",
                        "| `F10` | Close the Launcher |", 1)


def update_install(text: str) -> str:
    text = text.replace("# - F1: help, F2: launcher, F6: next window",
                        "# - F1/F10: launcher, F6: next window", 1)
    text = text.replace("# - F7: move/resize mode",
                        "# - F9: move/resize mode", 1)
    return text


def main() -> int:
    updates = {
        "src/core/desktop.c": update_desktop,
        "src/core/desktop.h": update_desktop_header,
        "src/apps/filemanager_app.c": update_filemanager,
        "src/apps/apps_internal.h": update_apps_internal,
        "tests/desktop_runtime_test.c": update_runtime_test,
        "README.md": update_readme,
        "docs/KEYBOARD_SHORTCUTS.md": update_shortcuts,
        "INSTALL.md": update_install,
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
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    for path, content in rendered.items():
        path.write_text(content, encoding="utf-8")
        print(path.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
