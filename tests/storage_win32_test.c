#include "test_harness.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "storage/retro_fs.h"

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

typedef struct Names {
    char **items;
    size_t count;
} Names;

static char *test_duplicate(const char *value) {
    size_t length = strlen(value);
    char *copy = malloc(length + 1);
    TEST_REQUIRE(copy != NULL);
    memcpy(copy, value, length + 1);
    return copy;
}

static bool collect_name(const RetroFsEntry *entry, void *userdata) {
    Names *names = userdata;
    char **next = realloc(names->items, (names->count + 1) * sizeof(*next));
    TEST_REQUIRE(next != NULL);
    names->items = next;
    names->items[names->count++] = test_duplicate(entry->name);
    return true;
}

static void free_names(Names *names) {
    for (size_t i = 0; i < names->count; ++i) free(names->items[i]);
    free(names->items);
    names->items = NULL;
    names->count = 0;
}

static wchar_t *utf8_to_wide(const char *value) {
    size_t length = strlen(value);
    TEST_REQUIRE(length <= INT_MAX);
    int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value,
                                       (int)length, NULL, 0);
    TEST_REQUIRE(required > 0);
    wchar_t *wide = malloc(((size_t)required + 1) * sizeof(*wide));
    TEST_REQUIRE(wide != NULL);
    TEST_REQUIRE(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value,
                                     (int)length, wide, required) == required);
    wide[required] = L'\0';
    for (int i = 0; i < required; ++i) {
        if (wide[i] == L'/') wide[i] = L'\\';
    }
    return wide;
}

static char *wide_to_utf8(const wchar_t *value) {
    size_t length = wcslen(value);
    TEST_REQUIRE(length <= INT_MAX);
    int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value,
                                       (int)length, NULL, 0, NULL, NULL);
    TEST_REQUIRE(required > 0);
    char *utf8 = malloc((size_t)required + 1);
    TEST_REQUIRE(utf8 != NULL);
    TEST_REQUIRE(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value,
                                     (int)length, utf8, required, NULL, NULL) ==
                 required);
    utf8[required] = '\0';
    return utf8;
}

static void raw_write(const RetroFsPath *path, const char *data, size_t length) {
    wchar_t *wide = utf8_to_wide(retro_fs_path_cstr(path));
    HANDLE handle = CreateFileW(wide, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                                NULL);
    TEST_REQUIRE(handle != INVALID_HANDLE_VALUE);
    size_t offset = 0;
    while (offset < length) {
        DWORD written = 0;
        DWORD wanted = (DWORD)(length - offset);
        TEST_REQUIRE(WriteFile(handle, data + offset, wanted, &written, NULL));
        TEST_REQUIRE(written > 0);
        offset += (size_t)written;
    }
    TEST_REQUIRE(FlushFileBuffers(handle));
    TEST_REQUIRE(CloseHandle(handle));
    free(wide);
}

static void delete_file(const RetroFsPath *path) {
    wchar_t *wide = utf8_to_wide(retro_fs_path_cstr(path));
    TEST_REQUIRE(DeleteFileW(wide));
    free(wide);
}

static void delete_directory(const RetroFsPath *path) {
    wchar_t *wide = utf8_to_wide(retro_fs_path_cstr(path));
    TEST_REQUIRE(RemoveDirectoryW(wide));
    free(wide);
}

static void assert_private_acl(const RetroFsPath *path) {
    wchar_t *wide = utf8_to_wide(retro_fs_path_cstr(path));
    PACL dacl = NULL;
    PSECURITY_DESCRIPTOR descriptor = NULL;
    DWORD status = GetNamedSecurityInfoW(
        wide, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
        NULL, NULL, &dacl, NULL, &descriptor);
    TEST_REQUIRE(status == ERROR_SUCCESS);
    TEST_REQUIRE(dacl != NULL);

    SECURITY_DESCRIPTOR_CONTROL control = 0;
    DWORD revision = 0;
    TEST_REQUIRE(GetSecurityDescriptorControl(descriptor, &control, &revision));
    TEST_REQUIRE((control & SE_DACL_PROTECTED) != 0);
    ACL_SIZE_INFORMATION information = {0};
    TEST_REQUIRE(GetAclInformation(dacl, &information, sizeof(information),
                                   AclSizeInformation));
    TEST_REQUIRE(information.AceCount == 1);
    ACCESS_ALLOWED_ACE *ace = NULL;
    TEST_REQUIRE(GetAce(dacl, 0, (void **)&ace));
    TEST_REQUIRE(ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE);
    TEST_REQUIRE((ace->Header.AceFlags & INHERITED_ACE) == 0);

    HANDLE token = NULL;
    TEST_REQUIRE(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token));
    DWORD needed = 0;
    (void)GetTokenInformation(token, TokenUser, NULL, 0, &needed);
    TEST_REQUIRE(GetLastError() == ERROR_INSUFFICIENT_BUFFER && needed > 0);
    TOKEN_USER *user = malloc(needed);
    TEST_REQUIRE(user != NULL);
    TEST_REQUIRE(GetTokenInformation(token, TokenUser, user, needed, &needed));
    TEST_REQUIRE(EqualSid(&ace->SidStart, user->User.Sid));
    free(user);
    TEST_REQUIRE(CloseHandle(token));
    LocalFree(descriptor);
    free(wide);
}

static RetroFsPath joined(const RetroFsPath *base, const char *name) {
    RetroFsPath result = {0};
    TEST_REQUIRE(retro_fs_path_join(base, name, &result) == RETRO_FS_OK);
    return result;
}

int main(void) {
    wchar_t temp_directory[MAX_PATH + 1];
    DWORD temp_length = GetTempPathW(MAX_PATH, temp_directory);
    TEST_REQUIRE(temp_length > 0 && temp_length < MAX_PATH);
    wchar_t unique_path[MAX_PATH + 1];
    TEST_REQUIRE(GetTempFileNameW(temp_directory, L"rtd", 0, unique_path) != 0);
    TEST_REQUIRE(DeleteFileW(unique_path));
    TEST_REQUIRE(CreateDirectoryW(unique_path, NULL));
    char *root_utf8 = wide_to_utf8(unique_path);

    RetroFsPath root = {0};
    TEST_REQUIRE(retro_fs_path_init(&root, root_utf8) == RETRO_FS_OK);
    free(root_utf8);

    RetroFsPath a = joined(&root, "a.txt");
    RetroFsPath b = joined(&root, "b.txt");
    raw_write(&a, "one\ntwo\n", 8);
    raw_write(&b, "b\n", 2);

    Names names = {0};
    TEST_REQUIRE(retro_fs_list(&root, 100, collect_name, &names) == RETRO_FS_OK);
    TEST_REQUIRE(names.count == 3);
    TEST_REQUIRE(strcmp(names.items[0], "..") == 0);
    TEST_REQUIRE(strcmp(names.items[1], "a.txt") == 0);
    TEST_REQUIRE(strcmp(names.items[2], "b.txt") == 0);
    free_names(&names);

    RetroFsPath parent = {0};
    TEST_REQUIRE(retro_fs_path_parent(&a, &parent) == RETRO_FS_OK);
    TEST_REQUIRE(strcmp(retro_fs_path_cstr(&parent), retro_fs_path_cstr(&root)) == 0);
    retro_fs_path_destroy(&parent);

    RetroFsPath invalid_join = {0};
    TEST_REQUIRE(retro_fs_path_join(&root, "bad\\name", &invalid_join) ==
                 RETRO_FS_INVALID_ARGUMENT);
    TEST_REQUIRE(retro_fs_path_join(&root, "stream:name", &invalid_join) ==
                 RETRO_FS_INVALID_ARGUMENT);

    char *text = NULL;
    size_t length = 0;
    RetroFsVersion version = {0};
    TEST_REQUIRE(retro_fs_read_text(&a, &text, &length, &version) == RETRO_FS_OK);
    TEST_REQUIRE(version.valid);
    TEST_REQUIRE(version.kind == RETRO_FS_KIND_REGULAR);
    TEST_REQUIRE(length == 8);
    TEST_REQUIRE(strcmp(text, "one\ntwo\n") == 0);
    free(text);
    text = NULL;

    TEST_REQUIRE(retro_fs_write_atomic(&a, "unversioned\n", 12, NULL, NULL) ==
                 RETRO_FS_CONFLICT);
    TEST_REQUIRE(retro_fs_write_atomic(&a, "changed\n", 8, &version, &version) ==
                 RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_read_text(&a, &text, &length, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(strcmp(text, "changed\n") == 0);
    free(text);
    text = NULL;

    RetroFsVersion stale = version;
    raw_write(&a, "external\n", 9);
    TEST_REQUIRE(retro_fs_write_atomic(&a, "blocked\n", 8, &stale, NULL) ==
                 RETRO_FS_CONFLICT);

    RetroFsPath fresh = joined(&root, "new.txt");
    TEST_REQUIRE(retro_fs_write_atomic(&fresh, "new\n", 4, NULL, NULL) ==
                 RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_read_text(&fresh, &text, &length, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(strcmp(text, "new\n") == 0);
    free(text);
    text = NULL;

    RetroFsPath utf8 = joined(&root, "niño.txt");
    const char *utf8_text = "niño\npingüino\ndiseños\n";
    size_t utf8_length = strlen(utf8_text);
    TEST_REQUIRE(retro_fs_write_atomic(&utf8, utf8_text, utf8_length, NULL, NULL) ==
                 RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_read_text(&utf8, &text, &length, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(length == utf8_length);
    TEST_REQUIRE(strcmp(text, utf8_text) == 0);
    free(text);
    text = NULL;

    RetroFsPath invalid = joined(&root, "invalid.txt");
    const char invalid_utf8[] = {(char)0xC3, '\0'};
    TEST_REQUIRE(retro_fs_write_atomic(&invalid, invalid_utf8, 1, NULL, NULL) ==
                 RETRO_FS_INVALID_TEXT);
    raw_write(&invalid, invalid_utf8, 1);
    TEST_REQUIRE(retro_fs_read_text(&invalid, &text, &length, NULL) ==
                 RETRO_FS_INVALID_TEXT);
    TEST_REQUIRE(text == NULL);
    raw_write(&invalid, "lf\ncrlf\r\n", 9);
    TEST_REQUIRE(retro_fs_read_text(&invalid, &text, &length, NULL) ==
                 RETRO_FS_INVALID_TEXT);


    RetroFsPath private_path = joined(&root, "recovery.txt");
    TEST_REQUIRE(retro_fs_write_new_private(&private_path, "secret\n", 7) ==
                 RETRO_FS_OK);
    assert_private_acl(&private_path);
    TEST_REQUIRE(retro_fs_write_new_private(&private_path, "replace\n", 8) ==
                 RETRO_FS_CONFLICT);
    TEST_REQUIRE(retro_fs_read_text(&private_path, &text, &length, NULL) ==
                 RETRO_FS_OK);
    TEST_REQUIRE(strcmp(text, "secret\n") == 0);
    free(text);
    text = NULL;

    RetroFsPath private_victim = joined(&root, "victim.txt");
    RetroFsPath private_link = joined(&root, "victim-link.txt");
    raw_write(&private_victim, "victim\n", 7);
    wchar_t *victim_native = utf8_to_wide(retro_fs_path_cstr(&private_victim));
    wchar_t *link_native = utf8_to_wide(retro_fs_path_cstr(&private_link));
    if (CreateSymbolicLinkW(link_native, victim_native,
                            SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
        TEST_REQUIRE(retro_fs_write_new_private(&private_link, "attack\n", 7) ==
                     RETRO_FS_CONFLICT);
        TEST_REQUIRE(retro_fs_read_text(&private_victim, &text, &length, NULL) ==
                     RETRO_FS_OK);
        TEST_REQUIRE(strcmp(text, "victim\n") == 0);
        free(text);
        text = NULL;
        delete_file(&private_link);
    } else {
        DWORD link_error = GetLastError();
        TEST_REQUIRE(link_error == ERROR_PRIVILEGE_NOT_HELD ||
                     link_error == ERROR_ACCESS_DENIED ||
                     link_error == ERROR_INVALID_PARAMETER ||
                     link_error == ERROR_NOT_SUPPORTED);
    }
    free(link_native);
    free(victim_native);

    RetroFsPath created = joined(&root, "created.txt");
    RetroFsPath renamed = joined(&root, "renamed.txt");
    RetroFsPath directory = joined(&root, "docs");
    RetroFsPath missing = joined(&root, "missing.txt");

    TEST_REQUIRE(retro_fs_create_file(&created) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_create_file(&created) == RETRO_FS_CONFLICT);
    TEST_REQUIRE(retro_fs_stat(&created, &version) == RETRO_FS_OK);
    TEST_REQUIRE(version.kind == RETRO_FS_KIND_REGULAR && version.size == 0);
    TEST_REQUIRE(retro_fs_rename(&created, &renamed) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_stat(&created, &version) == RETRO_FS_NOT_FOUND);
    TEST_REQUIRE(retro_fs_stat(&renamed, &version) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_rename(&renamed, &b) == RETRO_FS_CONFLICT);
    TEST_REQUIRE(retro_fs_stat(&renamed, &version) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_rename(&missing, &created) == RETRO_FS_NOT_FOUND);

    TEST_REQUIRE(retro_fs_create_directory(&directory) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_create_directory(&directory) == RETRO_FS_CONFLICT);
    TEST_REQUIRE(retro_fs_stat(&directory, &version) == RETRO_FS_OK);
    TEST_REQUIRE(version.kind == RETRO_FS_KIND_DIRECTORY);
    TEST_REQUIRE(retro_fs_read_text(&directory, &text, &length, NULL) ==
                 RETRO_FS_UNSUPPORTED);

    delete_file(&private_victim);
    delete_file(&private_path);
    delete_file(&a);
    delete_file(&b);
    delete_file(&fresh);
    delete_file(&utf8);
    delete_file(&invalid);
    delete_file(&renamed);
    delete_directory(&directory);


    retro_fs_path_destroy(&private_link);
    retro_fs_path_destroy(&private_victim);
    retro_fs_path_destroy(&private_path);
    retro_fs_path_destroy(&missing);
    retro_fs_path_destroy(&directory);
    retro_fs_path_destroy(&renamed);
    retro_fs_path_destroy(&created);
    retro_fs_path_destroy(&invalid);
    retro_fs_path_destroy(&utf8);
    retro_fs_path_destroy(&fresh);
    retro_fs_path_destroy(&b);
    retro_fs_path_destroy(&a);
    delete_directory(&root);
    retro_fs_path_destroy(&root);
    return 0;
}
