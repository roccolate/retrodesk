#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "storage/retro_fs.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/syscall.h>
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE 1
#endif
#endif

#include "core/utf8.h"

#define RETRO_FS_MAX_TEXT (1024u * 1024u)

static RetroFsError map_errno(int e) {
    if (e == ENOENT || e == ENOTDIR) return RETRO_FS_NOT_FOUND;
    if (e == EACCES || e == EPERM) return RETRO_FS_PERMISSION;
    if (e == EEXIST || e == ENOTEMPTY) return RETRO_FS_CONFLICT;
#ifdef ELOOP
    if (e == ELOOP) return RETRO_FS_UNSUPPORTED;
#endif
    if (e == ENOMEM) return RETRO_FS_OOM;
    return RETRO_FS_IO;
}

static RetroFsKind kind_from_mode(mode_t mode) {
    if (S_ISREG(mode)) return RETRO_FS_KIND_REGULAR;
    if (S_ISDIR(mode)) return RETRO_FS_KIND_DIRECTORY;
    if (S_ISLNK(mode)) return RETRO_FS_KIND_SYMLINK;
    return RETRO_FS_KIND_OTHER;
}

static int64_t modified_time_ns(const struct stat *s) {
#if defined(__APPLE__)
    return (int64_t)s->st_mtimespec.tv_sec * INT64_C(1000000000) +
           (int64_t)s->st_mtimespec.tv_nsec;
#else
    return (int64_t)s->st_mtim.tv_sec * INT64_C(1000000000) +
           (int64_t)s->st_mtim.tv_nsec;
#endif
}

static void fill_version(const struct stat *s, RetroFsVersion *v) {
    if (!s || !v) return;
    *v = (RetroFsVersion){0};
    v->valid = true;
    v->kind = kind_from_mode(s->st_mode);
    v->volume_id = (uint64_t)s->st_dev;
    v->file_id = (uint64_t)s->st_ino;
    v->size = s->st_size > 0 ? (uint64_t)s->st_size : 0;
    v->modified_ns = modified_time_ns(s);
}

static bool same_version(const RetroFsVersion *a, const RetroFsVersion *b) {
    return a && b && a->valid && b->valid &&
           a->kind == b->kind &&
           a->volume_id == b->volume_id &&
           a->file_id == b->file_id &&
           a->size == b->size &&
           a->modified_ns == b->modified_ns;
}

static bool same_stat_identity(const struct stat *a, const struct stat *b) {
    return a && b && a->st_dev == b->st_dev && a->st_ino == b->st_ino &&
           kind_from_mode(a->st_mode) == kind_from_mode(b->st_mode);
}

RetroFsError retro_fs_path_init(RetroFsPath *p, const char *v) {
    if (!p || !v || !*v) return RETRO_FS_INVALID_ARGUMENT;
    p->value = strdup(v);
    return p->value ? RETRO_FS_OK : RETRO_FS_OOM;
}

void retro_fs_path_destroy(RetroFsPath *p) {
    if (p) {
        free(p->value);
        p->value = NULL;
    }
}

const char *retro_fs_path_cstr(const RetroFsPath *p) {
    return p ? p->value : NULL;
}

RetroFsError retro_fs_path_parent(const RetroFsPath *p, RetroFsPath *out) {
    if (!p || !p->value || !out) return RETRO_FS_INVALID_ARGUMENT;
    char *copy = strdup(p->value);
    if (!copy) return RETRO_FS_OOM;
    char *slash = strrchr(copy, '/');
    if (!slash) {
        free(copy);
        return retro_fs_path_init(out, ".");
    }
    while (slash > copy + 1 && slash[-1] == '/') --slash;
    *slash = '\0';
    if (!*copy) {
        copy[0] = '/';
        copy[1] = '\0';
    }
    out->value = copy;
    return RETRO_FS_OK;
}

RetroFsError retro_fs_path_join(const RetroFsPath *b, const char *name,
                                 RetroFsPath *o) {
    if (!b || !b->value || !name || !name[0] || !o || strchr(name, '/')) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    size_t n = strlen(b->value);
    size_t m = strlen(name);
    if (n > SIZE_MAX - m - 2) return RETRO_FS_TOO_LARGE;
    char *s = malloc(n + m + 2);
    if (!s) return RETRO_FS_OOM;
    memcpy(s, b->value, n);
    s[n] = '/';
    memcpy(s + n + 1, name, m + 1);
    o->value = s;
    return RETRO_FS_OK;
}

RetroFsError retro_fs_stat(const RetroFsPath *p, RetroFsVersion *v) {
    struct stat s;
    if (!p || !p->value || !v) return RETRO_FS_INVALID_ARGUMENT;
    if (lstat(p->value, &s) < 0) return map_errno(errno);
    fill_version(&s, v);
    return RETRO_FS_OK;
}

static int entry_compare(const void *lhs, const void *rhs) {
    const RetroFsEntry *a = lhs;
    const RetroFsEntry *b = rhs;
    int ak = a->kind == RETRO_FS_KIND_DIRECTORY
                 ? 0
                 : (a->kind == RETRO_FS_KIND_REGULAR ? 1 : 2);
    int bk = b->kind == RETRO_FS_KIND_DIRECTORY
                 ? 0
                 : (b->kind == RETRO_FS_KIND_REGULAR ? 1 : 2);
    if (ak != bk) return ak - bk;
    return strcmp(a->name, b->name);
}

static void free_entries(RetroFsEntry *entries, size_t count) {
    if (!entries) return;
    for (size_t i = 0; i < count; ++i) free(entries[i].name);
    free(entries);
}

static bool reserve_entries(RetroFsEntry **entries, size_t *capacity,
                            size_t want) {
    if (!entries || !capacity) return false;
    if (want <= *capacity) return true;
    if (want > SIZE_MAX / sizeof(**entries)) return false;

    size_t next = *capacity ? *capacity : 64;
    while (next < want) {
        if (next > SIZE_MAX / 2) {
            next = want;
            break;
        }
        next *= 2;
    }

    RetroFsEntry *grown = realloc(*entries, next * sizeof(*grown));
    if (!grown) return false;
    *entries = grown;
    *capacity = next;
    return true;
}

RetroFsError retro_fs_list(const RetroFsPath *p, size_t max_entries,
                            RetroFsListFn cb, void *u) {
    if (!p || !p->value || !cb || max_entries == 0) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    DIR *d = opendir(p->value);
    if (!d) return map_errno(errno);
    RetroFsEntry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    RetroFsError result = RETRO_FS_OK;
    struct dirent *de;
    errno = 0;
    while ((de = readdir(d)) != NULL) {
        if (!strcmp(de->d_name, ".")) continue;
        if (count == max_entries) {
            result = RETRO_FS_TOO_LARGE;
            break;
        }
        RetroFsPath child = {0};
        result = retro_fs_path_join(p, de->d_name, &child);
        if (result != RETRO_FS_OK) break;
        struct stat s;
        if (lstat(child.value, &s) < 0) {
            result = map_errno(errno);
            retro_fs_path_destroy(&child);
            break;
        }
        RetroFsEntry entry = {0};
        entry.name = strdup(de->d_name);
        retro_fs_path_destroy(&child);
        if (!entry.name) {
            result = RETRO_FS_OOM;
            break;
        }
        fill_version(&s, &entry.version);
        entry.kind = entry.version.kind;
        entry.size = entry.version.size;
        if (!reserve_entries(&entries, &capacity, count + 1)) {
            free(entry.name);
            result = RETRO_FS_OOM;
            break;
        }
        entries[count++] = entry;
        errno = 0;
    }
    if (result == RETRO_FS_OK && errno != 0) result = map_errno(errno);
    if (closedir(d) < 0 && result == RETRO_FS_OK) result = map_errno(errno);
    if (result != RETRO_FS_OK) {
        free_entries(entries, count);
        return result;
    }
    if (count > 1) qsort(entries, count, sizeof(*entries), entry_compare);
    for (size_t i = 0; i < count; ++i) {
        if (!cb(&entries[i], u)) break;
    }
    free_entries(entries, count);
    return RETRO_FS_OK;
}

static bool valid_text_content(const char *data, size_t length) {
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

RetroFsError retro_fs_read_text(const RetroFsPath *p, char **out, size_t *len,
                                 RetroFsVersion *v) {
    if (!p || !p->value || !out || !len) return RETRO_FS_INVALID_ARGUMENT;
    *out = NULL;
    *len = 0;

    struct stat before;
    if (lstat(p->value, &before) < 0) return map_errno(errno);
    if (!S_ISREG(before.st_mode) || S_ISLNK(before.st_mode)) {
        return RETRO_FS_UNSUPPORTED;
    }

    int flags = O_RDONLY;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    int fd = open(p->value, flags);
    if (fd < 0) return map_errno(errno);

    struct stat opened;
    if (fstat(fd, &opened) < 0) {
        int e = errno;
        close(fd);
        return map_errno(e);
    }
    if (!S_ISREG(opened.st_mode) || !same_stat_identity(&before, &opened)) {
        close(fd);
        return RETRO_FS_CONFLICT;
    }
    if (opened.st_size < 0 || (uintmax_t)opened.st_size > RETRO_FS_MAX_TEXT) {
        close(fd);
        return RETRO_FS_TOO_LARGE;
    }

    size_t expected = (size_t)opened.st_size;
    char *buf = malloc(expected + 1);
    if (!buf) {
        close(fd);
        return RETRO_FS_OOM;
    }

    size_t got = 0;
    while (got < expected) {
        ssize_t amount = read(fd, buf + got, expected - got);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) {
            free(buf);
            close(fd);
            return RETRO_FS_IO;
        }
        got += (size_t)amount;
    }
    if (close(fd) < 0) {
        free(buf);
        return RETRO_FS_IO;
    }

    buf[got] = '\0';
    if (!valid_text_content(buf, got)) {
        free(buf);
        return RETRO_FS_INVALID_TEXT;
    }
    *out = buf;
    *len = got;
    if (v) fill_version(&opened, v);
    return RETRO_FS_OK;
}

static void cleanup_temporary(const char *path, int fd) {
    if (fd >= 0) (void)close(fd);
    if (path) (void)unlink(path);
}

static RetroFsError current_version(const char *path, bool *exists,
                                    struct stat *metadata,
                                    RetroFsVersion *version) {
    if (!path || !exists || !metadata) return RETRO_FS_INVALID_ARGUMENT;
    errno = 0;
    *exists = lstat(path, metadata) == 0;
    if (!*exists) {
        if (errno == ENOENT) {
            if (version) *version = (RetroFsVersion){0};
            return RETRO_FS_OK;
        }
        return map_errno(errno);
    }
    if (!S_ISREG(metadata->st_mode) || S_ISLNK(metadata->st_mode)) {
        return RETRO_FS_UNSUPPORTED;
    }
    if (version) fill_version(metadata, version);
    return RETRO_FS_OK;
}

static int rename_without_replace(const char *source, const char *destination) {
#if defined(__linux__) && defined(SYS_renameat2)
    return (int)syscall(SYS_renameat2, AT_FDCWD, source,
                        AT_FDCWD, destination, RENAME_NOREPLACE);
#else
    (void)source;
    (void)destination;
    errno = ENOSYS;
    return -1;
#endif
}

RetroFsError retro_fs_write_atomic(const RetroFsPath *p, const char *data, size_t n,
                                    const RetroFsVersion *exp,
                                    RetroFsVersion *written) {
    if (!p || !p->value || (!data && n)) return RETRO_FS_INVALID_ARGUMENT;
    if (n > RETRO_FS_MAX_TEXT) return RETRO_FS_TOO_LARGE;
    if (!valid_text_content(data, n)) return RETRO_FS_INVALID_TEXT;

    bool existed = false;
    struct stat original = {0};
    RetroFsVersion original_version = {0};
    RetroFsError status = current_version(
        p->value, &existed, &original, &original_version);
    if (status != RETRO_FS_OK) return status;
    if (existed && !exp) return RETRO_FS_CONFLICT;
    if (exp && (!existed || !same_version(exp, &original_version))) {
        return RETRO_FS_CONFLICT;
    }

    RetroFsPath parent = {0};
    status = retro_fs_path_parent(p, &parent);
    if (status != RETRO_FS_OK) return status;
    size_t parent_length = strlen(parent.value);
    if (parent_length > SIZE_MAX - 32) {
        retro_fs_path_destroy(&parent);
        return RETRO_FS_TOO_LARGE;
    }

    size_t temporary_size = parent_length + 32;
    char *temporary = malloc(temporary_size);
    if (!temporary) {
        retro_fs_path_destroy(&parent);
        return RETRO_FS_OOM;
    }
    snprintf(temporary, temporary_size,
             "%s/.retro-tmp-XXXXXX", parent.value);

    int fd = mkstemp(temporary);
    if (fd < 0) {
        int e = errno;
        free(temporary);
        retro_fs_path_destroy(&parent);
        return map_errno(e);
    }

    mode_t mode = existed ? original.st_mode & 07777 : 0666;
    if (fchmod(fd, mode) < 0) {
        cleanup_temporary(temporary, fd);
        free(temporary);
        retro_fs_path_destroy(&parent);
        return map_errno(errno);
    }

    size_t offset = 0;
    while (offset < n) {
        ssize_t amount = write(fd, data + offset, n - offset);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) {
            cleanup_temporary(temporary, fd);
            free(temporary);
            retro_fs_path_destroy(&parent);
            return RETRO_FS_IO;
        }
        offset += (size_t)amount;
    }
    if (fsync(fd) < 0) {
        cleanup_temporary(temporary, fd);
        free(temporary);
        retro_fs_path_destroy(&parent);
        return RETRO_FS_IO;
    }
    if (close(fd) < 0) {
        fd = -1;
        (void)unlink(temporary);
        free(temporary);
        retro_fs_path_destroy(&parent);
        return RETRO_FS_IO;
    }
    fd = -1;

    bool still_exists = false;
    struct stat current = {0};
    RetroFsVersion current_token = {0};
    status = current_version(
        p->value, &still_exists, &current, &current_token);
    if (status != RETRO_FS_OK || still_exists != existed ||
        (existed && !same_version(&original_version, &current_token))) {
        (void)unlink(temporary);
        free(temporary);
        retro_fs_path_destroy(&parent);
        return status == RETRO_FS_OK ? RETRO_FS_CONFLICT : status;
    }

    int rename_result;
    if (existed) {
        rename_result = rename(temporary, p->value);
    } else {
        rename_result = rename_without_replace(temporary, p->value);
        if (rename_result < 0 && (errno == ENOSYS || errno == EINVAL)) {
            struct stat destination;
            if (lstat(p->value, &destination) == 0) {
                errno = EEXIST;
                rename_result = -1;
            } else if (errno == ENOENT) {
                rename_result = rename(temporary, p->value);
            }
        }
    }
    if (rename_result < 0) {
        int e = errno;
        (void)unlink(temporary);
        free(temporary);
        retro_fs_path_destroy(&parent);
        return map_errno(e);
    }

    int directory_flags = O_RDONLY;
#ifdef O_DIRECTORY
    directory_flags |= O_DIRECTORY;
#endif
    int directory_fd = open(parent.value, directory_flags);
    if (directory_fd >= 0) {
        (void)fsync(directory_fd);
        (void)close(directory_fd);
    }

    free(temporary);
    retro_fs_path_destroy(&parent);
    if (written) {
        RetroFsError stat_result = retro_fs_stat(p, written);
        if (stat_result != RETRO_FS_OK) *written = (RetroFsVersion){0};
    }
    return RETRO_FS_OK;
}

RetroFsError retro_fs_create_file(const RetroFsPath *path) {
    if (!path || !path->value || !path->value[0]) return RETRO_FS_INVALID_ARGUMENT;
    int fd = open(path->value, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0) return map_errno(errno);
    if (fsync(fd) < 0 || close(fd) < 0) {
        int e = errno;
        (void)unlink(path->value);
        return map_errno(e);
    }
    return RETRO_FS_OK;
}

RetroFsError retro_fs_create_directory(const RetroFsPath *path) {
    if (!path || !path->value || !path->value[0]) return RETRO_FS_INVALID_ARGUMENT;
    if (mkdir(path->value, 0777) < 0) return map_errno(errno);
    return RETRO_FS_OK;
}

RetroFsError retro_fs_rename(const RetroFsPath *source,
                              const RetroFsPath *destination) {
    if (!source || !source->value || !source->value[0] || !destination ||
        !destination->value || !destination->value[0]) {
        return RETRO_FS_INVALID_ARGUMENT;
    }

    if (rename_without_replace(source->value, destination->value) == 0) {
        return RETRO_FS_OK;
    }
    int native_error = errno;
    if (native_error != ENOSYS && native_error != EINVAL) {
        return map_errno(native_error);
    }

    struct stat source_metadata;
    if (lstat(source->value, &source_metadata) < 0) return map_errno(errno);
    if (S_ISREG(source_metadata.st_mode)) {
        if (link(source->value, destination->value) < 0) return map_errno(errno);
        if (unlink(source->value) < 0) {
            int e = errno;
            (void)unlink(destination->value);
            return map_errno(e);
        }
        return RETRO_FS_OK;
    }

    struct stat existing;
    if (lstat(destination->value, &existing) == 0) return RETRO_FS_CONFLICT;
    if (errno != ENOENT) return map_errno(errno);
    if (rename(source->value, destination->value) < 0) return map_errno(errno);
    return RETRO_FS_OK;
}

const char *retro_fs_error_string(RetroFsError e) {
    static const char *s[] = {
        "ok", "not found", "permission denied", "unsupported", "too large",
        "invalid text", "conflict", "out of memory", "I/O error",
        "invalid argument"
    };
    return e <= RETRO_FS_INVALID_ARGUMENT ? s[e] : "unknown";
}
