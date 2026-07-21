#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


cmake = ROOT / "CMakeLists.txt"
replace_once(
    cmake,
    '''# The first product slice uses the POSIX storage adapter. Windows and DOS use
# the header-only unsupported adapter until their native implementations land.
set(RETRODESK_STORAGE_SOURCE)
if (UNIX AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
    set(RETRODESK_STORAGE_SOURCE src/storage/retro_fs_posix.c)
    list(APPEND RETRODESK_SOURCES ${RETRODESK_STORAGE_SOURCE})
endif()
''',
    '''# Select the native storage adapter for each supported host. DOS keeps the
# header-only unsupported adapter until its dedicated implementation lands.
set(RETRODESK_STORAGE_SOURCE)
if (WIN32)
    set(RETRODESK_STORAGE_SOURCE src/storage/retro_fs_win32.c)
elseif (UNIX AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
    set(RETRODESK_STORAGE_SOURCE src/storage/retro_fs_posix.c)
endif()
if (RETRODESK_STORAGE_SOURCE)
    list(APPEND RETRODESK_SOURCES ${RETRODESK_STORAGE_SOURCE})
endif()
''',
)
replace_once(
    cmake,
    '''    if (UNIX AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
        add_executable(storage_test
            tests/storage_test.c
            src/storage/retro_fs_posix.c
            src/core/utf8.c
        )
        target_include_directories(storage_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        add_test(NAME storage_test COMMAND storage_test)
    endif()
''',
    '''    if (WIN32)
        add_executable(storage_test
            tests/storage_win32_test.c
            src/storage/retro_fs_win32.c
            src/core/utf8.c
        )
        target_include_directories(storage_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        add_test(NAME storage_test COMMAND storage_test)
    elseif (UNIX AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
        add_executable(storage_test
            tests/storage_test.c
            src/storage/retro_fs_posix.c
            src/core/utf8.c
        )
        target_include_directories(storage_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
        add_test(NAME storage_test COMMAND storage_test)
    endif()
''',
)

platform_test = ROOT / "tests/platform_features_test.c"
replace_once(
    platform_test,
    '#if defined(_WIN32) || defined(__DJGPP__)\nstatic bool unsupported_list_callback',
    '#if defined(__DJGPP__)\nstatic bool unsupported_list_callback',
)
replace_once(
    platform_test,
    '''#if defined(_WIN32) || defined(__DJGPP__)
    TEST_REQUIRE(!platform_input_backend_supported(INPUT_BACKEND_TTY_RAW));

    RetroFsPath path = {0};
    TEST_REQUIRE(retro_fs_path_init(&path, ".") == RETRO_FS_OK);
    TEST_REQUIRE(strstr(retro_fs_path_cstr(&path), "storage unavailable") != NULL);
    TEST_REQUIRE(retro_fs_list(&path, 1, unsupported_list_callback, NULL) ==
                 RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_create_file(&path) == RETRO_FS_UNSUPPORTED);
    TEST_REQUIRE(retro_fs_write_atomic(&path, "", 0, NULL, NULL) ==
                 RETRO_FS_UNSUPPORTED);
    retro_fs_path_destroy(&path);
#else
    TEST_REQUIRE(platform_input_backend_supported(INPUT_BACKEND_TTY_RAW));
#endif
''',
    '''#if defined(_WIN32) || defined(__DJGPP__)
    TEST_REQUIRE(!platform_input_backend_supported(INPUT_BACKEND_TTY_RAW));
#else
    TEST_REQUIRE(platform_input_backend_supported(INPUT_BACKEND_TTY_RAW));
#endif

#if defined(__DJGPP__)
    RetroFsPath path = {0};
    TEST_REQUIRE(retro_fs_path_init(&path, ".") == RETRO_FS_OK);
    TEST_REQUIRE(strstr(retro_fs_path_cstr(&path), "storage unavailable") != NULL);
    TEST_REQUIRE(retro_fs_list(&path, 1, unsupported_list_callback, NULL) ==
                 RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_create_file(&path) == RETRO_FS_UNSUPPORTED);
    TEST_REQUIRE(retro_fs_write_atomic(&path, "", 0, NULL, NULL) ==
                 RETRO_FS_UNSUPPORTED);
    retro_fs_path_destroy(&path);
#endif
''',
)

# This helper and its one-shot workflow must not remain in the product diff.
(ROOT / ".github/workflows/agent-win32-storage-patch.yml").unlink()
Path(__file__).unlink()
