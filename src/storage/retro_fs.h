#ifndef RETRODESK_STORAGE_RETRO_FS_H
#define RETRODESK_STORAGE_RETRO_FS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <time.h>

typedef enum {
    RETRO_FS_OK = 0,
    RETRO_FS_NOT_FOUND,
    RETRO_FS_PERMISSION,
    RETRO_FS_UNSUPPORTED,
    RETRO_FS_TOO_LARGE,
    RETRO_FS_INVALID_TEXT,
    RETRO_FS_CONFLICT,
    RETRO_FS_OOM,
    RETRO_FS_IO,
    RETRO_FS_INVALID_ARGUMENT
} RetroFsError;

typedef struct {
    char *value;
} RetroFsPath;

typedef struct {
    dev_t device;
    ino_t inode;
    off_t size;
    time_t mtime;
    mode_t mode;
} RetroFsVersion;

typedef struct {
    char *name;
    mode_t mode;
    off_t size;
    RetroFsVersion version;
} RetroFsEntry;

typedef bool (*RetroFsListFn)(const RetroFsEntry *entry, void *userdata);

RetroFsError retro_fs_path_init(RetroFsPath *path, const char *value);
void retro_fs_path_destroy(RetroFsPath *path);
const char *retro_fs_path_cstr(const RetroFsPath *path);
RetroFsError retro_fs_path_parent(const RetroFsPath *path, RetroFsPath *parent);
RetroFsError retro_fs_path_join(const RetroFsPath *base, const char *name,
                                 RetroFsPath *joined);

RetroFsError retro_fs_stat(const RetroFsPath *path, RetroFsVersion *version);
/* Lists at most max_entries entries. A directory larger than the limit is
   rejected before any partial result is delivered. */
RetroFsError retro_fs_list(const RetroFsPath *path, size_t max_entries,
                            RetroFsListFn callback, void *userdata);
RetroFsError retro_fs_read_text(const RetroFsPath *path, char **data,
                                 size_t *length, RetroFsVersion *version);
RetroFsError retro_fs_write_atomic(const RetroFsPath *path, const char *data,
                                    size_t length, const RetroFsVersion *expected,
                                    RetroFsVersion *written);

/* Creation and rename operations never replace an existing destination. */
RetroFsError retro_fs_create_file(const RetroFsPath *path);
RetroFsError retro_fs_create_directory(const RetroFsPath *path);
RetroFsError retro_fs_rename(const RetroFsPath *source,
                              const RetroFsPath *destination);

const char *retro_fs_error_string(RetroFsError error);

#endif
