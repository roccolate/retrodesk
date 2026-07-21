#ifndef RETRODESK_STORAGE_RETRO_FS_H
#define RETRODESK_STORAGE_RETRO_FS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__DJGPP__)
#include <stdlib.h>
#include <string.h>
#endif

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
   return this value, but must not infer platform semantics from volume_id or
   file_id. Zero initialization produces an invalid/no-version token. Kind,
   size, and modified_ns describe the observed object at capture time; only the
   storage adapter decides whether a later object is the same version. */
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

#if defined(_WIN32) || defined(__DJGPP__)

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4505)
#endif

/* Windows and DOS do not have a native storage adapter yet. Keep the API
   link-complete and fail every mutating/read operation explicitly instead of
   silently linking the POSIX implementation or leaving unresolved symbols.
   Directory listing returns an empty view whose path label states that storage
   is unavailable, allowing the desktop and editor to remain usable. */
#define RETRO_FS_UNAVAILABLE_PATH "[storage unavailable on Windows/DOS]"

static inline RetroFsError retro_fs_path_init(RetroFsPath *path,
                                               const char *value) {
    if (!path || !value || !value[0]) return RETRO_FS_INVALID_ARGUMENT;
    const char *label = RETRO_FS_UNAVAILABLE_PATH;
    size_t length = strlen(label);
    char *copy = malloc(length + 1);
    if (!copy) return RETRO_FS_OOM;
    memcpy(copy, label, length + 1);
    path->value = copy;
    return RETRO_FS_OK;
}

static inline void retro_fs_path_destroy(RetroFsPath *path) {
    if (!path) return;
    free(path->value);
    path->value = NULL;
}

static inline const char *retro_fs_path_cstr(const RetroFsPath *path) {
    return path && path->value ? path->value : RETRO_FS_UNAVAILABLE_PATH;
}

static inline RetroFsError retro_fs_path_parent(const RetroFsPath *path,
                                                 RetroFsPath *parent) {
    if (!path || !path->value || !parent) return RETRO_FS_INVALID_ARGUMENT;
    parent->value = NULL;
    return RETRO_FS_UNSUPPORTED;
}

static inline RetroFsError retro_fs_path_join(const RetroFsPath *base,
                                               const char *name,
                                               RetroFsPath *joined) {
    if (!base || !base->value || !name || !name[0] || !joined) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    joined->value = NULL;
    return RETRO_FS_UNSUPPORTED;
}

static inline RetroFsError retro_fs_stat(const RetroFsPath *path,
                                         RetroFsVersion *version) {
    if (!path || !path->value || !version) return RETRO_FS_INVALID_ARGUMENT;
    *version = (RetroFsVersion){0};
    return RETRO_FS_UNSUPPORTED;
}

static inline RetroFsError retro_fs_list(const RetroFsPath *path,
                                         size_t max_entries,
                                         RetroFsListFn callback,
                                         void *userdata) {
    if (!path || !path->value || !callback || max_entries == 0) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    (void)userdata;
    return RETRO_FS_OK;
}

static inline RetroFsError retro_fs_read_text(const RetroFsPath *path,
                                              char **data,
                                              size_t *length,
                                              RetroFsVersion *version) {
    if (!path || !path->value || !data || !length || !version) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    *data = NULL;
    *length = 0;
    *version = (RetroFsVersion){0};
    return RETRO_FS_UNSUPPORTED;
}

static inline RetroFsError retro_fs_write_atomic(
    const RetroFsPath *path, const char *data, size_t length,
    const RetroFsVersion *expected, RetroFsVersion *written) {
    if (!path || !path->value || (!data && length != 0)) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    (void)expected;
    if (written) *written = (RetroFsVersion){0};
    return RETRO_FS_UNSUPPORTED;
}

static inline RetroFsError retro_fs_create_file(const RetroFsPath *path) {
    return path && path->value ? RETRO_FS_UNSUPPORTED
                               : RETRO_FS_INVALID_ARGUMENT;
}

static inline RetroFsError retro_fs_create_directory(const RetroFsPath *path) {
    return path && path->value ? RETRO_FS_UNSUPPORTED
                               : RETRO_FS_INVALID_ARGUMENT;
}

static inline RetroFsError retro_fs_rename(const RetroFsPath *source,
                                           const RetroFsPath *destination) {
    return source && source->value && destination && destination->value
               ? RETRO_FS_UNSUPPORTED
               : RETRO_FS_INVALID_ARGUMENT;
}

static inline const char *retro_fs_error_string(RetroFsError error) {
    static const char *const messages[] = {
        "ok", "not found", "permission denied", "unsupported", "too large",
        "invalid text", "conflict", "out of memory", "I/O error",
        "invalid argument"
    };
    return error <= RETRO_FS_INVALID_ARGUMENT ? messages[error] : "unknown";
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#else

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

#endif
