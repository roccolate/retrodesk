#include "test_harness.h"

#include <fcntl.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
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


    char path_private[512];
    char path_private_link[512];
    char path_private_victim[512];
    char path_partial[512];
    char path_race[512];
    TEST_REQUIRE(snprintf(path_private, sizeof(path_private),
                          "%s/private.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_private_link, sizeof(path_private_link),
                          "%s/private-link.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_private_victim, sizeof(path_private_victim),
                          "%s/private-victim.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_partial, sizeof(path_partial),
                          "%s/partial.txt", dir) > 0);
    TEST_REQUIRE(snprintf(path_race, sizeof(path_race),
                          "%s/race.txt", dir) > 0);

    RetroFsPath private_path = {0};
    RetroFsPath private_link = {0};
    RetroFsPath partial_path = {0};
    RetroFsPath race_path = {0};
    TEST_REQUIRE(retro_fs_path_init(&private_path, path_private) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_path_init(&private_link, path_private_link) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_path_init(&partial_path, path_partial) == RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_path_init(&race_path, path_race) == RETRO_FS_OK);

    mode_t previous_umask = umask(0);
    TEST_REQUIRE(retro_fs_write_new_private(&private_path, "secret\n", 7) ==
                 RETRO_FS_OK);
    (void)umask(previous_umask);
    struct stat private_metadata;
    TEST_REQUIRE(stat(path_private, &private_metadata) == 0);
    TEST_REQUIRE((private_metadata.st_mode & 0777) == 0600);
    TEST_REQUIRE(retro_fs_write_new_private(&private_path, "replace\n", 8) ==
                 RETRO_FS_CONFLICT);
    TEST_REQUIRE(retro_fs_read_text(&private_path, &text, &length, NULL) ==
                 RETRO_FS_OK);
    TEST_REQUIRE(strcmp(text, "secret\n") == 0);
    free(text);
    text = NULL;

    write_file(path_private_victim, "victim\n");
    TEST_REQUIRE(symlink(path_private_victim, path_private_link) == 0);
    TEST_REQUIRE(retro_fs_write_new_private(&private_link, "attack\n", 7) ==
                 RETRO_FS_CONFLICT);
    RetroFsPath private_victim = {0};
    TEST_REQUIRE(retro_fs_path_init(&private_victim, path_private_victim) ==
                 RETRO_FS_OK);
    TEST_REQUIRE(retro_fs_read_text(&private_victim, &text, &length, NULL) ==
                 RETRO_FS_OK);
    TEST_REQUIRE(strcmp(text, "victim\n") == 0);
    free(text);
    text = NULL;

    struct rlimit original_limit;
    TEST_REQUIRE(getrlimit(RLIMIT_FSIZE, &original_limit) == 0);
    if (original_limit.rlim_cur == RLIM_INFINITY || original_limit.rlim_cur > 1) {
        struct rlimit limited = original_limit;
        limited.rlim_cur = 1;
        void (*previous_handler)(int) = signal(SIGXFSZ, SIG_IGN);
        TEST_REQUIRE(previous_handler != SIG_ERR);
        TEST_REQUIRE(setrlimit(RLIMIT_FSIZE, &limited) == 0);
        RetroFsError partial_status = retro_fs_write_new_private(
            &partial_path, "partial\n", 8);
        TEST_REQUIRE(setrlimit(RLIMIT_FSIZE, &original_limit) == 0);
        TEST_REQUIRE(signal(SIGXFSZ, previous_handler) != SIG_ERR);
        TEST_REQUIRE(partial_status != RETRO_FS_OK);
        TEST_REQUIRE(retro_fs_stat(&partial_path, &version) ==
                     RETRO_FS_NOT_FOUND);
    }

    int gate[2];
    TEST_REQUIRE(pipe(gate) == 0);
    pid_t first_writer = fork();
    TEST_REQUIRE(first_writer >= 0);
    if (first_writer == 0) {
        char token = 0;
        (void)close(gate[1]);
        if (read(gate[0], &token, 1) != 1) _exit(3);
        RetroFsError status = retro_fs_write_new_private(
            &race_path, "race\n", 5);
        _exit(status == RETRO_FS_OK ? 0 :
              (status == RETRO_FS_CONFLICT ? 2 : 3));
    }
    pid_t second_writer = fork();
    TEST_REQUIRE(second_writer >= 0);
    if (second_writer == 0) {
        char token = 0;
        (void)close(gate[1]);
        if (read(gate[0], &token, 1) != 1) _exit(3);
        RetroFsError status = retro_fs_write_new_private(
            &race_path, "race\n", 5);
        _exit(status == RETRO_FS_OK ? 0 :
              (status == RETRO_FS_CONFLICT ? 2 : 3));
    }
    (void)close(gate[0]);
    TEST_REQUIRE(write(gate[1], "xx", 2) == 2);
    TEST_REQUIRE(close(gate[1]) == 0);
    int first_status = 0;
    int second_status = 0;
    TEST_REQUIRE(waitpid(first_writer, &first_status, 0) == first_writer);
    TEST_REQUIRE(waitpid(second_writer, &second_status, 0) == second_writer);
    TEST_REQUIRE(WIFEXITED(first_status) && WIFEXITED(second_status));
    int first_code = WEXITSTATUS(first_status);
    int second_code = WEXITSTATUS(second_status);
    TEST_REQUIRE((first_code == 0 && second_code == 2) ||
                 (first_code == 2 && second_code == 0));

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


    retro_fs_path_destroy(&race_path);
    retro_fs_path_destroy(&partial_path);
    retro_fs_path_destroy(&private_victim);
    retro_fs_path_destroy(&private_link);
    retro_fs_path_destroy(&private_path);
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

    unlink(path_race);
    unlink(path_partial);
    unlink(path_private_link);
    unlink(path_private_victim);
    unlink(path_private);
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
