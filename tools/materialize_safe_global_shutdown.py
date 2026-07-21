#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one marker, found {count}")
    return text.replace(old, new, 1)


def update_app_runtime_h(text: str) -> str:
    text = replace_once(
        text,
        "typedef struct RetroAppInstance RetroAppInstance;\ntypedef struct AppRegistry AppRegistry;\n",
        "typedef struct RetroAppInstance RetroAppInstance;\ntypedef struct AppRegistry AppRegistry;\n\ntypedef enum RetroCloseResult {\n    RETRO_CLOSE_ALLOWED = 0,\n    RETRO_CLOSE_DEFERRED,\n    RETRO_CLOSE_CANCELLED,\n} RetroCloseResult;\n",
        "close result enum",
    )
    text = replace_once(
        text,
        "    bool close_requested;\n    char *resource_path_owned;\n",
        "    bool close_requested;\n    bool close_pending;\n    RetroCloseResult close_result;\n    char *resource_path_owned;\n",
        "app close state",
    )
    return replace_once(
        text,
        "void app_request_close(RetroAppInstance *app);\nbool app_is_close_requested(const RetroAppInstance *app);\n",
        "RetroCloseResult app_request_close(RetroAppInstance *app);\nvoid app_resolve_close(RetroAppInstance *app, RetroCloseResult result);\nvoid app_reset_close_request(RetroAppInstance *app);\nbool app_is_close_requested(const RetroAppInstance *app);\nbool app_is_close_pending(const RetroAppInstance *app);\nRetroCloseResult app_close_result(const RetroAppInstance *app);\n",
        "close API declarations",
    )


def update_app_runtime_c(text: str) -> str:
    old = '''void app_request_close(RetroAppInstance *app) {
    if (!app) return;
    app->close_requested = true;
}

bool app_is_close_requested(const RetroAppInstance *app) {
    return app && app->close_requested;
}
'''
    new = '''RetroCloseResult app_request_close(RetroAppInstance *app) {
    if (!app) return RETRO_CLOSE_CANCELLED;

    app->close_requested = false;
    app->close_pending = false;
    app->close_result = RETRO_CLOSE_ALLOWED;

    if (app->descriptor && app->descriptor->can_close &&
        !app->descriptor->can_close(app)) {
        app->close_pending = true;
        app->close_result = RETRO_CLOSE_DEFERRED;
        return RETRO_CLOSE_DEFERRED;
    }

    app->close_requested = true;
    return RETRO_CLOSE_ALLOWED;
}

void app_resolve_close(RetroAppInstance *app, RetroCloseResult result) {
    if (!app || !app->close_pending || result == RETRO_CLOSE_DEFERRED) return;
    app->close_pending = false;
    app->close_result = result;
    app->close_requested = result == RETRO_CLOSE_ALLOWED;
}

void app_reset_close_request(RetroAppInstance *app) {
    if (!app) return;
    app->close_requested = false;
    app->close_pending = false;
    app->close_result = RETRO_CLOSE_ALLOWED;
}

bool app_is_close_requested(const RetroAppInstance *app) {
    return app && app->close_requested;
}

bool app_is_close_pending(const RetroAppInstance *app) {
    return app && app->close_pending;
}

RetroCloseResult app_close_result(const RetroAppInstance *app) {
    return app ? app->close_result : RETRO_CLOSE_CANCELLED;
}
'''
    return replace_once(text, old, new, "close API implementation")


def update_notepad(text: str) -> str:
    text = replace_once(
        text,
        "    state->force_close = true;\n    app_request_close(instance);\n",
        "    state->force_close = true;\n    app_resolve_close(instance, RETRO_CLOSE_ALLOWED);\n",
        "notepad allowed resolution",
    )
    return replace_once(
        text,
        "        state->close_after_save = false;\n        state->error[0] = '\\0';\n        return;\n",
        "        state->close_after_save = false;\n        state->error[0] = '\\0';\n        app_resolve_close(instance, RETRO_CLOSE_CANCELLED);\n        return;\n",
        "notepad cancelled resolution",
    )


def update_desktop_h(text: str) -> str:
    marker = '''bool desktop_register_app_for_test(Desktop *desktop, const RetroAppDescriptor *desc);
bool desktop_dispatch_event_for_test(Desktop *desktop, const RetroEvent *event);
RetroAppInstance *desktop_app_instance_for_test(Desktop *desktop,
                                                const char *app_id);
'''
    return replace_once(
        text,
        marker,
        marker + "bool desktop_shutdown_pending_for_test(const Desktop *desktop);\n",
        "desktop shutdown test hook",
    )


def update_desktop(text: str) -> str:
    text = replace_once(
        text,
        "    bool window_resize_mode;\n    time_t last_status_tick;\n",
        "    bool window_resize_mode;\n    bool shutdown_requested;\n    size_t shutdown_index;\n    WindowId shutdown_waiting_window;\n    time_t last_status_tick;\n",
        "desktop shutdown state",
    )

    text = replace_once(
        text,
        '''static void app_window_event(RetroWindow *window, const RetroEvent *event, void *user_data) {
    RetroAppInstance *app = (RetroAppInstance *)user_data;
    app_handle_event(app, event);
    if (app_is_close_requested(app)) {
        if (!app->descriptor->can_close || app->descriptor->can_close(app)) {
            retro_window_request_close(window);
        } else {
            app->close_requested = false;
        }
    }
}
''',
        '''static void app_window_event(RetroWindow *window, const RetroEvent *event, void *user_data) {
    RetroAppInstance *app = (RetroAppInstance *)user_data;
    app_handle_event(app, event);
    if (app_is_close_requested(app)) {
        Desktop *desktop = app->ctx.desktop;
        if (!desktop || !desktop->shutdown_requested) {
            retro_window_request_close(window);
        }
    }
}
''',
        "app window close routing",
    )

    text = replace_once(
        text,
        '''static bool desktop_request_app_close(Desktop *desktop, WindowId window_id) {
    if (!desktop || window_id == WINDOW_ID_INVALID) return false;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        RunningApp *running = &desktop->apps[i];
        if (running->window_id != window_id || !running->app) continue;
        RetroAppInstance *app = running->app;
        if (app->descriptor->can_close && !app->descriptor->can_close(app)) return false;
        app_request_close(app);
        wm_close_window(desktop->wm, window_id);
        return true;
    }
    return false;
}
''',
        '''static bool desktop_request_app_close(Desktop *desktop, WindowId window_id) {
    if (!desktop || window_id == WINDOW_ID_INVALID) return false;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        RunningApp *running = &desktop->apps[i];
        if (running->window_id != window_id || !running->app) continue;
        RetroCloseResult result = app_request_close(running->app);
        if (result != RETRO_CLOSE_ALLOWED) return false;
        wm_close_window(desktop->wm, window_id);
        return true;
    }
    return false;
}
''',
        "single app close request",
    )

    text = replace_once(
        text,
        "    desktop->launcher.window_id = WINDOW_ID_INVALID;\n",
        "    desktop->launcher.window_id = WINDOW_ID_INVALID;\n    desktop->shutdown_waiting_window = WINDOW_ID_INVALID;\n",
        "shutdown initialization",
    )

    text = replace_once(
        text,
        '''        if (should_close && window_exists && slot->app->descriptor->can_close &&
            !slot->app->descriptor->can_close(slot->app)) {
            slot->app->close_requested = false;
            should_close = false;
        }
        if (should_close && window_exists) {
            wm_close_window(desktop->wm, slot->window_id);
            window_exists = false;
        }

        if (!window_exists || should_close) {
''',
        '''        if (should_close && window_exists && !desktop->shutdown_requested) {
            wm_close_window(desktop->wm, slot->window_id);
            window_exists = false;
        }

        if (!window_exists || (should_close && !desktop->shutdown_requested)) {
''',
        "shutdown-aware cleanup",
    )

    helpers = r'''static RunningApp *desktop_find_running_app(Desktop *desktop,
                                            WindowId window_id) {
    if (!desktop || window_id == WINDOW_ID_INVALID) return NULL;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        if (desktop->apps[i].window_id == window_id) return &desktop->apps[i];
    }
    return NULL;
}

static void desktop_reset_shutdown_requests(Desktop *desktop) {
    if (!desktop) return;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        app_reset_close_request(desktop->apps[i].app);
    }
}

static void desktop_cancel_shutdown(Desktop *desktop) {
    if (!desktop) return;
    desktop_reset_shutdown_requests(desktop);
    desktop->shutdown_requested = false;
    desktop->shutdown_index = 0;
    desktop->shutdown_waiting_window = WINDOW_ID_INVALID;
    desktop_request_redraw(desktop);
}

static void desktop_commit_shutdown(Desktop *desktop) {
    if (!desktop) return;
    desktop->shutdown_requested = false;
    desktop->shutdown_waiting_window = WINDOW_ID_INVALID;
    for (size_t i = 0; i < desktop->app_count; ++i) {
        if (wm_window_exists(desktop->wm, desktop->apps[i].window_id)) {
            wm_close_window(desktop->wm, desktop->apps[i].window_id);
        }
    }
    desktop_cleanup_apps(desktop);
    desktop->shutdown_index = 0;
    desktop->running = false;
}

static void desktop_continue_shutdown(Desktop *desktop) {
    if (!desktop || !desktop->shutdown_requested) return;

    if (desktop->shutdown_waiting_window != WINDOW_ID_INVALID) {
        RunningApp *waiting = desktop_find_running_app(
            desktop, desktop->shutdown_waiting_window);
        if (!waiting || !waiting->app) {
            desktop_cancel_shutdown(desktop);
            return;
        }
        if (app_is_close_pending(waiting->app)) return;
        if (app_close_result(waiting->app) == RETRO_CLOSE_CANCELLED) {
            desktop_cancel_shutdown(desktop);
            return;
        }
        if (!app_is_close_requested(waiting->app)) {
            desktop_cancel_shutdown(desktop);
            return;
        }
        desktop->shutdown_waiting_window = WINDOW_ID_INVALID;
        desktop->shutdown_index++;
    }

    while (desktop->shutdown_index < desktop->app_count) {
        RunningApp *running = &desktop->apps[desktop->shutdown_index];
        RetroCloseResult result = app_request_close(running->app);
        if (result == RETRO_CLOSE_ALLOWED) {
            desktop->shutdown_index++;
            continue;
        }
        if (result == RETRO_CLOSE_CANCELLED) {
            desktop_cancel_shutdown(desktop);
            return;
        }
        desktop->shutdown_waiting_window = running->window_id;
        wm_focus_window(desktop->wm, running->window_id);
        wm_bring_to_front(desktop->wm, running->window_id);
        desktop_request_redraw(desktop);
        return;
    }

    desktop_commit_shutdown(desktop);
}

static void desktop_begin_shutdown(Desktop *desktop) {
    if (!desktop || desktop->shutdown_requested) return;
    if (desktop->launcher.open) desktop_launcher_close(desktop);
    desktop->window_mode = false;
    desktop->window_resize_mode = false;
    desktop->shutdown_requested = true;
    desktop->shutdown_index = 0;
    desktop->shutdown_waiting_window = WINDOW_ID_INVALID;
    desktop_continue_shutdown(desktop);
}

'''
    text = replace_once(
        text,
        "static void desktop_update_taskbar_snapshot(Desktop *desktop,\n",
        helpers + "static void desktop_update_taskbar_snapshot(Desktop *desktop,\n",
        "shutdown coordinator",
    )

    text = replace_once(
        text,
        "    if (!desktop || !mouse || !mouse->button1_clicked) return false;\n",
        "    if (!desktop || !mouse || !mouse->button1_clicked) return false;\n    if (desktop->shutdown_requested) return true;\n",
        "disable taskbar during shutdown",
    )

    text = replace_once(
        text,
        '''static bool desktop_handle_key_command(Desktop *desktop, const RetroKeyEvent *key) {
    if (!desktop || !key) return false;

    /* Global accelerators must not consume printable text.  The previous
''',
        '''static bool desktop_handle_key_command(Desktop *desktop, const RetroKeyEvent *key) {
    if (!desktop || !key) return false;

    if (desktop->shutdown_requested) {
        return key->key_code == RETRO_KEY_CTRL_Q;
    }

    /* Global accelerators must not consume printable text.  The previous
''',
        "shutdown key gate",
    )

    text = replace_once(
        text,
        "            desktop->running = false;\n            return true;\n",
        "            desktop_begin_shutdown(desktop);\n            return true;\n",
        "launcher shutdown accelerator",
    )
    text = replace_once(
        text,
        "    if (key->key_code == RETRO_KEY_CTRL_Q) {\n        desktop->running = false;\n        return true;\n    }\n",
        "    if (key->key_code == RETRO_KEY_CTRL_Q) {\n        desktop_begin_shutdown(desktop);\n        return true;\n    }\n",
        "global shutdown accelerator",
    )

    text = replace_once(
        text,
        "    desktop_cleanup_apps(desktop);\n    desktop_request_redraw(desktop);\n    return true;\n",
        "    desktop_cleanup_apps(desktop);\n    desktop_continue_shutdown(desktop);\n    desktop_request_redraw(desktop);\n    return true;\n",
        "continue shutdown after event",
    )

    hook = '''RetroAppInstance *desktop_app_instance_for_test(Desktop *desktop,
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
'''
    return replace_once(
        text,
        hook,
        hook + '''
bool desktop_shutdown_pending_for_test(const Desktop *desktop) {
    return desktop && desktop->shutdown_requested;
}
''',
        "shutdown test probe",
    )


def update_app_registry_test(text: str) -> str:
    tests = r'''static bool deferred_can_close(RetroAppInstance *instance) {
    (void)instance;
    return false;
}

static void test_close_resolution_contract(void) {
    RetroAppDescriptor descriptor = {
        .app_id = "close-test",
        .display_name = "Close Test",
        .can_close = deferred_can_close,
    };
    RetroAppInstance instance = {.descriptor = &descriptor};

    TEST_REQUIRE(app_request_close(&instance) == RETRO_CLOSE_DEFERRED);
    TEST_REQUIRE(app_is_close_pending(&instance));
    TEST_REQUIRE(!app_is_close_requested(&instance));

    app_resolve_close(&instance, RETRO_CLOSE_CANCELLED);
    TEST_REQUIRE(!app_is_close_pending(&instance));
    TEST_REQUIRE(app_close_result(&instance) == RETRO_CLOSE_CANCELLED);
    TEST_REQUIRE(!app_is_close_requested(&instance));

    TEST_REQUIRE(app_request_close(&instance) == RETRO_CLOSE_DEFERRED);
    app_resolve_close(&instance, RETRO_CLOSE_ALLOWED);
    TEST_REQUIRE(app_is_close_requested(&instance));
    app_reset_close_request(&instance);
    TEST_REQUIRE(!app_is_close_requested(&instance));
    TEST_REQUIRE(!app_is_close_pending(&instance));
}

'''
    text = replace_once(text, "int main(void) {\n", tests + "int main(void) {\n",
                        "close contract test")
    return replace_once(
        text,
        "    app_registry_destroy(registry);\n    return 0;\n",
        "    app_registry_destroy(registry);\n    test_close_resolution_contract();\n    return 0;\n",
        "close contract registration",
    )


def update_runtime_test(text: str) -> str:
    tests = r'''static void test_ctrl_q_cancel_preserves_all_apps(void) {
    PlatformBackend *platform = NULL;
    Desktop *desktop = create_test_desktop(&platform);
    TEST_REQUIRE(desktop != NULL);

    RetroAppInstance *notepad = app_launch(desktop, "notepad");
    TEST_REQUIRE(notepad != NULL);
    type_notepad_char(notepad, 'x');
    size_t app_count = desktop_app_count(desktop);

    RetroEvent quit = key_event(RETRO_KEY_CTRL_Q, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &quit));
    TEST_REQUIRE(desktop_shutdown_pending_for_test(desktop));
    TEST_REQUIRE(notepad_close_prompt_for_test(notepad));
    TEST_REQUIRE(desktop_app_count(desktop) == app_count);

    RetroEvent cancel = key_event(RETRO_KEY_ESC, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &cancel));
    TEST_REQUIRE(!desktop_shutdown_pending_for_test(desktop));
    TEST_REQUIRE(desktop_app_count(desktop) == app_count);
    TEST_REQUIRE(notepad_dirty_for_test(notepad));

    destroy_test_desktop(desktop, platform);
}

static void test_ctrl_q_discard_commits_shutdown(void) {
    PlatformBackend *platform = NULL;
    Desktop *desktop = create_test_desktop(&platform);
    TEST_REQUIRE(desktop != NULL);

    RetroAppInstance *notepad = app_launch(desktop, "notepad");
    TEST_REQUIRE(notepad != NULL);
    type_notepad_char(notepad, 'x');

    RetroEvent quit = key_event(RETRO_KEY_CTRL_Q, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &quit));
    TEST_REQUIRE(desktop_shutdown_pending_for_test(desktop));
    TEST_REQUIRE(desktop_app_count(desktop) >= 2);

    RetroEvent discard = key_event('d', 'd');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &discard));
    TEST_REQUIRE(!desktop_shutdown_pending_for_test(desktop));
    TEST_REQUIRE(desktop_app_count(desktop) == 0);

    destroy_test_desktop(desktop, platform);
}

'''
    text = replace_once(
        text,
        "static void test_desktop_f2_reaches_focused_app(void) {\n",
        tests + "static void test_desktop_f2_reaches_focused_app(void) {\n",
        "shutdown integration tests",
    )
    registration = '''    test_desktop_f2_reaches_focused_app();
    test_launcher_close_respects_dirty_notepad();
'''
    return replace_once(
        text,
        registration,
        registration + '''    test_ctrl_q_cancel_preserves_all_apps();
    test_ctrl_q_discard_commits_shutdown();
''',
        "shutdown test registration",
    )


def update_docs(text: str) -> str:
    return replace_once(
        text,
        "| `Ctrl+Q` | Quit RetroDesk |",
        "| `Ctrl+Q` | Request coordinated shutdown; dirty apps may Save, Discard, or Cancel |",
        "shutdown shortcut documentation",
    )


def main() -> int:
    updates = {
        "src/app/app_runtime.h": update_app_runtime_h,
        "src/app/app_runtime.c": update_app_runtime_c,
        "src/apps/notepad_app.c": update_notepad,
        "src/core/desktop.h": update_desktop_h,
        "src/core/desktop.c": update_desktop,
        "tests/app_registry_test.c": update_app_registry_test,
        "tests/desktop_runtime_test.c": update_runtime_test,
        "docs/KEYBOARD_SHORTCUTS.md": update_docs,
    }
    rendered = {}
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
