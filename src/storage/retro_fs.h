#ifndef RETRODESK_STORAGE_RETRO_FS_H
#define RETRODESK_STORAGE_RETRO_FS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum RetroFsError {
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

typedef enum RetroFsKind {
    RETRO_FS_KIND_UNKNOWN = 0,
    RETRO_FS_KIND_REGULAR,
    RETRO_FS_KIND_DIRECTORY,
    RETRO_FS_KIND_SYMLINK,
    RETRO_FS_KIND_OTHER,
} RetroFsKind;

typedef struct RetroFsPath {
    char *value;
} RetroFsPath;

/* Adapter-neutral optimistic-concurrency token. Applications may retain and
   return this value, but must not infer platform semantics from the identity
   fields. Zero initialization produces an invalid/no-version token. */
typedef struct RetroFsVersion {
    bool valid;
    RetroFsKind kind;
    uint64_t volume_id;
    uint64_t file_id;
    uint64_t size;
    int64_t modified_ns;
} RetroFsVersion;

typedef struct RetroFsEntry {
    char *name;
    RetroFsKind kind;
    uint64_t size;
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
/* Text operations use validated UTF-8. They reject malformed sequences,
   embedded NUL/ESC, disallowed C0/C1 controls, and mixed LF/CRLF files. */
RetroFsError retro_fs_read_text(const RetroFsPath *path, char **data,
                                 size_t *length, RetroFsVersion *version);
RetroFsError retro_fs_write_atomic(const RetroFsPath *path, const char *data,
                                    size_t length,
                                    const RetroFsVersion *expected,
                                    RetroFsVersion *written);

/* Creation and rename operations never intentionally replace an existing
   destination. They return RETRO_FS_CONFLICT when the destination exists. */
RetroFsError retro_fs_create_file(const RetroFsPath *path);
RetroFsError retro_fs_create_directory(const RetroFsPath *path);
RetroFsError retro_fs_rename(const RetroFsPath *source,
                              const RetroFsPath *destination);

const char *retro_fs_error_string(RetroFsError error);

#endif
