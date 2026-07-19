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
    TEST_REQUIRE(snprintf(path_a, sizeof(path_a), "%s/a.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_b, sizeof(path_b), "%s/b.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_new, sizeof(path_new), "%s/new.txt", dir) > 0);
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

    RetroFsPath a = {0};
    TEST_REQUIRE(retro_fs_path_init(&a, path_a) == RETRO_FS_OK);
    char *text = NULL;
    size_t length = 0;
    RetroFsVersion version = {0};
    TEST_REQUIRE(retro_fs_read_text(&a, &text, &length, &version) == RETRO_FS_OK);
    TEST_REQUIRE(length == 8);
    TEST_REQUIRE(strcmp(text, "one\ntwo\n") == 0);
    free(text);

    TEST_REQUIRE(retro_fs_write_atomic(&a, "changed\n", 8, &version, &version) ==
                 RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_read_text(&a, &text, &length, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(strcmp(text, "changed\n") == 0);
    free(text);

    RetroFsVersion stale = version;
    write_file(path_a, "external\n");
    TEST_REQUIRE(retro_fs_write_atomic(&a, "blocked\n", 8, &stale, NULL) ==
                 RETRO_FS_CONFLICT);

    RetroFsPath fresh = {0};
    TEST_REQUIRE(retro_fs_path_init(&fresh, path_new) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_write_atomic(&fresh, "new\n", 4, NULL, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_read_text(&fresh, &text, &length, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(strcmp(text, "new\n") == 0);
    free(text);

    retro_fs_path_destroy(&fresh);
    retro_fs_path_destroy(&a);
    retro_fs_path_destroy(&root);
    unlink(path_a);
    unlink(path_b);
    unlink(path_new);
    TEST_REQUIRE(rmdir(dir) == 0);
    return 0;
}
