#!/usr/bin/env python3
from pathlib import Path
import re

path = Path(__file__).with_name("materialize_safe_global_shutdown.py")
text = path.read_text(encoding="utf-8")

notepad_pattern = r"def update_notepad\(text: str\) -> str:\n.*?\n\ndef update_desktop_h"
notepad_replacement = r'''def update_notepad(text: str) -> str:
    text = replace_once(
        text,
        "    state->force_close = true;\n    app_request_close(instance);\n",
        "    state->force_close = true;\n    app_resolve_close(instance, RETRO_CLOSE_ALLOWED);\n",
        "notepad allowed resolution",
    )
    prompt_start = text.index("static void np_handle_close_prompt(")
    prompt_end = text.index("static bool np_find_next(", prompt_start)
    prompt = text[prompt_start:prompt_end]
    prompt = replace_once(
        prompt,
        "        state->close_after_save = false;\n        state->error[0] = '\\0';\n        return;\n",
        "        state->close_after_save = false;\n        state->error[0] = '\\0';\n        app_resolve_close(instance, RETRO_CLOSE_CANCELLED);\n        return;\n",
        "notepad cancelled resolution",
    )
    text = text[:prompt_start] + prompt + text[prompt_end:]

    save_start = text.index("static void np_handle_save_as(")
    save_end = text.index("static void np_event(", save_start)
    save_as = text[save_start:save_end]
    save_as = replace_once(
        save_as,
        "    if (code == RETRO_KEY_ESC) {\n        state->save_as = false;\n        state->close_after_save = false;\n        state->error[0] = '\\0';\n        return;\n    }\n",
        "    if (code == RETRO_KEY_ESC) {\n        bool was_closing = state->close_after_save;\n        state->save_as = false;\n        state->close_after_save = false;\n        state->error[0] = '\\0';\n        if (was_closing) {\n            app_resolve_close(instance, RETRO_CLOSE_CANCELLED);\n        }\n        return;\n    }\n",
        "save as cancellation resolution",
    )
    save_as = replace_once(
        save_as,
        "        } else {\n            state->close_after_save = false;\n        }\n",
        "        } else {\n            state->close_after_save = false;\n            if (should_close) {\n                app_resolve_close(instance, RETRO_CLOSE_CANCELLED);\n            }\n        }\n",
        "save as failure resolution",
    )
    return text[:save_start] + save_as + text[save_end:]


def update_desktop_h'''
text, count = re.subn(notepad_pattern, lambda match: notepad_replacement,
                      text, count=1, flags=re.S)
if count != 1:
    raise SystemExit(f"repair expected one update_notepad function, found {count}")

header_pattern = r"def update_desktop_h\(text: str\) -> str:\n.*?\n\ndef update_desktop\(text: str\) -> str:"
header_replacement = r'''def update_desktop_h(text: str) -> str:
    return replace_once(
        text,
        "#endif\nvoid desktop_shutdown(Desktop *desktop);\n",
        "bool desktop_shutdown_pending_for_test(const Desktop *desktop);\n#endif\nvoid desktop_shutdown(Desktop *desktop);\n",
        "desktop shutdown test hook",
    )


def update_desktop(text: str) -> str:'''
text, count = re.subn(header_pattern, lambda match: header_replacement,
                      text, count=1, flags=re.S)
if count != 1:
    raise SystemExit(f"repair expected one update_desktop_h function, found {count}")

old_init = r'''    text = replace_once(
        text,
        "    desktop->launcher.window_id = WINDOW_ID_INVALID;\n",
        "    desktop->launcher.window_id = WINDOW_ID_INVALID;\n    desktop->shutdown_waiting_window = WINDOW_ID_INVALID;\n",
        "shutdown initialization",
    )
'''
new_init = r'''    create_start = text.index("Desktop *desktop_create(")
    create_end = text.index("RetroAppInstance *app_launch_with_path(", create_start)
    create_body = text[create_start:create_end]
    create_body = replace_once(
        create_body,
        "    desktop->launcher.window_id = WINDOW_ID_INVALID;\n",
        "    desktop->launcher.window_id = WINDOW_ID_INVALID;\n    desktop->shutdown_waiting_window = WINDOW_ID_INVALID;\n",
        "shutdown initialization",
    )
    text = text[:create_start] + create_body + text[create_end:]
'''
if text.count(old_init) != 1:
    raise SystemExit(f"repair expected one shutdown initialization block, found {text.count(old_init)}")
text = text.replace(old_init, new_init, 1)

hook_pattern = r"\n    hook = '''RetroAppInstance \*desktop_app_instance_for_test.*?        \"shutdown test probe\",\n    \)\n"
hook_replacement = r'''
    return replace_once(
        text,
        "\nint desktop_run(Desktop *desktop) {\n",
        "\nbool desktop_shutdown_pending_for_test(const Desktop *desktop) {\n    return desktop && desktop->shutdown_requested;\n}\n\nint desktop_run(Desktop *desktop) {\n",
        "shutdown test probe",
    )
'''
text, count = re.subn(hook_pattern, lambda match: hook_replacement,
                      text, count=1, flags=re.S)
if count != 1:
    raise SystemExit(f"repair expected one shutdown hook block, found {count}")

runtime_marker = r'''def update_runtime_test(text: str) -> str:
    tests = r'''
runtime_replacement = r'''def update_runtime_test(text: str) -> str:
    text = text.replace(
        "TEST_REQUIRE(!instance.descriptor->can_close(&instance));",
        "TEST_REQUIRE(app_request_close(&instance) == RETRO_CLOSE_DEFERRED);",
    )
    text = replace_once(
        text,
        """    app_request_close(diagnostics);
    const RetroEvent events[] = {key_event(RETRO_KEY_CTRL_Q, '\\0')};
    platform_stub_set_events(platform, events, 1);
    TEST_REQUIRE(desktop_run(desktop) == EXIT_SUCCESS);

    /* Close request should have removed terminal app/window before exit. */
    TEST_REQUIRE(desktop_app_count(desktop) == base_apps);
    TEST_REQUIRE(desktop_window_count(desktop) == base_windows);
""",
        """    RetroEvent close = key_event(RETRO_KEY_CTRL_W, '\\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &close));

    /* A normal close removes only the active Diagnostics instance. */
    TEST_REQUIRE(desktop_app_count(desktop) == base_apps);
    TEST_REQUIRE(desktop_window_count(desktop) == base_windows);

    const RetroEvent events[] = {key_event(RETRO_KEY_CTRL_Q, '\\0')};
    platform_stub_set_events(platform, events, 1);
    TEST_REQUIRE(desktop_run(desktop) == EXIT_SUCCESS);

    /* A clean global shutdown commits every app close atomically. */
    TEST_REQUIRE(desktop_app_count(desktop) == 0);
    TEST_REQUIRE(desktop_window_count(desktop) == base_windows - base_apps);
""",
        "clean close and global shutdown test migration",
    )
    tests = r'''
if text.count(runtime_marker) != 1:
    raise SystemExit(f"repair expected one runtime test marker, found {text.count(runtime_marker)}")
text = text.replace(runtime_marker, runtime_replacement, 1)

path.write_text(text, encoding="utf-8")
