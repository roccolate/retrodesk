#include "storage/retro_fs.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "core/utf8.h"

#define RETRO_FS_MAX_TEXT (1024u * 1024u)
#define RETRO_FS_TEMP_ATTEMPTS 128u

static RetroFsError map_win32_error(DWORD error) {
    switch (error) {
        case ERROR_SUCCESS: return RETRO_FS_OK;
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_DRIVE:
        case ERROR_BAD_NETPATH:
        case ERROR_BAD_NET_NAME:
            return RETRO_FS_NOT_FOUND;
        case ERROR_ACCESS_DENIED:
        case ERROR_PRIVILEGE_NOT_HELD:
        case ERROR_SHARING_VIOLATION:
        case ERROR_LOCK_VIOLATION:
        case ERROR_WRITE_PROTECT:
            return RETRO_FS_PERMISSION;
        case ERROR_FILE_EXISTS:
        case ERROR_ALREADY_EXISTS:
        case ERROR_DIR_NOT_EMPTY:
            return RETRO_FS_CONFLICT;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            return RETRO_FS_OOM;
        case ERROR_FILENAME_EXCED_RANGE:
        case ERROR_BUFFER_OVERFLOW:
            return RETRO_FS_TOO_LARGE;
        case ERROR_NOT_SUPPORTED:
        case ERROR_INVALID_FUNCTION:
        case ERROR_CANT_ACCESS_FILE:
            return RETRO_FS_UNSUPPORTED;
        default:
            return RETRO_FS_IO;
    }
}

static RetroFsError duplicate_utf8(const char *value, char **out) {
    if (!value || !out) return RETRO_FS_INVALID_ARGUMENT;
    size_t length = strlen(value);
    if (length == SIZE_MAX) return RETRO_FS_TOO_LARGE;
    char *copy = malloc(length + 1);
    if (!copy) return RETRO_FS_OOM;
    memcpy(copy, value, length + 1);
    *out = copy;
    return RETRO_FS_OK;
}

static RetroFsError utf8_to_wide(const char *value, wchar_t **out) {
    if (!value || !out || !value[0]) return RETRO_FS_INVALID_ARGUMENT;
    *out = NULL;
    size_t byte_length = strlen(value);
    if (byte_length > (size_t)INT_MAX) return RETRO_FS_TOO_LARGE;
    int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value,
                                       (int)byte_length, NULL, 0);
    if (required <= 0) return RETRO_FS_INVALID_ARGUMENT;
    if ((size_t)required > (SIZE_MAX / sizeof(wchar_t)) - 1) {
        return RETRO_FS_TOO_LARGE;
    }
    wchar_t *wide = malloc(((size_t)required + 1) * sizeof(*wide));
    if (!wide) return RETRO_FS_OOM;
    int converted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value,
                                        (int)byte_length, wide, required);
    if (converted != required) {
        free(wide);
        return RETRO_FS_INVALID_ARGUMENT;
    }
    wide[required] = L'\0';
    for (int i = 0; i < required; ++i) {
        if (wide[i] == L'/') wide[i] = L'\\';
    }
    *out = wide;
    return RETRO_FS_OK;
}

static bool has_extended_prefix(const wchar_t *path) {
    return path && wcslen(path) >= 4 && path[0] == L'\\' &&
           path[1] == L'\\' && (path[2] == L'?' || path[2] == L'.') &&
           path[3] == L'\\';
}

static bool is_drive_absolute(const wchar_t *path) {
    if (!path || wcslen(path) < 3) return false;
    wchar_t letter = path[0];
    bool alpha = (letter >= L'A' && letter <= L'Z') ||
                 (letter >= L'a' && letter <= L'z');
    return alpha && path[1] == L':' && path[2] == L'\\';
}

static RetroFsError path_to_native(const char *value, wchar_t **out) {
    wchar_t *wide = NULL;
    RetroFsError status = utf8_to_wide(value, &wide);
    if (status != RETRO_FS_OK) return status;

    if (has_extended_prefix(wide)) {
        *out = wide;
        return RETRO_FS_OK;
    }

    size_t length = wcslen(wide);
    const wchar_t *prefix = NULL;
    const wchar_t *body = wide;
    size_t prefix_length = 0;
    if (is_drive_absolute(wide)) {
        prefix = L"\\\\?\\";
        prefix_length = 4;
    } else if (length >= 2 && wide[0] == L'\\' && wide[1] == L'\\') {
        prefix = L"\\\\?\\UNC\\";
        prefix_length = 8;
        body = wide + 2;
        length -= 2;
    }

    if (!prefix) {
        *out = wide;
        return RETRO_FS_OK;
    }
    if (length > SIZE_MAX - prefix_length - 1 ||
        length + prefix_length > (SIZE_MAX / sizeof(wchar_t)) - 1) {
        free(wide);
        return RETRO_FS_TOO_LARGE;
    }
    wchar_t *extended = malloc((prefix_length + length + 1) * sizeof(*extended));
    if (!extended) {
        free(wide);
        return RETRO_FS_OOM;
    }
    memcpy(extended, prefix, prefix_length * sizeof(*extended));
    memcpy(extended + prefix_length, body, (length + 1) * sizeof(*extended));
    free(wide);
    *out = extended;
    return RETRO_FS_OK;
}

static RetroFsError wide_to_utf8(const wchar_t *value, char **out) {
    if (!value || !out) return RETRO_FS_INVALID_ARGUMENT;
    *out = NULL;
    size_t wide_length = wcslen(value);
    if (wide_length > (size_t)INT_MAX) return RETRO_FS_TOO_LARGE;
    int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value,
                                       (int)wide_length, NULL, 0, NULL, NULL);
    if (required <= 0) return RETRO_FS_UNSUPPORTED;
    char *utf8 = malloc((size_t)required + 1);
    if (!utf8) return RETRO_FS_OOM;
    int converted = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value,
                                        (int)wide_length, utf8, required,
                                        NULL, NULL);
    if (converted != required) {
        free(utf8);
        return RETRO_FS_UNSUPPORTED;
    }
    utf8[required] = '\0';
    *out = utf8;
    return RETRO_FS_OK;
}

static RetroFsKind kind_from_attributes(DWORD attributes) {
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) return RETRO_FS_KIND_SYMLINK;
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) return RETRO_FS_KIND_DIRECTORY;
    if (attributes & FILE_ATTRIBUTE_DEVICE) return RETRO_FS_KIND_OTHER;
    return RETRO_FS_KIND_REGULAR;
}

static int64_t filetime_to_unix_ns(FILETIME time) {
    const uint64_t epoch_ticks = UINT64_C(116444736000000000);
    ULARGE_INTEGER value;
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    uint64_t ticks = value.QuadPart;
    if (ticks >= epoch_ticks) {
        uint64_t delta = ticks - epoch_ticks;
        if (delta > (uint64_t)INT64_MAX / 100u) return INT64_MAX;
        return (int64_t)(delta * 100u);
    }
    uint64_t delta = epoch_ticks - ticks;
    if (delta > ((uint64_t)INT64_MAX + 1u) / 100u) return INT64_MIN;
    if (delta * 100u == (uint64_t)INT64_MAX + 1u) return INT64_MIN;
    return -(int64_t)(delta * 100u);
}

static void fill_version(const BY_HANDLE_FILE_INFORMATION *info,
                         RetroFsVersion *version) {
    if (!info || !version) return;
    ULARGE_INTEGER size;
    size.LowPart = info->nFileSizeLow;
    size.HighPart = info->nFileSizeHigh;
    ULARGE_INTEGER file_id;
    file_id.LowPart = info->nFileIndexLow;
    file_id.HighPart = info->nFileIndexHigh;
    *version = (RetroFsVersion){0};
    version->valid = true;
    version->kind = kind_from_attributes(info->dwFileAttributes);
    version->volume_id = (uint64_t)info->dwVolumeSerialNumber;
    version->file_id = file_id.QuadPart;
    version->size = size.QuadPart;
    version->modified_ns = filetime_to_unix_ns(info->ftLastWriteTime);
}

static bool same_version(const RetroFsVersion *a, const RetroFsVersion *b) {
    return a && b && a->valid && b->valid && a->kind == b->kind &&
           a->volume_id == b->volume_id && a->file_id == b->file_id &&
           a->size == b->size && a->modified_ns == b->modified_ns;
}

static RetroFsError stat_native(const wchar_t *path, RetroFsVersion *version) {
    HANDLE handle = CreateFileW(path, FILE_READ_ATTRIBUTES,
                                FILE_SHARE_READ | FILE_SHARE_WRITE |
                                    FILE_SHARE_DELETE,
                                NULL, OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS |
                                    FILE_FLAG_OPEN_REPARSE_POINT,
                                NULL);
    if (handle == INVALID_HANDLE_VALUE) return map_win32_error(GetLastError());
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(handle, &info)) {
        DWORD error = GetLastError();
        CloseHandle(handle);
        return map_win32_error(error);
    }
    if (!CloseHandle(handle)) return map_win32_error(GetLastError());
    fill_version(&info, version);
    return RETRO_FS_OK;
}

static RetroFsError current_version_native(const wchar_t *path, bool *exists,
                                           RetroFsVersion *version) {
    if (!path || !exists) return RETRO_FS_INVALID_ARGUMENT;
    RetroFsVersion observed = {0};
    RetroFsError status = stat_native(path, &observed);
    if (status == RETRO_FS_NOT_FOUND) {
        *exists = false;
        if (version) *version = (RetroFsVersion){0};
        return RETRO_FS_OK;
    }
    if (status != RETRO_FS_OK) return status;
    *exists = true;
    if (observed.kind != RETRO_FS_KIND_REGULAR) return RETRO_FS_UNSUPPORTED;
    if (version) *version = observed;
    return RETRO_FS_OK;
}

RetroFsError retro_fs_path_init(RetroFsPath *path, const char *value) {
    if (!path || !value || !value[0]) return RETRO_FS_INVALID_ARGUMENT;
    path->value = NULL;
    wchar_t *native = NULL;
    RetroFsError status = path_to_native(value, &native);
    free(native);
    if (status != RETRO_FS_OK) return status;
    return duplicate_utf8(value, &path->value);
}

void retro_fs_path_destroy(RetroFsPath *path) {
    if (!path) return;
    free(path->value);
    path->value = NULL;
}

const char *retro_fs_path_cstr(const RetroFsPath *path) {
    return path ? path->value : NULL;
}

static bool is_separator(char ch) {
    return ch == '/' || ch == '\\';
}

static size_t root_length(const char *path) {
    size_t length = strlen(path);
    if (length >= 2 &&
        ((path[0] >= 'A' && path[0] <= 'Z') ||
         (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':') {
        return length >= 3 && is_separator(path[2]) ? 3 : 2;
    }
    if (length >= 2 && is_separator(path[0]) && is_separator(path[1])) {
        size_t cursor = 2;
        while (cursor < length && !is_separator(path[cursor])) ++cursor;
        while (cursor < length && is_separator(path[cursor])) ++cursor;
        while (cursor < length && !is_separator(path[cursor])) ++cursor;
        if (cursor < length && is_separator(path[cursor])) ++cursor;
        return cursor;
    }
    return length > 0 && is_separator(path[0]) ? 1 : 0;
}

RetroFsError retro_fs_path_parent(const RetroFsPath *path, RetroFsPath *parent) {
    if (!path || !path->value || !parent) return RETRO_FS_INVALID_ARGUMENT;
    parent->value = NULL;
    size_t length = strlen(path->value);
    size_t root = root_length(path->value);
    while (length > root && is_separator(path->value[length - 1])) --length;
    size_t cursor = length;
    while (cursor > root && !is_separator(path->value[cursor - 1])) --cursor;
    if (cursor == 0 || (cursor <= root && root == 0)) {
        return retro_fs_path_init(parent, ".");
    }
    size_t parent_length = cursor <= root ? root : cursor - 1;
    while (parent_length > root && is_separator(path->value[parent_length - 1])) {
        --parent_length;
    }
    if (parent_length == 0) return retro_fs_path_init(parent, ".");
    char *copy = malloc(parent_length + 1);
    if (!copy) return RETRO_FS_OOM;
    memcpy(copy, path->value, parent_length);
    copy[parent_length] = '\0';
    parent->value = copy;
    return RETRO_FS_OK;
}

RetroFsError retro_fs_path_join(const RetroFsPath *base, const char *name,
                                 RetroFsPath *joined) {
    if (!base || !base->value || !name || !name[0] || !joined ||
        strchr(name, '/') || strchr(name, '\\') || strchr(name, ':')) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    joined->value = NULL;
    size_t base_length = strlen(base->value);
    size_t name_length = strlen(name);
    bool needs_separator = base_length > 0 &&
                           !is_separator(base->value[base_length - 1]);
    size_t extra = needs_separator ? 1u : 0u;
    if (base_length > SIZE_MAX - name_length - extra - 1) {
        return RETRO_FS_TOO_LARGE;
    }
    char *value = malloc(base_length + name_length + extra + 1);
    if (!value) return RETRO_FS_OOM;
    memcpy(value, base->value, base_length);
    size_t offset = base_length;
    if (needs_separator) value[offset++] = '\\';
    memcpy(value + offset, name, name_length + 1);
    wchar_t *native = NULL;
    RetroFsError status = path_to_native(value, &native);
    free(native);
    if (status != RETRO_FS_OK) {
        free(value);
        return status;
    }
    joined->value = value;
    return RETRO_FS_OK;
}

RetroFsError retro_fs_stat(const RetroFsPath *path, RetroFsVersion *version) {
    if (!path || !path->value || !version) return RETRO_FS_INVALID_ARGUMENT;
    wchar_t *native = NULL;
    RetroFsError status = path_to_native(path->value, &native);
    if (status == RETRO_FS_OK) status = stat_native(native, version);
    free(native);
    return status;
}

static int entry_compare(const void *lhs, const void *rhs) {
    const RetroFsEntry *a = lhs;
    const RetroFsEntry *b = rhs;
    int a_rank = a->kind == RETRO_FS_KIND_DIRECTORY
                     ? 0
                     : (a->kind == RETRO_FS_KIND_REGULAR ? 1 : 2);
    int b_rank = b->kind == RETRO_FS_KIND_DIRECTORY
                     ? 0
                     : (b->kind == RETRO_FS_KIND_REGULAR ? 1 : 2);
    if (a_rank != b_rank) return a_rank - b_rank;
    return strcmp(a->name, b->name);
}

static void free_entries(RetroFsEntry *entries, size_t count) {
    if (!entries) return;
    for (size_t i = 0; i < count; ++i) free(entries[i].name);
    free(entries);
}

static bool reserve_entries(RetroFsEntry **entries, size_t *capacity,
                            size_t wanted) {
    if (!entries || !capacity) return false;
    if (wanted <= *capacity) return true;
    if (wanted > SIZE_MAX / sizeof(**entries)) return false;
    size_t next = *capacity ? *capacity : 64u;
    while (next < wanted) {
        if (next > SIZE_MAX / 2u) {
            next = wanted;
            break;
        }
        next *= 2u;
    }
    RetroFsEntry *grown = realloc(*entries, next * sizeof(*grown));
    if (!grown) return false;
    *entries = grown;
    *capacity = next;
    return true;
}

static RetroFsError append_entry(RetroFsEntry **entries, size_t *count,
                                 size_t *capacity, size_t max_entries,
                                 const char *name, const RetroFsPath *path) {
    if (*count == max_entries) return RETRO_FS_TOO_LARGE;
    RetroFsEntry entry = {0};
    RetroFsError status = duplicate_utf8(name, &entry.name);
    if (status != RETRO_FS_OK) return status;
    status = retro_fs_stat(path, &entry.version);
    if (status != RETRO_FS_OK) {
        free(entry.name);
        return status;
    }
    entry.kind = entry.version.kind;
    entry.size = entry.version.size;
    if (!reserve_entries(entries, capacity, *count + 1)) {
        free(entry.name);
        return RETRO_FS_OOM;
    }
    (*entries)[(*count)++] = entry;
    return RETRO_FS_OK;
}

RetroFsError retro_fs_list(const RetroFsPath *path, size_t max_entries,
                            RetroFsListFn callback, void *userdata) {
    if (!path || !path->value || !callback || max_entries == 0) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    RetroFsVersion directory = {0};
    RetroFsError status = retro_fs_stat(path, &directory);
    if (status != RETRO_FS_OK) return status;
    if (directory.kind != RETRO_FS_KIND_DIRECTORY) return RETRO_FS_UNSUPPORTED;

    RetroFsEntry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    RetroFsPath parent = {0};
    status = retro_fs_path_parent(path, &parent);
    if (status == RETRO_FS_OK) {
        status = append_entry(&entries, &count, &capacity, max_entries,
                              "..", &parent);
    }
    retro_fs_path_destroy(&parent);
    if (status != RETRO_FS_OK) {
        free_entries(entries, count);
        return status;
    }

    wchar_t *native = NULL;
    status = path_to_native(path->value, &native);
    if (status != RETRO_FS_OK) {
        free_entries(entries, count);
        return status;
    }
    size_t native_length = wcslen(native);
    bool needs_separator = native_length > 0 && native[native_length - 1] != L'\\';
    size_t pattern_length = native_length + (needs_separator ? 1u : 0u) + 1u;
    if (pattern_length > (SIZE_MAX / sizeof(wchar_t)) - 1) {
        free(native);
        free_entries(entries, count);
        return RETRO_FS_TOO_LARGE;
    }
    wchar_t *pattern = malloc((pattern_length + 1) * sizeof(*pattern));
    if (!pattern) {
        free(native);
        free_entries(entries, count);
        return RETRO_FS_OOM;
    }
    memcpy(pattern, native, native_length * sizeof(*pattern));
    size_t offset = native_length;
    if (needs_separator) pattern[offset++] = L'\\';
    pattern[offset++] = L'*';
    pattern[offset] = L'\0';
    free(native);

    WIN32_FIND_DATAW find_data;
    HANDLE find = FindFirstFileW(pattern, &find_data);
    free(pattern);
    if (find == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) {
            free_entries(entries, count);
            return map_win32_error(error);
        }
    } else {
        for (;;) {
            if (wcscmp(find_data.cFileName, L".") != 0 &&
                wcscmp(find_data.cFileName, L"..") != 0) {
                char *name = NULL;
                status = wide_to_utf8(find_data.cFileName, &name);
                if (status == RETRO_FS_OK) {
                    RetroFsPath child = {0};
                    status = retro_fs_path_join(path, name, &child);
                    if (status == RETRO_FS_OK) {
                        status = append_entry(&entries, &count, &capacity,
                                              max_entries, name, &child);
                    }
                    retro_fs_path_destroy(&child);
                }
                free(name);
                if (status != RETRO_FS_OK) break;
            }
            if (!FindNextFileW(find, &find_data)) {
                DWORD error = GetLastError();
                if (error != ERROR_NO_MORE_FILES) status = map_win32_error(error);
                break;
            }
        }
        if (!FindClose(find) && status == RETRO_FS_OK) {
            status = map_win32_error(GetLastError());
        }
    }
    if (status != RETRO_FS_OK) {
        free_entries(entries, count);
        return status;
    }
    if (count > 1) qsort(entries, count, sizeof(*entries), entry_compare);
    for (size_t i = 0; i < count; ++i) {
        if (!callback(&entries[i], userdata)) break;
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
        if (!retro_utf8_decode(data, length, offset, &codepoint, &byte_length)) {
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
    wchar_t *native = NULL;
    RetroFsError status = path_to_native(path->value, &native);
    if (status != RETRO_FS_OK) return status;
    HANDLE handle = CreateFileW(native, GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE |
                                    FILE_SHARE_DELETE,
                                NULL, OPEN_EXISTING,
                                FILE_FLAG_SEQUENTIAL_SCAN |
                                    FILE_FLAG_OPEN_REPARSE_POINT |
                                    FILE_FLAG_BACKUP_SEMANTICS,
                                NULL);
    free(native);
    if (handle == INVALID_HANDLE_VALUE) return map_win32_error(GetLastError());

    BY_HANDLE_FILE_INFORMATION before_info;
    if (!GetFileInformationByHandle(handle, &before_info)) {
        DWORD error = GetLastError();
        CloseHandle(handle);
        return map_win32_error(error);
    }
    RetroFsVersion before = {0};
    fill_version(&before_info, &before);
    if (before.kind != RETRO_FS_KIND_REGULAR) {
        CloseHandle(handle);
        return RETRO_FS_UNSUPPORTED;
    }
    if (before.size > RETRO_FS_MAX_TEXT) {
        CloseHandle(handle);
        return RETRO_FS_TOO_LARGE;
    }
    size_t expected = (size_t)before.size;
    char *buffer = malloc(expected + 1);
    if (!buffer) {
        CloseHandle(handle);
        return RETRO_FS_OOM;
    }
    size_t total = 0;
    while (total < expected) {
        DWORD chunk = 0;
        DWORD wanted = (DWORD)(expected - total);
        if (!ReadFile(handle, buffer + total, wanted, &chunk, NULL)) {
            DWORD error = GetLastError();
            free(buffer);
            CloseHandle(handle);
            return map_win32_error(error);
        }
        if (chunk == 0) {
            free(buffer);
            CloseHandle(handle);
            return RETRO_FS_IO;
        }
        total += (size_t)chunk;
    }
    BY_HANDLE_FILE_INFORMATION after_info;
    if (!GetFileInformationByHandle(handle, &after_info)) {
        DWORD error = GetLastError();
        free(buffer);
        CloseHandle(handle);
        return map_win32_error(error);
    }
    if (!CloseHandle(handle)) {
        DWORD error = GetLastError();
        free(buffer);
        return map_win32_error(error);
    }
    RetroFsVersion after = {0};
    fill_version(&after_info, &after);
    if (!same_version(&before, &after)) {
        free(buffer);
        return RETRO_FS_CONFLICT;
    }
    buffer[total] = '\0';
    if (!valid_text_content(buffer, total)) {
        free(buffer);
        return RETRO_FS_INVALID_TEXT;
    }
    *data = buffer;
    *length = total;
    if (version) *version = after;
    return RETRO_FS_OK;
}

static RetroFsError build_temp_path(const wchar_t *parent, unsigned int attempt,
                                    wchar_t **out) {
    if (!parent || !out) return RETRO_FS_INVALID_ARGUMENT;
    size_t parent_length = wcslen(parent);
    bool separator = parent_length > 0 && parent[parent_length - 1] != L'\\';
    if (parent_length > (SIZE_MAX / sizeof(wchar_t)) - 96u) {
        return RETRO_FS_TOO_LARGE;
    }
    size_t capacity = parent_length + 96u;
    wchar_t *temporary = malloc(capacity * sizeof(*temporary));
    if (!temporary) return RETRO_FS_OOM;
    int count = swprintf(temporary, capacity,
                         L"%ls%ls.retro-tmp-%08lX-%016llX-%u.tmp",
                         parent, separator ? L"\\" : L"",
                         (unsigned long)GetCurrentProcessId(),
                         (unsigned long long)GetTickCount64(), attempt);
    if (count < 0 || (size_t)count >= capacity) {
        free(temporary);
        return RETRO_FS_TOO_LARGE;
    }
    *out = temporary;
    return RETRO_FS_OK;
}

static RetroFsError create_temporary(const wchar_t *parent,
                                     wchar_t **temporary, HANDLE *handle) {
    if (!parent || !temporary || !handle) return RETRO_FS_INVALID_ARGUMENT;
    *temporary = NULL;
    *handle = INVALID_HANDLE_VALUE;
    for (unsigned int attempt = 0; attempt < RETRO_FS_TEMP_ATTEMPTS; ++attempt) {
        wchar_t *candidate = NULL;
        RetroFsError status = build_temp_path(parent, attempt, &candidate);
        if (status != RETRO_FS_OK) return status;
        HANDLE created = CreateFileW(candidate, GENERIC_READ | GENERIC_WRITE, 0,
                                     NULL, CREATE_NEW,
                                     FILE_ATTRIBUTE_TEMPORARY |
                                         FILE_FLAG_WRITE_THROUGH,
                                     NULL);
        if (created != INVALID_HANDLE_VALUE) {
            *temporary = candidate;
            *handle = created;
            return RETRO_FS_OK;
        }
        DWORD error = GetLastError();
        free(candidate);
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
            return map_win32_error(error);
        }
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
    if (written) *written = (RetroFsVersion){0};
    if (length > RETRO_FS_MAX_TEXT) return RETRO_FS_TOO_LARGE;
    if (!valid_text_content(data, length)) return RETRO_FS_INVALID_TEXT;

    wchar_t *native = NULL;
    RetroFsError status = path_to_native(path->value, &native);
    if (status != RETRO_FS_OK) return status;
    bool existed = false;
    RetroFsVersion original = {0};
    status = current_version_native(native, &existed, &original);
    if (status != RETRO_FS_OK) {
        free(native);
        return status;
    }
    if (existed && !expected) {
        free(native);
        return RETRO_FS_CONFLICT;
    }
    if (expected && (!existed || !same_version(expected, &original))) {
        free(native);
        return RETRO_FS_CONFLICT;
    }

    RetroFsPath parent_path = {0};
    status = retro_fs_path_parent(path, &parent_path);
    if (status != RETRO_FS_OK) {
        free(native);
        return status;
    }
    wchar_t *parent = NULL;
    status = path_to_native(parent_path.value, &parent);
    retro_fs_path_destroy(&parent_path);
    if (status != RETRO_FS_OK) {
        free(native);
        return status;
    }

    wchar_t *temporary = NULL;
    HANDLE handle = INVALID_HANDLE_VALUE;
    status = create_temporary(parent, &temporary, &handle);
    free(parent);
    if (status != RETRO_FS_OK) {
        free(native);
        return status;
    }
    size_t offset = 0;
    while (offset < length) {
        DWORD chunk = 0;
        size_t remaining = length - offset;
        DWORD wanted = remaining > (size_t)MAXDWORD ? MAXDWORD : (DWORD)remaining;
        if (!WriteFile(handle, data + offset, wanted, &chunk, NULL) || chunk == 0) {
            DWORD error = GetLastError();
            CloseHandle(handle);
            DeleteFileW(temporary);
            free(temporary);
            free(native);
            return error == ERROR_SUCCESS ? RETRO_FS_IO : map_win32_error(error);
        }
        offset += (size_t)chunk;
    }
    if (!FlushFileBuffers(handle)) {
        DWORD error = GetLastError();
        CloseHandle(handle);
        DeleteFileW(temporary);
        free(temporary);
        free(native);
        return map_win32_error(error);
    }
    if (!CloseHandle(handle)) {
        DWORD error = GetLastError();
        DeleteFileW(temporary);
        free(temporary);
        free(native);
        return map_win32_error(error);
    }

    bool still_exists = false;
    RetroFsVersion current = {0};
    status = current_version_native(native, &still_exists, &current);
    if (status != RETRO_FS_OK || still_exists != existed ||
        (existed && !same_version(&original, &current))) {
        DeleteFileW(temporary);
        free(temporary);
        free(native);
        return status == RETRO_FS_OK ? RETRO_FS_CONFLICT : status;
    }

    BOOL moved;
    if (existed) {
        moved = ReplaceFileW(native, temporary, NULL,
                             REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL);
    } else {
        moved = MoveFileExW(temporary, native, MOVEFILE_WRITE_THROUGH);
    }
    if (!moved) {
        DWORD error = GetLastError();
        DeleteFileW(temporary);
        free(temporary);
        free(native);
        return map_win32_error(error);
    }
    free(temporary);
    free(native);
    if (written) {
        status = retro_fs_stat(path, written);
        if (status != RETRO_FS_OK) *written = (RetroFsVersion){0};
    }
    return RETRO_FS_OK;
}

RetroFsError retro_fs_create_file(const RetroFsPath *path) {
    if (!path || !path->value || !path->value[0]) return RETRO_FS_INVALID_ARGUMENT;
    wchar_t *native = NULL;
    RetroFsError status = path_to_native(path->value, &native);
    if (status != RETRO_FS_OK) return status;
    HANDLE handle = CreateFileW(native, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                                NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        free(native);
        return map_win32_error(error);
    }
    if (!FlushFileBuffers(handle)) {
        DWORD error = GetLastError();
        CloseHandle(handle);
        DeleteFileW(native);
        free(native);
        return map_win32_error(error);
    }
    if (!CloseHandle(handle)) {
        DWORD error = GetLastError();
        DeleteFileW(native);
        free(native);
        return map_win32_error(error);
    }
    free(native);
    return RETRO_FS_OK;
}

RetroFsError retro_fs_create_directory(const RetroFsPath *path) {
    if (!path || !path->value || !path->value[0]) return RETRO_FS_INVALID_ARGUMENT;
    wchar_t *native = NULL;
    RetroFsError status = path_to_native(path->value, &native);
    if (status != RETRO_FS_OK) return status;
    if (!CreateDirectoryW(native, NULL)) status = map_win32_error(GetLastError());
    free(native);
    return status;
}

RetroFsError retro_fs_rename(const RetroFsPath *source,
                             const RetroFsPath *destination) {
    if (!source || !source->value || !source->value[0] || !destination ||
        !destination->value || !destination->value[0]) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    wchar_t *source_native = NULL;
    wchar_t *destination_native = NULL;
    RetroFsError status = path_to_native(source->value, &source_native);
    if (status == RETRO_FS_OK) {
        status = path_to_native(destination->value, &destination_native);
    }
    if (status == RETRO_FS_OK &&
        !MoveFileExW(source_native, destination_native, MOVEFILE_WRITE_THROUGH)) {
        status = map_win32_error(GetLastError());
    }
    free(destination_native);
    free(source_native);
    return status;
}

const char *retro_fs_error_string(RetroFsError error) {
    static const char *const messages[] = {
        "ok", "not found", "permission denied", "unsupported", "too large",
        "invalid text", "conflict", "out of memory", "I/O error",
        "invalid argument"
    };
    return error <= RETRO_FS_INVALID_ARGUMENT ? messages[error] : "unknown";
}
