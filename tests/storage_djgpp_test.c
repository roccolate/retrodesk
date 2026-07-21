#include "test_harness.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "storage/retro_fs.h"

#ifndef O_BINARY
#define O_BINARY 0
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

static bool ascii_case_equal(const char *left, const char *right) {
    if (!left || !right) return left == right;
    while (*left && *right) {
        unsigned char a = (unsigned char)*left++;
        unsigned char b = (unsigned char)*right++;
        if (a >= (unsigned char)'a' && a <= (unsigned char)'z') {
            a = (unsigned char)(a - (unsigned char)'a' + (unsigned char)'A');
        }
        if (b >= (unsigned char)'a' && b <= (unsigned char)'z') {
            b = (unsigned char)(b - (unsigned char)'a' + (unsigned char)'A');
        }
        if (a != b) return false;
    }
    return *left == '\0' && *right == '\0';
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
    for (size_t index = 0; index < names->count; ++index) {
        free(names->items[index]);
    }
    free(names->items);
    names->items = NULL;
    names->count = 0;
}

static RetroFsPath joined(const RetroFsPath *base, const char *name) {
    RetroFsPath result = {0};
    TEST_REQUIRE(retro_fs_path_join(base, name, &result) == RETRO_FS_OK);
    return result;
}

static void raw_write(const RetroFsPath *path, const char *data, size_t length) {
    int descriptor = open(retro_fs_path_cstr(path),
                          O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    TEST_REQUIRE(descriptor >= 0);
    size_t offset = 0;
    while (offset < length) {
        ssize_t amount = write(descriptor, data + offset, length - offset);
        TEST_REQUIRE(amount > 0);
        offset += (size_t)amount;
    }
    TEST_REQUIRE(close(descriptor) == 0);
}

static void remove_if_present(const char *path) {
    if (unlink(path) < 0) TEST_REQUIRE(errno == ENOENT);
}

static void remove_directory_if_present(const char *path) {
    if (rmdir(path) < 0) {
        TEST_REQUIRE(errno == ENOENT || errno == ENOTEMPTY);
    }
}

int main(void) {
    FILE *log_file = freopen("FSTEST.LOG", "w", stderr);
    TEST_REQUIRE(log_file != NULL);
    setvbuf(stderr, NULL, _IONBF, 0);
    char root_name[9];
    unsigned long suffix =
        ((unsigned long)getpid() ^ (unsigned long)clock()) & 0xFFFFFu;
    TEST_REQUIRE(snprintf(root_name, sizeof(root_name), "RD%05lX", suffix) == 7);
    remove_directory_if_present(root_name);

    RetroFsPath root = {0};
    TEST_REQUIRE(retro_fs_path_init(&root, root_name) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_create_directory(&root) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_create_directory(&root) == RETRO_FS_CONFLICT);

    RetroFsPath a = joined(&root, "A.TXT");
    RetroFsPath b = joined(&root, "B.TXT");
    TEST_REQUIRE(retro_fs_write_atomic(&a, "one\ntwo\n", 8, NULL, NULL) ==
                 RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_write_atomic(&b, "b\n", 2, NULL, NULL) == RETRO_FS_OK);

    Names names = {0};
    TEST_REQUIRE(retro_fs_list(&root, 100, collect_name, &names) == RETRO_FS_OK);
    TEST_REQUIRE(names.count == 3);
    TEST_REQUIRE(strcmp(names.items[0], "..") == 0);
    TEST_REQUIRE(ascii_case_equal(names.items[1], "A.TXT"));
    TEST_REQUIRE(ascii_case_equal(names.items[2], "B.TXT"));
    free_names(&names);

    RetroFsPath parent = {0};
    TEST_REQUIRE(retro_fs_path_parent(&a, &parent) == RETRO_FS_OK);
    TEST_REQUIRE(strcmp(retro_fs_path_cstr(&parent),
                        retro_fs_path_cstr(&root)) == 0);
    retro_fs_path_destroy(&parent);

    RetroFsPath rejected = {0};
    TEST_REQUIRE(retro_fs_path_join(&root, "BAD/NAME", &rejected) ==
                 RETRO_FS_INVALID_ARGUMENT);
    TEST_REQUIRE(retro_fs_path_join(&root, "ni\xC3\xB1o.txt", &rejected) ==
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
    TEST_REQUIRE(retro_fs_write_atomic(&a, "changed\n", 8,
                                       &version, &version) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_read_text(&a, &text, &length, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(strcmp(text, "changed\n") == 0);
    free(text);
    text = NULL;

    RetroFsVersion stale = version;
    raw_write(&a, "external\n", 9);
    TEST_REQUIRE(retro_fs_write_atomic(&a, "blocked\n", 8, &stale, NULL) ==
                 RETRO_FS_CONFLICT);

    RetroFsPath utf8 = joined(&root, "UTF8.TXT");
    const char *utf8_text = "ni\xC3\xB1o\nping\xC3\xBCino\n";
    size_t utf8_length = strlen(utf8_text);
    TEST_REQUIRE(retro_fs_write_atomic(&utf8, utf8_text, utf8_length,
                                       NULL, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_read_text(&utf8, &text, &length, NULL) == RETRO_FS_OK);
    TEST_REQUIRE(length == utf8_length);
    TEST_REQUIRE(memcmp(text, utf8_text, utf8_length) == 0);
    free(text);
    text = NULL;

    RetroFsPath invalid = joined(&root, "BAD.TXT");
    const char invalid_utf8[] = {(char)0xC3};
    TEST_REQUIRE(retro_fs_write_atomic(&invalid, invalid_utf8, 1,
                                       NULL, NULL) == RETRO_FS_INVALID_TEXT);
    raw_write(&invalid, invalid_utf8, 1);
    TEST_REQUIRE(retro_fs_read_text(&invalid, &text, &length, NULL) ==
                 RETRO_FS_INVALID_TEXT);
    TEST_REQUIRE(text == NULL);
    raw_write(&invalid, "lf\ncrlf\r\n", 9);
    TEST_REQUIRE(retro_fs_read_text(&invalid, &text, &length, NULL) ==
                 RETRO_FS_INVALID_TEXT);

    RetroFsPath created = joined(&root, "CREATE.TXT");
    RetroFsPath renamed = joined(&root, "RENAMED.TXT");
    RetroFsPath directory = joined(&root, "DOCS");
    RetroFsPath missing = joined(&root, "MISS.TXT");

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

    remove_if_present(retro_fs_path_cstr(&a));
    remove_if_present(retro_fs_path_cstr(&b));
    remove_if_present(retro_fs_path_cstr(&utf8));
    remove_if_present(retro_fs_path_cstr(&invalid));
    remove_if_present(retro_fs_path_cstr(&renamed));
    TEST_REQUIRE(rmdir(retro_fs_path_cstr(&directory)) == 0);

    retro_fs_path_destroy(&missing);
    retro_fs_path_destroy(&directory);
    retro_fs_path_destroy(&renamed);
    retro_fs_path_destroy(&created);
    retro_fs_path_destroy(&invalid);
    retro_fs_path_destroy(&utf8);
    retro_fs_path_destroy(&b);
    retro_fs_path_destroy(&a);
    TEST_REQUIRE(rmdir(retro_fs_path_cstr(&root)) == 0);
    retro_fs_path_destroy(&root);
    return 0;
}
