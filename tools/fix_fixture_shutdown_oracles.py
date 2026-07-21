#!/usr/bin/env python3
from pathlib import Path

path = Path("tests/retrocore_event_fixture_test.c")
text = path.read_text(encoding="utf-8")


def replace_once(source: str, old: str, new: str, label: str) -> str:
    count = source.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one marker, found {count}")
    return source.replace(old, new, 1)


def update_section(source: str, start_marker: str, end_marker: str,
                   transform) -> str:
    start = source.index(start_marker)
    end = source.index(end_marker, start)
    section = source[start:end]
    section = transform(section)
    return source[:start] + section + source[end:]


text = replace_once(
    text,
    "    if (platform->next_event >= platform->event_count) return RETRO_POLL_TIMEOUT;\n",
    "    if (platform->next_event >= platform->event_count) return RETRO_POLL_CLOSED;\n",
    "fixture exhaustion result",
)

quit_start = text.index("static RetroEvent make_quit_event(void) {")
quit_end = text.index("static RetroEvent make_focus_next_event(void) {", quit_start)
text = text[:quit_start] + text[quit_end:]


def update_drag(section: str) -> str:
    section = replace_once(section, "    RetroEvent events[4];\n",
                           "    RetroEvent events[3];\n",
                           "drag event array")
    return replace_once(
        section,
        '''    events[0] = make_pointer_down_event(local_down_x, local_down_y);
    events[1] = make_pointer_move_event(local_down_x + dx, local_down_y + dy);
    events[2] = make_pointer_up_event(local_down_x + dx, local_down_y + dy);
    events[3] = make_quit_event();
    platform_stub_set_events(platform, events, sizeof(events) / sizeof(events[0]));

    TEST_REQUIRE(desktop_run(desktop) == EXIT_SUCCESS);
''',
        '''    events[0] = make_pointer_down_event(local_down_x, local_down_y);
    events[1] = make_pointer_move_event(local_down_x + dx, local_down_y + dy);
    events[2] = make_pointer_up_event(local_down_x + dx, local_down_y + dy);
    platform_stub_set_events(platform, events, sizeof(events) / sizeof(events[0]));

    TEST_REQUIRE(desktop_run(desktop) == EXIT_SUCCESS);
''',
        "drag fixture replay",
    )


text = update_section(
    text,
    "static void replay_window_drag_basic_if_available(void) {",
    "static void replay_focus_next_basic_if_available(void) {",
    update_drag,
)


def update_focus(section: str) -> str:
    section = replace_once(section, "    RetroEvent events[2];\n",
                           "    RetroEvent event;\n",
                           "focus event declaration")
    return replace_once(
        section,
        '''    events[0] = make_focus_next_event();
    events[1] = make_quit_event();
    platform_stub_set_events(platform, events, sizeof(events) / sizeof(events[0]));

    TEST_REQUIRE(desktop_run(desktop) == EXIT_SUCCESS);
''',
        '''    event = make_focus_next_event();
    platform_stub_set_events(platform, &event, 1);

    TEST_REQUIRE(desktop_run(desktop) == EXIT_SUCCESS);
''',
        "focus fixture replay",
    )


text = update_section(
    text,
    "static void replay_focus_next_basic_if_available(void) {",
    "static void replay_close_focused_window_if_available(void) {",
    update_focus,
)


def update_close(section: str) -> str:
    section = replace_once(section, "    RetroEvent events[2];\n",
                           "    RetroEvent event;\n",
                           "close event declaration")
    return replace_once(
        section,
        '''    events[0] = make_close_focused_window_event();
    events[1] = make_quit_event();
    platform_stub_set_events(platform, events, sizeof(events) / sizeof(events[0]));

    TEST_REQUIRE(desktop_run(desktop) == EXIT_SUCCESS);
''',
        '''    event = make_close_focused_window_event();
    platform_stub_set_events(platform, &event, 1);

    TEST_REQUIRE(desktop_run(desktop) == EXIT_SUCCESS);
''',
        "close fixture replay",
    )


text = update_section(
    text,
    "static void replay_close_focused_window_if_available(void) {",
    "int main(void) {",
    update_close,
)

path.write_text(text, encoding="utf-8")
