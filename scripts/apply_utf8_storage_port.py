#!/usr/bin/env python3
"""Enable validated UTF-8 text roundtrips in the POSIX storage adapter."""

from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one marker, found {count}")
    return text.replace(old, new, 1)


storage_path = Path("src/storage/retro_fs_posix.c")
storage = storage_path.read_text(encoding="utf-8")

if '#include "core/utf8.h"' not in storage:
    storage = replace_once(
        storage,
        '#include <unistd.h>\n',
        '#include <unistd.h>\n\n#include "core/utf8.h"\n',
        "UTF-8 storage include",
    )

validation_helper = r'''static bool valid_text_content(const char *data, size_t length) {
    if ((!data && length != 0) || !retro_utf8_validate(data, length)) {
        return false;
    }

    bool saw_lf = false;
    bool saw_crlf = false;
    size_t offset = 0;
    while (offset < length) {
        uint32_t codepoint = 0;
        size_t byte_len = 0;
        if (!retro_utf8_decode(data, length, offset,
                               &codepoint, &byte_len)) {
            return false;
        }

        if (codepoint == 0 || codepoint == 27 ||
            (codepoint < 32 && codepoint != '\t' &&
             codepoint != '\n' && codepoint != '\r') ||
            (codepoint >= 0x7Fu && codepoint < 0xA0u)) {
            return false;
        }

        if (codepoint == '\r') {
            if (offset + 1 >= length || data[offset + 1] != '\n') {
                return false;
            }
            saw_crlf = true;
        } else if (codepoint == '\n') {
            if (offset == 0 || data[offset - 1] != '\r') saw_lf = true;
        }
        offset += byte_len;
    }
    return !(saw_lf && saw_crlf);
}

'''
if "static bool valid_text_content" not in storage:
    storage = replace_once(
        storage,
        "RetroFsError retro_fs_read_text(const RetroFsPath *p, char **out, size_t *len,\n",
        validation_helper +
        "RetroFsError retro_fs_read_text(const RetroFsPath *p, char **out, size_t *len,\n",
        "storage text validator",
    )

old_read_validation = '''    bool saw_lf = false;
    bool saw_crlf = false;
    for (size_t i = 0; i < got; ++i) {
        unsigned char c = (unsigned char)buf[i];
        if (c == 0 || c == 27 ||
            (c < 32 && c != '\t' && c != '\n' && c != '\r') || c >= 127) {
            free(buf);
            return RETRO_FS_INVALID_TEXT;
        }
        if (c == '\r') {
            if (i + 1 >= got || buf[i + 1] != '\n') {
                free(buf);
                return RETRO_FS_INVALID_TEXT;
            }
            saw_crlf = true;
        } else if (c == '\n') {
            if (i == 0 || buf[i - 1] != '\r') saw_lf = true;
        }
    }
    if (saw_lf && saw_crlf) {
        free(buf);
        return RETRO_FS_INVALID_TEXT;
    }
'''
new_read_validation = '''    if (!valid_text_content(buf, got)) {
        free(buf);
        return RETRO_FS_INVALID_TEXT;
    }
'''
if old_read_validation in storage:
    storage = replace_once(
        storage,
        old_read_validation,
        new_read_validation,
        "storage read validation",
    )

write_guard = '''    if (!p || !p->value || (!data && n)) return RETRO_FS_INVALID_ARGUMENT;
    if (n > RETRO_FS_MAX_TEXT) return RETRO_FS_TOO_LARGE;
'''
write_guard_utf8 = '''    if (!p || !p->value || (!data && n)) return RETRO_FS_INVALID_ARGUMENT;
    if (n > RETRO_FS_MAX_TEXT) return RETRO_FS_TOO_LARGE;
    if (!valid_text_content(data, n)) return RETRO_FS_INVALID_TEXT;
'''
if "if (!valid_text_content(data, n))" not in storage:
    storage = replace_once(
        storage,
        write_guard,
        write_guard_utf8,
        "storage write validation",
    )

storage_path.write_text(storage, encoding="utf-8")


cmake_path = Path("CMakeLists.txt")
cmake = cmake_path.read_text(encoding="utf-8")
storage_target = '''        add_executable(storage_test
            tests/storage_test.c
            src/storage/retro_fs_posix.c
        )
'''
storage_target_utf8 = '''        add_executable(storage_test
            tests/storage_test.c
            src/storage/retro_fs_posix.c
            src/core/utf8.c
        )
'''
if storage_target in cmake:
    cmake = replace_once(
        cmake,
        storage_target,
        storage_target_utf8,
        "storage test UTF-8 source",
    )
cmake_path.write_text(cmake, encoding="utf-8")


test_path = Path("tests/storage_test.c")
test = test_path.read_text(encoding="utf-8")

if "path_utf8" not in test:
    test = replace_once(
        test,
        '''    char path_directory[512];
    char path_missing[512];
''',
        '''    char path_directory[512];
    char path_missing[512];
    char path_utf8[512];
    char path_invalid[512];
''',
        "storage UTF-8 path declarations",
    )
    test = replace_once(
        test,
        '''    TEST_REQUIRE(snprintf(path_directory, sizeof(path_directory), "%s/docs", dir) > 0);
    TEST_REQUIRE(snprintf(path_missing, sizeof(path_missing), "%s/missing.txt", dir) > 0);
''',
        '''    TEST_REQUIRE(snprintf(path_directory, sizeof(path_directory), "%s/docs", dir) > 0);
    TEST_REQUIRE(snprintf(path_missing, sizeof(path_missing), "%s/missing.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_utf8, sizeof(path_utf8), "%s/utf8.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_invalid, sizeof(path_invalid), "%s/invalid.txt", dir) > 0);
''',
        "storage UTF-8 path setup",
    )

utf8_tests = r'''    RetroFsPath utf8 = {0};
    RetroFsPath invalid = {0};
    TEST_REQUIRE(retro_fs_path_init(&utf8, path_utf8) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_path_init(&invalid, path_invalid) == RETRO_FS_OK);

    const char *utf8_text = "niño\npingüino\ndiseños\n";
    size_t utf8_length = strlen(utf8_text);
    TEST_REQUIRE(retro_fs_write_atomic(&utf8, utf8_text, utf8_length,
                                       NULL, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_read_text(&utf8, &text, &length, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(length == utf8_length);
    TEST_REQUIRE(strcmp(text, utf8_text) == 0);
    free(text);
    text = NULL;

    const char invalid_utf8[] = {(char)0xC3, '\0'};
    TEST_REQUIRE(retro_fs_write_atomic(&invalid, invalid_utf8, 1,
                                       NULL, NULL) == RETRO_FS_INVALID_TEXT);
    write_file(path_invalid, invalid_utf8);
    TEST_REQUIRE(retro_fs_read_text(&invalid, &text, &length, NULL) ==
                 RETRO_FS_INVALID_TEXT);
    TEST_REQUIRE(text == NULL);

    write_file(path_invalid, "lf\ncrlf\r\n");
    TEST_REQUIRE(retro_fs_read_text(&invalid, &text, &length, NULL) ==
                 RETRO_FS_INVALID_TEXT);
    TEST_REQUIRE(text == NULL);

    const char c1_control[] = {'a', (char)0xC2, (char)0x85, '\0'};
    TEST_REQUIRE(retro_fs_write_atomic(&invalid, c1_control, 3,
                                       NULL, NULL) == RETRO_FS_INVALID_TEXT);

'''
if "const char *utf8_text" not in test:
    test = replace_once(
        test,
        '''    TEST_REQUIRE(retro_fs_read_text(&fresh, &text, &length, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(strcmp(text, "new\n") == 0);
    free(text);

''',
        '''    TEST_REQUIRE(retro_fs_read_text(&fresh, &text, &length, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(strcmp(text, "new\n") == 0);
    free(text);
    text = NULL;

''' + utf8_tests,
        "storage UTF-8 roundtrip tests",
    )

if "retro_fs_path_destroy(&invalid);" not in test:
    test = replace_once(
        test,
        '''    retro_fs_path_destroy(&missing);
''',
        '''    retro_fs_path_destroy(&invalid);
    retro_fs_path_destroy(&utf8);
    retro_fs_path_destroy(&missing);
''',
        "storage UTF-8 path cleanup",
    )
    test = replace_once(
        test,
        '''    unlink(path_new);
    unlink(path_renamed);
''',
        '''    unlink(path_new);
    unlink(path_utf8);
    unlink(path_invalid);
    unlink(path_renamed);
''',
        "storage UTF-8 file cleanup",
    )

test_path.write_text(test, encoding="utf-8")
