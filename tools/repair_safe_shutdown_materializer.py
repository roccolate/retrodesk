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
    return text[:prompt_start] + prompt + text[prompt_end:]


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

path.write_text(text, encoding="utf-8")
