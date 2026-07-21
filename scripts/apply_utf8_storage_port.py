#!/usr/bin/env python3
"""Enable validated UTF-8 text roundtrips in the POSIX storage adapter."""

from __future__ import annotations

import re
from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one marker, found {count}")
    return text.replace(old, new, 1)


def sub_once(text: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.DOTALL)
    if count != 1:
        raise SystemExit(f"{label}: expected one match, found {count}")
    return updated


# ---------------------------------------------------------------------------
# POSIX adapter
# ---------------------------------------------------------------------------

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
    marker = "RetroFsError retro_fs_read_text(const RetroFsPath *p, char **out, size_t *len,"
    storage = replace_once(
        storage,
        marker,
        validation_helper + marker,
        "storage text validator",
    )

if "if (!valid_text_content(buf, got))" not in storage:
    storage = sub_once(
        storage,
        r"    bool saw_lf = false;\n"
        r".*?"
        r"    if \(saw_lf && saw_crlf\) \{\n"
        r"        free\(buf\);\n"
        r"        return RETRO_FS_INVALID_TEXT;\n"
        r"    \}\n",
        "    if (!valid_text_content(buf, got)) {\n"
        "        free(buf);\n"
        "        return RETRO_FS_INVALID_TEXT;\n"
        "    }\n",
        "storage read validation",
    )

if "if (!valid_text_content(data, n))" not in storage:
    storage = replace_once(
        storage,
        "    if (n > RETRO_FS_MAX_TEXT) return RETRO_FS_TOO_LARGE;\n",
        "    if (n > RETRO_FS_MAX_TEXT) return RETRO_FS_TOO_LARGE;\n"
        "    if (!valid_text_content(data, n)) return RETRO_FS_INVALID_TEXT;\n",
        "storage write validation",
    )

storage_path.write_text(storage, encoding="utf-8")


# ---------------------------------------------------------------------------
# Build graph
# ---------------------------------------------------------------------------

cmake_path = Path("CMakeLists.txt")
cmake = cmake_path.read_text(encoding="utf-8")

if re.search(
    r"add_executable\(storage_test\s+"
    r"tests/storage_test\.c\s+"
    r"src/storage/retro_fs_posix\.c\s+"
    r"src/core/utf8\.c\s+\)",
    cmake,
    flags=re.DOTALL,
) is None:
    cmake = sub_once(
        cmake,
        r"(add_executable\(storage_test\s+"
        r"tests/storage_test\.c\s+"
        r"src/storage/retro_fs_posix\.c)(\s+\))",
        r"\1\n            src/core/utf8.c\2",
        "storage test UTF-8 source",
    )

cmake_path.write_text(cmake, encoding="utf-8")


# ---------------------------------------------------------------------------
# Storage regression tests
# ---------------------------------------------------------------------------

test_path = Path("tests/storage_test.c")
test = test_path.read_text(encoding="utf-8")

if "char path_utf8[512];" not in test:
    test = replace_once(
        test,
        "    char path_missing[512];\n",
        "    char path_missing[512];\n"
        "    char path_utf8[512];\n"
        "    char path_invalid[512];\n",
        "storage UTF-8 path declarations",
    )

if '"%s/utf8.txt"' not in test:
    test = replace_once(
        test,
        '    TEST_REQUIRE(snprintf(path_missing, sizeof(path_missing), "%s/missing.txt", dir) > 0);\n',
        '    TEST_REQUIRE(snprintf(path_missing, sizeof(path_missing), "%s/missing.txt", dir) > 0);\n'
        '    TEST_REQUIRE(snprintf(path_utf8, sizeof(path_utf8), "%s/utf8.txt", dir) > 0);\n'
        '    TEST_REQUIRE(snprintf(path_invalid, sizeof(path_invalid), "%s/invalid.txt", dir) > 0);\n',
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
    test = sub_once(
        test,
        r"(    TEST_REQUIRE\(retro_fs_read_text\(&fresh, &text, &length, NULL\) == RETRO_FS_OK\);\n"
        r"    TEST_REQUIRE\(strcmp\(text, \"new\\n\"\) == 0\);\n"
        r"    free\(text\);\n)",
        r"\1    text = NULL;\n\n" + utf8_tests,
        "storage UTF-8 roundtrip tests",
    )

if "retro_fs_path_destroy(&invalid);" not in test:
    test = replace_once(
        test,
        "    retro_fs_path_destroy(&missing);\n",
        "    retro_fs_path_destroy(&invalid);\n"
        "    retro_fs_path_destroy(&utf8);\n"
        "    retro_fs_path_destroy(&missing);\n",
        "storage UTF-8 path cleanup",
    )

if "    unlink(path_utf8);" not in test:
    test = replace_once(
        test,
        "    unlink(path_new);\n",
        "    unlink(path_new);\n"
        "    unlink(path_utf8);\n"
        "    unlink(path_invalid);\n",
        "storage UTF-8 file cleanup",
    )

test_path.write_text(test, encoding="utf-8")
