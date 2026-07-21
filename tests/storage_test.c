#include "test_harness.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "storage/retro_fs.h"

typedef struct Names {
    char **items;
    size_t count;
} Names;

static bool collect_name(const RetroFsEntry *entry, void *userdata) {
    Names *names = userdata;
    char **next = realloc(names->items, (names->count + 1) * sizeof(*next));
    TEST_REQUIRE(next != NULL);
    names->items = next;
    names->items[names->count] = strdup(entry->name);
    TEST_REQUIRE(names->items[names->count] != NULL);
    names->count++;
    return true;
}

static void free_names(Names *names) {
    for (size_t i = 0; i < names->count; ++i) free(names->items[i]);
    free(names->items);
    names->items = NULL;
    names->count = 0;
}

static void write_file(const char *path, const char *text) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    TEST_REQUIRE(fd >= 0);
    size_t length = strlen(text);
    TEST_REQUIRE(write(fd, text, length) == (ssize_t)length);
    TEST_REQUIRE(close(fd) == 0);
}

int main(void) {
    char dir_template[] = "/tmp/retrodesk-fs-XXXXXX";
    char *dir = mkdtemp(dir_template);
    TEST_REQUIRE(dir != NULL);
    char path_a[512];
    char path_b[512];
    char path_new[512];
    char path_created[512];
    char path_renamed[512];
    char path_directory[512];
    char path_missing[512];
    char path_utf8[512];
    char path_invalid[512];
    char path_link[512];
    TEST_REQUIRE(snprintf(path_a, sizeof(path_a), "%s/a.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_b, sizeof(path_b), "%s/b.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_new, sizeof(path_new), "%s/new.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_created, sizeof(path_created), "%s/created.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_renamed, sizeof(path_renamed), "%s/renamed.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_directory, sizeof(path_directory), "%s/docs", dir) > 0);
    TEST_REQUIRE(snprintf(path_missing, sizeof(path_missing), "%s/missing.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_utf8, sizeof(path_utf8), "%s/utf8.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_invalid, sizeof(path_invalid), "%s/invalid.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_link, sizeof(path_link), "%s/link.txt", dir) > 0);
    write_file(path_a, "one\ntwo\n");
    write_file(path_b, "b\n");

    RetroFsPath root = {0};
    TEST_REQUIRE(retro_fs_path_init(&root, dir) == RETRO_FS_OK);
    Names names = {0};
    TEST_REQUIRE(retro_fs_list(&root, 100, collect_name, &names) == RETRO_FS_OK);
    TEST_REQUIRE(names.count == 3);
    TEST_REQUIRE(strcmp(names.items[0], "..") == 0);
    TEST_REQUIRE(strcmp(names.items[1], "a.txt") == 0);
    TEST_REQUIRE(strcmp(names.items[2], "b.txt") == 0);
    free_names(&names);

    RetroFsPath empty_join = {0};
    TEST_REQUIRE(retro_fs_path_join(&root, "", &empty_join) ==
                 RETRO_FS_INVALID_ARGUMENT);

    RetroFsPath a = {0};
    TEST_REQUIRE(retro_fs_path_init(&a, path_a) == RETRO_FS_OK);
    char *text = NULL;
    size_t length = 0;
    RetroFsVersion version = {0};
    TEST_REQUIRE(retro_fs_read_text(&a, &text, &length, &version) == RETRO_FS_OK);
    TEST_REQUIRE(length == 8);
    TEST_REQUIRE(strcmp(text, "one\ntwo\n") == 0);
    free(text);

    TEST_REQUIRE(retro_fs_write_atomic(&a, "unversioned\n", 12,
                                        NULL, NULL) == RETRO_FS_CONFLICT);
    TEST_REQUIRE(retro_fs_write_atomic(&a, "changed\n", 8, &version, &version) ==
                 RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_read_text(&a, &text, &length, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(strcmp(text, "changed\n") == 0);
    free(text);

    RetroFsVersion stale = version;
    write_file(path_a, "external\n");
    TEST_REQUIRE(retro_fs_write_atomic(&a, "blocked\n", 8, &stale, NULL) ==
                 RETRO_FS_CONFLICT);

    TEST_REQUIRE(symlink(path_a, path_link) == 0);
    RetroFsPath link_path = {0};
    TEST_REQUIRE(retro_fs_path_init(&link_path, path_link) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_stat(&link_path, &version) == RETRO_FS_OK);
    TEST_REQUIRE(version.kind == RETRO_FS_KIND_SYMLINK);
    TEST_REQUIRE(retro_fs_read_text(&link_path, &text, &length, NULL) ==
                 RETRO_FS_UNSUPPORTED);
    TEST_REQUIRE(text == NULL);
    TEST_REQUIRE(retro_fs_write_atomic(&link_path, "blocked\n", 8,
                                        NULL, NULL) == RETRO_FS_UNSUPPORTED);

    RetroFsPath fresh = {0};
    TEST_REQUIRE(retro_fs_path_init(&fresh, path_new) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_write_atomic(&fresh, "new\n", 4, NULL, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_read_text(&fresh, &text, &length, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(strcmp(text, "new\n") == 0);
    free(text);
    text = NULL;

    RetroFsPath utf8 = {0};
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

    RetroFsPath created = {0};
    RetroFsPath renamed = {0};
    RetroFsPath directory = {0};
    RetroFsPath existing = {0};
    RetroFsPath missing = {0};
    TEST_REQUIRE(retro_fs_path_init(&created, path_created) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_path_init(&renamed, path_renamed) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_path_init(&directory, path_directory) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_path_init(&existing, path_b) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_path_init(&missing, path_missing) == RETRO_FS_OK);

    TEST_REQUIRE(retro_fs_create_file(&created) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_create_file(&created) == RETRO_FS_CONFLICT);
    TEST_REQUIRE(retro_fs_stat(&created, &version) == RETRO_FS_OK);
    TEST_REQUIRE(version.valid);
    TEST_REQUIRE(version.kind == RETRO_FS_KIND_REGULAR);
    TEST_REQUIRE(version.size == 0);

    TEST_REQUIRE(retro_fs_rename(&created, &renamed) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_stat(&created, &version) == RETRO_FS_NOT_FOUND);
    TEST_REQUIRE(retro_fs_stat(&renamed, &version) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_rename(&renamed, &existing) == RETRO_FS_CONFLICT);
    TEST_REQUIRE(retro_fs_stat(&renamed, &version) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_rename(&missing, &created) == RETRO_FS_NOT_FOUND);

    TEST_REQUIRE(retro_fs_create_directory(&directory) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_create_directory(&directory) == RETRO_FS_CONFLICT);
    TEST_REQUIRE(retro_fs_stat(&directory, &version) == RETRO_FS_OK);
    TEST_REQUIRE(version.valid);
    TEST_REQUIRE(version.kind == RETRO_FS_KIND_DIRECTORY);

    retro_fs_path_destroy(&link_path);
    retro_fs_path_destroy(&invalid);
    retro_fs_path_destroy(&utf8);
    retro_fs_path_destroy(&missing);
    retro_fs_path_destroy(&existing);
    retro_fs_path_destroy(&directory);
    retro_fs_path_destroy(&renamed);
    retro_fs_path_destroy(&created);
    retro_fs_path_destroy(&fresh);
    retro_fs_path_destroy(&a);
    retro_fs_path_destroy(&root);
    unlink(path_link);
    unlink(path_a);
    unlink(path_b);
    unlink(path_new);
    unlink(path_utf8);
    unlink(path_invalid);
    unlink(path_renamed);
    TEST_REQUIRE(rmdir(path_directory) == 0);
    TEST_REQUIRE(rmdir(dir) == 0);
    return 0;
}
