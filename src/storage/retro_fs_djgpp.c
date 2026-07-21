#include "storage/retro_fs.h"

#if defined(__DJGPP__)

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "core/utf8.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define RETRO_FS_MAX_TEXT (1024u * 1024u)
#define RETRO_FS_TEMP_ATTEMPTS 256u
#define RETRO_FS_FNV_OFFSET UINT64_C(1469598103934665603)
#define RETRO_FS_FNV_PRIME UINT64_C(1099511628211)

static RetroFsError map_errno(int error) {
    if (error == ENOENT || error == ENOTDIR) return RETRO_FS_NOT_FOUND;
    if (error == EACCES || error == EPERM) return RETRO_FS_PERMISSION;
#ifdef EROFS
    if (error == EROFS) return RETRO_FS_PERMISSION;
#endif
    if (error == EEXIST || error == ENOTEMPTY) return RETRO_FS_CONFLICT;
    if (error == ENOMEM) return RETRO_FS_OOM;
#ifdef ENAMETOOLONG
    if (error == ENAMETOOLONG) return RETRO_FS_TOO_LARGE;
#endif
#ifdef EFBIG
    if (error == EFBIG) return RETRO_FS_TOO_LARGE;
#endif
#ifdef ENOSYS
    if (error == ENOSYS) return RETRO_FS_UNSUPPORTED;
#endif
    return RETRO_FS_IO;
}

static char *duplicate_string(const char *value) {
    if (!value) return NULL;
    size_t length = strlen(value);
    if (length == SIZE_MAX) return NULL;
    char *copy = malloc(length + 1);
    if (!copy) return NULL;
    memcpy(copy, value, length + 1);
    return copy;
}

static bool is_separator(char value) {
    return value == '/' || value == '\\';
}

static bool is_ascii_alpha(char value) {
    unsigned char byte = (unsigned char)value;
    return (byte >= (unsigned char)'A' && byte <= (unsigned char)'Z') ||
           (byte >= (unsigned char)'a' && byte <= (unsigned char)'z');
}

static bool valid_dos_path(const char *value) {
    if (!value || !value[0]) return false;
    for (size_t index = 0; value[index] != '\0'; ++index) {
        unsigned char byte = (unsigned char)value[index];
        if (byte >= 0x80u || byte < 0x20u || byte == 0x7Fu) return false;
        if (byte == '"' || byte == '<' || byte == '>' || byte == '|' ||
            byte == '?' || byte == '*') {
            return false;
        }
        if (byte == ':' && !(index == 1 && is_ascii_alpha(value[0]))) {
            return false;
        }
    }
    return true;
}

static bool valid_dos_component(const char *name) {
    if (!name || !name[0] || !valid_dos_path(name)) return false;
    if (strchr(name, '/') || strchr(name, '\\') || strchr(name, ':')) return false;
    size_t length = strlen(name);
    if (length == 0) return false;
    if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0 &&
        (name[length - 1] == '.' || name[length - 1] == ' ')) {
        return false;
    }
    return true;
}

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t length) {
    const unsigned char *bytes = data;
    for (size_t index = 0; index < length; ++index) {
        hash ^= (uint64_t)bytes[index];
        hash *= RETRO_FS_FNV_PRIME;
    }
    return hash;
}

static uint64_t hash_path(const char *path) {
    uint64_t hash = RETRO_FS_FNV_OFFSET;
    if (!path) return hash;
    for (size_t index = 0; path[index] != '\0'; ++index) {
        unsigned char byte = (unsigned char)path[index];
        if (byte == '\\') byte = '/';
        if (byte >= (unsigned char)'a' && byte <= (unsigned char)'z') {
            byte = (unsigned char)(byte - (unsigned char)'a' + (unsigned char)'A');
        }
        hash = hash_bytes(hash, &byte, 1);
    }
    return hash;
}

static uint64_t volume_identity(const char *path, const struct stat *metadata) {
    uint64_t identity = metadata ? (uint64_t)metadata->st_dev : 0;
    if (path && is_ascii_alpha(path[0]) && path[1] == ':') {
        unsigned char drive = (unsigned char)toupper((unsigned char)path[0]);
        identity ^= (uint64_t)(drive - (unsigned char)'A' + 1u) << 32;
    }
    return identity;
}

static RetroFsKind kind_from_mode(mode_t mode) {
    if (S_ISREG(mode)) return RETRO_FS_KIND_REGULAR;
    if (S_ISDIR(mode)) return RETRO_FS_KIND_DIRECTORY;
    return RETRO_FS_KIND_OTHER;
}

static int64_t modified_time_ns(const struct stat *metadata) {
    if (!metadata) return 0;
    if ((int64_t)metadata->st_mtime > INT64_MAX / INT64_C(1000000000)) {
        return INT64_MAX;
    }
    return (int64_t)metadata->st_mtime * INT64_C(1000000000);
}

static RetroFsError hash_regular_file(const char *path, uint64_t *hash_out) {
    if (!path || !hash_out) return RETRO_FS_INVALID_ARGUMENT;
    int descriptor = open(path, O_RDONLY | O_BINARY);
    if (descriptor < 0) return map_errno(errno);
    uint64_t hash = RETRO_FS_FNV_OFFSET;
    unsigned char buffer[4096];
    for (;;) {
        ssize_t amount = read(descriptor, buffer, sizeof(buffer));
        if (amount < 0 && errno == EINTR) continue;
        if (amount < 0) {
            int error = errno;
            (void)close(descriptor);
            return map_errno(error);
        }
        if (amount == 0) break;
        hash = hash_bytes(hash, buffer, (size_t)amount);
    }
    if (close(descriptor) < 0) return map_errno(errno);
    *hash_out = hash;
    return RETRO_FS_OK;
}

static RetroFsError fill_version(const char *path, const struct stat *metadata,
                                 RetroFsVersion *version) {
    if (!path || !metadata || !version) return RETRO_FS_INVALID_ARGUMENT;
    RetroFsVersion result = {0};
    result.valid = true;
    result.kind = kind_from_mode(metadata->st_mode);
    result.volume_id = volume_identity(path, metadata);
    result.file_id = hash_path(path);
    result.size = metadata->st_size > 0 ? (uint64_t)metadata->st_size : 0;
    result.modified_ns = modified_time_ns(metadata);
    if (result.kind == RETRO_FS_KIND_REGULAR &&
        result.size <= RETRO_FS_MAX_TEXT) {
        uint64_t content_hash = 0;
        RetroFsError status = hash_regular_file(path, &content_hash);
        if (status != RETRO_FS_OK) return status;
        result.file_id ^= content_hash + UINT64_C(0x9e3779b97f4a7c15) +
                          (result.file_id << 6) + (result.file_id >> 2);
    }
    *version = result;
    return RETRO_FS_OK;
}

static void fill_version_from_buffer(const char *path,
                                     const struct stat *metadata,
                                     const char *data, size_t length,
                                     RetroFsVersion *version) {
    if (!path || !metadata || !version) return;
    uint64_t path_hash = hash_path(path);
    uint64_t content_hash = hash_bytes(RETRO_FS_FNV_OFFSET, data, length);
    uint64_t file_id = path_hash ^
        (content_hash + UINT64_C(0x9e3779b97f4a7c15) +
         (path_hash << 6) + (path_hash >> 2));
    *version = (RetroFsVersion){
        .valid = true,
        .kind = kind_from_mode(metadata->st_mode),
        .volume_id = volume_identity(path, metadata),
        .file_id = file_id,
        .size = metadata->st_size > 0 ? (uint64_t)metadata->st_size : 0,
        .modified_ns = modified_time_ns(metadata),
    };
}

static bool same_version(const RetroFsVersion *left,
                         const RetroFsVersion *right) {
    return left && right && left->valid && right->valid &&
           left->kind == right->kind &&
           left->volume_id == right->volume_id &&
           left->file_id == right->file_id &&
           left->size == right->size &&
           left->modified_ns == right->modified_ns;
}

static bool same_opened_object(const struct stat *before,
                               const struct stat *opened) {
    if (!before || !opened || kind_from_mode(before->st_mode) !=
                                  kind_from_mode(opened->st_mode)) {
        return false;
    }
    if ((before->st_ino != 0 || opened->st_ino != 0) &&
        (before->st_dev != opened->st_dev || before->st_ino != opened->st_ino)) {
        return false;
    }
    return before->st_size == opened->st_size &&
           before->st_mtime == opened->st_mtime;
}

RetroFsError retro_fs_path_init(RetroFsPath *path, const char *value) {
    if (!path || !valid_dos_path(value)) return RETRO_FS_INVALID_ARGUMENT;
    path->value = duplicate_string(value);
    return path->value ? RETRO_FS_OK : RETRO_FS_OOM;
}

void retro_fs_path_destroy(RetroFsPath *path) {
    if (!path) return;
    free(path->value);
    path->value = NULL;
}

const char *retro_fs_path_cstr(const RetroFsPath *path) {
    return path ? path->value : NULL;
}

RetroFsError retro_fs_path_parent(const RetroFsPath *path,
                                  RetroFsPath *parent) {
    if (!path || !path->value || !parent) return RETRO_FS_INVALID_ARGUMENT;
    char *copy = duplicate_string(path->value);
    if (!copy) return RETRO_FS_OOM;
    size_t length = strlen(copy);
    while (length > 1 && is_separator(copy[length - 1])) {
        if (length == 3 && copy[1] == ':') break;
        copy[--length] = '\0';
    }
    char *last = NULL;
    for (char *cursor = copy; *cursor; ++cursor) {
        if (is_separator(*cursor)) last = cursor;
    }
    if (!last) {
        free(copy);
        return retro_fs_path_init(parent, ".");
    }
    if (last == copy) {
        copy[1] = '\0';
    } else if (last == copy + 2 && copy[1] == ':') {
        copy[3] = '\0';
    } else {
        while (last > copy && is_separator(last[-1])) --last;
        *last = '\0';
    }
    parent->value = copy;
    return RETRO_FS_OK;
}

RetroFsError retro_fs_path_join(const RetroFsPath *base, const char *name,
                                RetroFsPath *joined) {
    if (!base || !base->value || !joined || !valid_dos_component(name)) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    size_t base_length = strlen(base->value);
    size_t name_length = strlen(name);
    bool has_separator = base_length > 0 &&
                         is_separator(base->value[base_length - 1]);
    size_t extra = has_separator ? 1 : 2;
    if (name_length > SIZE_MAX - extra ||
        base_length > SIZE_MAX - name_length - extra) {
        return RETRO_FS_TOO_LARGE;
    }
    char *value = malloc(base_length + name_length + extra);
    if (!value) return RETRO_FS_OOM;
    memcpy(value, base->value, base_length);
    size_t offset = base_length;
    if (!has_separator) value[offset++] = '/';
    memcpy(value + offset, name, name_length + 1);
    joined->value = value;
    return RETRO_FS_OK;
}

RetroFsError retro_fs_stat(const RetroFsPath *path, RetroFsVersion *version) {
    if (!path || !path->value || !version) return RETRO_FS_INVALID_ARGUMENT;
    struct stat metadata;
    if (stat(path->value, &metadata) < 0) return map_errno(errno);
    return fill_version(path->value, &metadata, version);
}

static int ascii_case_compare(const char *left, const char *right) {
    while (*left && *right) {
        unsigned char a = (unsigned char)*left++;
        unsigned char b = (unsigned char)*right++;
        if (a >= 'a' && a <= 'z') a = (unsigned char)(a - 'a' + 'A');
        if (b >= 'a' && b <= 'z') b = (unsigned char)(b - 'a' + 'A');
        if (a != b) return a < b ? -1 : 1;
    }
    if (*left) return 1;
    if (*right) return -1;
    return 0;
}

static int entry_compare(const void *left, const void *right) {
    const RetroFsEntry *a = left;
    const RetroFsEntry *b = right;
    int a_rank = a->kind == RETRO_FS_KIND_DIRECTORY
                     ? 0
                     : (a->kind == RETRO_FS_KIND_REGULAR ? 1 : 2);
    int b_rank = b->kind == RETRO_FS_KIND_DIRECTORY
                     ? 0
                     : (b->kind == RETRO_FS_KIND_REGULAR ? 1 : 2);
    if (a_rank != b_rank) return a_rank - b_rank;
    return ascii_case_compare(a->name, b->name);
}

static void free_entries(RetroFsEntry *entries, size_t count) {
    if (!entries) return;
    for (size_t index = 0; index < count; ++index) free(entries[index].name);
    free(entries);
}

static bool reserve_entries(RetroFsEntry **entries, size_t *capacity,
                            size_t wanted) {
    if (!entries || !capacity) return false;
    if (wanted <= *capacity) return true;
    size_t next = *capacity ? *capacity : 32;
    while (next < wanted) {
        if (next > SIZE_MAX / 2) {
            next = wanted;
            break;
        }
        next *= 2;
    }
    if (next > SIZE_MAX / sizeof(**entries)) return false;
    RetroFsEntry *grown = realloc(*entries, next * sizeof(*grown));
    if (!grown) return false;
    *entries = grown;
    *capacity = next;
    return true;
}

RetroFsError retro_fs_list(const RetroFsPath *path, size_t max_entries,
                           RetroFsListFn callback, void *userdata) {
    if (!path || !path->value || !callback || max_entries == 0) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    DIR *directory = opendir(path->value);
    if (!directory) return map_errno(errno);
    RetroFsEntry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    RetroFsError status = RETRO_FS_OK;
    errno = 0;
    struct dirent *native_entry = NULL;
    while ((native_entry = readdir(directory)) != NULL) {
        if (strcmp(native_entry->d_name, ".") == 0) continue;
        if (!valid_dos_component(native_entry->d_name)) {
            status = RETRO_FS_UNSUPPORTED;
            break;
        }
        if (count == max_entries) {
            status = RETRO_FS_TOO_LARGE;
            break;
        }
        RetroFsPath child = {0};
        status = retro_fs_path_join(path, native_entry->d_name, &child);
        if (status != RETRO_FS_OK) break;
        struct stat metadata;
        if (stat(child.value, &metadata) < 0) {
            status = map_errno(errno);
            retro_fs_path_destroy(&child);
            break;
        }
        RetroFsEntry entry = {0};
        entry.name = duplicate_string(native_entry->d_name);
        if (!entry.name) {
            retro_fs_path_destroy(&child);
            status = RETRO_FS_OOM;
            break;
        }
        status = fill_version(child.value, &metadata, &entry.version);
        retro_fs_path_destroy(&child);
        if (status != RETRO_FS_OK) {
            free(entry.name);
            break;
        }
        entry.kind = entry.version.kind;
        entry.size = entry.version.size;
        if (!reserve_entries(&entries, &capacity, count + 1)) {
            free(entry.name);
            status = RETRO_FS_OOM;
            break;
        }
        entries[count++] = entry;
        errno = 0;
    }
    if (status == RETRO_FS_OK && errno != 0) status = map_errno(errno);
    if (closedir(directory) < 0 && status == RETRO_FS_OK) {
        status = map_errno(errno);
    }
    if (status != RETRO_FS_OK) {
        free_entries(entries, count);
        return status;
    }
    if (count > 1) qsort(entries, count, sizeof(*entries), entry_compare);
    for (size_t index = 0; index < count; ++index) {
        if (!callback(&entries[index], userdata)) break;
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
        size_t byte_length = 0;
        if (!retro_utf8_decode(data, length, offset,
                               &codepoint, &byte_length)) {
            return false;
        }
        if (codepoint == 0 || codepoint == 27 ||
            (codepoint < 32 && codepoint != '\t' && codepoint != '\n' &&
             codepoint != '\r') ||
            (codepoint >= 0x7Fu && codepoint < 0xA0u)) {
            return false;
        }
        if (codepoint == '\r') {
            if (offset + 1 >= length || data[offset + 1] != '\n') return false;
            saw_crlf = true;
        } else if (codepoint == '\n') {
            if (offset == 0 || data[offset - 1] != '\r') saw_lf = true;
        }
        offset += byte_length;
    }
    return !(saw_lf && saw_crlf);
}

RetroFsError retro_fs_read_text(const RetroFsPath *path, char **data,
                                size_t *length, RetroFsVersion *version) {
    if (!path || !path->value || !data || !length) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    *data = NULL;
    *length = 0;
    struct stat before;
    if (stat(path->value, &before) < 0) return map_errno(errno);
    if (!S_ISREG(before.st_mode)) return RETRO_FS_UNSUPPORTED;
    if (before.st_size < 0 || (uintmax_t)before.st_size > RETRO_FS_MAX_TEXT) {
        return RETRO_FS_TOO_LARGE;
    }
    int descriptor = open(path->value, O_RDONLY | O_BINARY);
    if (descriptor < 0) return map_errno(errno);
    struct stat opened;
    if (fstat(descriptor, &opened) < 0) {
        int error = errno;
        (void)close(descriptor);
        return map_errno(error);
    }
    if (!S_ISREG(opened.st_mode) || !same_opened_object(&before, &opened)) {
        (void)close(descriptor);
        return RETRO_FS_CONFLICT;
    }
    size_t expected = (size_t)opened.st_size;
    char *buffer = malloc(expected + 1);
    if (!buffer) {
        (void)close(descriptor);
        return RETRO_FS_OOM;
    }
    size_t received = 0;
    while (received < expected) {
        ssize_t amount = read(descriptor, buffer + received, expected - received);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) {
            free(buffer);
            (void)close(descriptor);
            return RETRO_FS_IO;
        }
        received += (size_t)amount;
    }
    struct stat after;
    if (fstat(descriptor, &after) < 0 || !same_opened_object(&opened, &after)) {
        free(buffer);
        (void)close(descriptor);
        return RETRO_FS_CONFLICT;
    }
    if (close(descriptor) < 0) {
        free(buffer);
        return map_errno(errno);
    }
    buffer[received] = '\0';
    if (!valid_text_content(buffer, received)) {
        free(buffer);
        return RETRO_FS_INVALID_TEXT;
    }
    *data = buffer;
    *length = received;
    if (version) fill_version_from_buffer(path->value, &after, buffer, received, version);
    return RETRO_FS_OK;
}

static bool flush_descriptor(int descriptor) {
    if (fsync(descriptor) == 0) return true;
#ifdef ENOSYS
    if (errno == ENOSYS) return true;
#endif
    if (errno == EINVAL) return true;
    return false;
}

static void cleanup_temporary(const char *path, int descriptor) {
    if (descriptor >= 0) (void)close(descriptor);
    if (path) (void)unlink(path);
}

static RetroFsError current_version(const char *path, bool *exists,
                                    struct stat *metadata,
                                    RetroFsVersion *version) {
    if (!path || !exists || !metadata) return RETRO_FS_INVALID_ARGUMENT;
    errno = 0;
    *exists = stat(path, metadata) == 0;
    if (!*exists) {
        if (errno == ENOENT) {
            if (version) *version = (RetroFsVersion){0};
            return RETRO_FS_OK;
        }
        return map_errno(errno);
    }
    if (!S_ISREG(metadata->st_mode)) return RETRO_FS_UNSUPPORTED;
    return version ? fill_version(path, metadata, version) : RETRO_FS_OK;
}

static RetroFsError create_unique_path(const RetroFsPath *parent,
                                       const char *prefix,
                                       char **path_out, int *descriptor_out) {
    if (!parent || !parent->value || !prefix || !path_out) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    unsigned long seed = (unsigned long)getpid() ^ (unsigned long)clock();
    for (unsigned int attempt = 0; attempt < RETRO_FS_TEMP_ATTEMPTS; ++attempt) {
        char name[13];
        unsigned long value = (seed + attempt * 977u) & 0xFFFFFu;
        int count = snprintf(name, sizeof(name), "%s%05lX.TMP", prefix, value);
        if (count <= 0 || (size_t)count >= sizeof(name)) return RETRO_FS_IO;
        RetroFsPath candidate = {0};
        RetroFsError status = retro_fs_path_join(parent, name, &candidate);
        if (status != RETRO_FS_OK) return status;
        if (!descriptor_out) {
            struct stat existing;
            if (stat(candidate.value, &existing) < 0 && errno == ENOENT) {
                *path_out = candidate.value;
                return RETRO_FS_OK;
            }
            retro_fs_path_destroy(&candidate);
            continue;
        }
        int descriptor = open(candidate.value,
                              O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0600);
        if (descriptor >= 0) {
            *path_out = candidate.value;
            *descriptor_out = descriptor;
            return RETRO_FS_OK;
        }
        int error = errno;
        retro_fs_path_destroy(&candidate);
        if (error != EEXIST && error != EACCES) return map_errno(error);
    }
    return RETRO_FS_CONFLICT;
}

RetroFsError retro_fs_write_atomic(const RetroFsPath *path, const char *data,
                                   size_t length,
                                   const RetroFsVersion *expected,
                                   RetroFsVersion *written) {
    if (!path || !path->value || (!data && length != 0)) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    if (length > RETRO_FS_MAX_TEXT) return RETRO_FS_TOO_LARGE;
    if (!valid_text_content(data, length)) return RETRO_FS_INVALID_TEXT;

    bool existed = false;
    struct stat original = {0};
    RetroFsVersion original_version = {0};
    RetroFsError status = current_version(path->value, &existed, &original,
                                          &original_version);
    if (status != RETRO_FS_OK) return status;
    if (existed && !expected) return RETRO_FS_CONFLICT;
    if (expected && (!existed || !same_version(expected, &original_version))) {
        return RETRO_FS_CONFLICT;
    }

    RetroFsPath parent = {0};
    status = retro_fs_path_parent(path, &parent);
    if (status != RETRO_FS_OK) return status;
    char *temporary = NULL;
    int descriptor = -1;
    status = create_unique_path(&parent, "RT", &temporary, &descriptor);
    if (status != RETRO_FS_OK) {
        retro_fs_path_destroy(&parent);
        return status;
    }
    size_t offset = 0;
    while (offset < length) {
        ssize_t amount = write(descriptor, data + offset, length - offset);
        if (amount < 0 && errno == EINTR) continue;
        if (amount <= 0) {
            cleanup_temporary(temporary, descriptor);
            free(temporary);
            retro_fs_path_destroy(&parent);
            return RETRO_FS_IO;
        }
        offset += (size_t)amount;
    }
    if (!flush_descriptor(descriptor)) {
        cleanup_temporary(temporary, descriptor);
        free(temporary);
        retro_fs_path_destroy(&parent);
        return map_errno(errno);
    }
    if (close(descriptor) < 0) {
        descriptor = -1;
        (void)unlink(temporary);
        free(temporary);
        retro_fs_path_destroy(&parent);
        return map_errno(errno);
    }
    descriptor = -1;

    bool still_exists = false;
    struct stat current = {0};
    RetroFsVersion current_token = {0};
    status = current_version(path->value, &still_exists, &current, &current_token);
    if (status != RETRO_FS_OK || still_exists != existed ||
        (existed && !same_version(&original_version, &current_token))) {
        (void)unlink(temporary);
        free(temporary);
        retro_fs_path_destroy(&parent);
        return status == RETRO_FS_OK ? RETRO_FS_CONFLICT : status;
    }

    if (!existed) {
        struct stat destination;
        if (stat(path->value, &destination) == 0) {
            status = RETRO_FS_CONFLICT;
        } else if (errno != ENOENT) {
            status = map_errno(errno);
        } else if (rename(temporary, path->value) < 0) {
            int error = errno;
            status = stat(path->value, &destination) == 0
                         ? RETRO_FS_CONFLICT
                         : map_errno(error);
        }
    } else {
        char *backup = NULL;
        status = create_unique_path(&parent, "RB", &backup, NULL);
        if (status == RETRO_FS_OK && rename(path->value, backup) < 0) {
            status = map_errno(errno);
        }
        if (status == RETRO_FS_OK && rename(temporary, path->value) < 0) {
            int error = errno;
            (void)rename(backup, path->value);
            status = map_errno(error);
        }
        if (status == RETRO_FS_OK) (void)unlink(backup);
        free(backup);
    }
    if (status != RETRO_FS_OK) (void)unlink(temporary);
    free(temporary);
    retro_fs_path_destroy(&parent);
    if (status != RETRO_FS_OK) return status;
    if (written) {
        RetroFsError stat_status = retro_fs_stat(path, written);
        if (stat_status != RETRO_FS_OK) *written = (RetroFsVersion){0};
    }
    return RETRO_FS_OK;
}

RetroFsError retro_fs_create_file(const RetroFsPath *path) {
    if (!path || !path->value || !path->value[0]) return RETRO_FS_INVALID_ARGUMENT;
    int descriptor = open(path->value,
                          O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0666);
    if (descriptor < 0) return map_errno(errno);
    if (!flush_descriptor(descriptor)) {
        int error = errno;
        (void)close(descriptor);
        (void)unlink(path->value);
        return map_errno(error);
    }
    if (close(descriptor) < 0) {
        int error = errno;
        (void)unlink(path->value);
        return map_errno(error);
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
    struct stat metadata;
    if (stat(source->value, &metadata) < 0) return map_errno(errno);
    if (stat(destination->value, &metadata) == 0) return RETRO_FS_CONFLICT;
    if (errno != ENOENT) return map_errno(errno);
    if (rename(source->value, destination->value) < 0) {
        int error = errno;
        if (stat(destination->value, &metadata) == 0) return RETRO_FS_CONFLICT;
        return map_errno(error);
    }
    return RETRO_FS_OK;
}

const char *retro_fs_error_string(RetroFsError error) {
    static const char *const messages[] = {
        "ok", "not found", "permission denied", "unsupported", "too large",
        "invalid text", "conflict", "out of memory", "I/O error",
        "invalid argument"
    };
    return error <= RETRO_FS_INVALID_ARGUMENT ? messages[error] : "unknown";
}

#endif
