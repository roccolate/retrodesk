#!/usr/bin/env python3
"""Finalize generated core-routing changes before validation."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise SystemExit(f"{label}: expected one marker, found {text.count(old)}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


# Implementations must link into the test target even though declarations stay
# hidden behind RETRODESK_ENABLE_TEST_HOOKS in the public header.
desktop = ROOT / "src/core/desktop.c"
replace_once(
    desktop,
    "#ifdef RETRODESK_ENABLE_TEST_HOOKS\nbool desktop_dispatch_event_for_test",
    "bool desktop_dispatch_event_for_test",
    "desktop hook opening guard",
)
replace_once(
    desktop,
    "}\n#endif\n\nint desktop_run",
    "}\n\nint desktop_run",
    "desktop hook closing guard",
)

# The routing regression should not depend on the filesystem or on which entry
# File Manager initially selects. Use a focused hosted app that records F2.
test_file = ROOT / "tests/desktop_runtime_test.c"
old_test = r'''static void test_desktop_f2_reaches_filemanager(void) {
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
'''
new_test = r'''static int g_key_sink_f2_count;

static bool key_sink_create(RetroAppInstance *instance,
                            const RetroAppContext *ctx) {
    TEST_REQUIRE(instance != NULL);
    TEST_REQUIRE(ctx != NULL);
    instance->state = &g_key_sink_f2_count;
    return true;
}

static void key_sink_event(RetroAppInstance *instance,
                           const RetroEvent *event) {
    TEST_REQUIRE(instance != NULL);
    if (event && event->type == RETRO_EVENT_KEY &&
        event->data.key.key_code == RETRO_KEY_F2) {
        g_key_sink_f2_count++;
    }
}

static void key_sink_destroy(RetroAppInstance *instance) {
    TEST_REQUIRE(instance != NULL);
    instance->state = NULL;
}

static const RetroAppDescriptor k_key_sink_descriptor = {
    .app_id = "test-key-sink",
    .display_name = "Key Sink",
    .required_capabilities = PLATFORM_CAP_KEYBOARD_BASIC,
    .default_height = 6,
    .default_width = 24,
    .default_y = 2,
    .default_x = 4,
    .window_flags = WINDOW_FLAG_APP_OWNED,
    .create = key_sink_create,
    .on_event = key_sink_event,
    .destroy = key_sink_destroy,
};

static void test_desktop_f2_reaches_focused_app(void) {
    PlatformBackend *platform = NULL;
    Desktop *desktop = create_test_desktop(&platform);
    TEST_REQUIRE(desktop_register_app_for_test(
        desktop, &k_key_sink_descriptor));
    g_key_sink_f2_count = 0;
    RetroAppInstance *sink = app_launch(desktop, "test-key-sink");
    TEST_REQUIRE(sink != NULL);
    TEST_REQUIRE(desktop_active_window(desktop) == sink->ctx.window_id);

    RetroEvent f2 = key_event(RETRO_KEY_F2, '\0');
    TEST_REQUIRE(desktop_dispatch_event_for_test(desktop, &f2));
    TEST_REQUIRE(g_key_sink_f2_count == 1);
    TEST_REQUIRE(desktop_active_window(desktop) == sink->ctx.window_id);
    destroy_test_desktop(desktop, platform);
}
'''
replace_once(test_file, old_test, new_test, "deterministic F2 routing test")
replace_once(
    test_file,
    "    test_desktop_f2_reaches_filemanager();\n",
    "    test_desktop_f2_reaches_focused_app();\n",
    "F2 routing test registration",
)

# Remove the no-longer-needed File Manager private probe.
fm = ROOT / "src/apps/filemanager_app.c"
replace_once(
    fm,
    '''bool filemanager_prompt_active_for_test(
    const RetroAppInstance *instance) {
    const FileManagerState *state = instance ? instance->state : NULL;
    return state && state->prompt_mode != FM_PROMPT_NONE;
}

''',
    "",
    "remove File Manager prompt probe",
)

internal = ROOT / "src/apps/apps_internal.h"
replace_once(
    internal,
    '''bool filemanager_prompt_active_for_test(
    const RetroAppInstance *instance);
''',
    "",
    "remove File Manager prompt probe declaration",
)
