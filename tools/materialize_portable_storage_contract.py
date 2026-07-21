#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RETRO_FS_H = r'''#ifndef RETRODESK_STORAGE_RETRO_FS_H
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
'''


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one marker, found {count}")
    return text.replace(old, new, 1)


def update_posix(text: str) -> str:
    old_version = '''static void fill_version(const struct stat *s, RetroFsVersion *v) {
    v->device = s->st_dev;
    v->inode = s->st_ino;
    v->size = s->st_size;
    v->mtime = s->st_mtime;
    v->mode = s->st_mode;
}

static bool same_version(const RetroFsVersion *a, const RetroFsVersion *b) {
    return a && b && a->device == b->device && a->inode == b->inode &&
           a->size == b->size && a->mtime == b->mtime;
}
'''
    new_version = '''static RetroFsKind kind_from_mode(mode_t mode) {
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
'''
    text = replace_once(text, old_version, new_version,
                        "portable POSIX version mapping")

    old_compare = '''static int entry_compare(const void *lhs, const void *rhs) {
    const RetroFsEntry *a = lhs;
    const RetroFsEntry *b = rhs;
    bool ad = S_ISDIR(a->mode);
    bool bd = S_ISDIR(b->mode);
    bool ar = S_ISREG(a->mode);
    bool br = S_ISREG(b->mode);
    int ak = ad ? 0 : (ar ? 1 : 2);
    int bk = bd ? 0 : (br ? 1 : 2);
    if (ak != bk) return ak - bk;
    return strcmp(a->name, b->name);
}
'''
    new_compare = '''static int entry_compare(const void *lhs, const void *rhs) {
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
'''
    text = replace_once(text, old_compare, new_compare,
                        "portable entry ordering")

    text = replace_once(
        text,
        '''        entry.mode = s.st_mode;
        entry.size = s.st_size;
        fill_version(&s, &entry.version);
''',
        '''        fill_version(&s, &entry.version);
        entry.kind = entry.version.kind;
        entry.size = entry.version.size;
''',
        "portable entry metadata",
    )

    old_expected = '''    if (exp &&
        (!exists ||
         !same_version(exp, &(RetroFsVersion){old.st_dev, old.st_ino, old.st_size,
                                               old.st_mtime, old.st_mode}))) {
        return RETRO_FS_CONFLICT;
    }
'''
    new_expected = '''    if (exp) {
        RetroFsVersion current = {0};
        if (exists) fill_version(&old, &current);
        if (!exists || !same_version(exp, &current)) {
            return RETRO_FS_CONFLICT;
        }
    }
'''
    return replace_once(text, old_expected, new_expected,
                        "portable expected-version comparison")


def update_filemanager(text: str) -> str:
    text = replace_once(text, "#include <sys/stat.h>\n", "",
                        "remove POSIX stat include")
    text = replace_once(
        text,
        '''typedef struct FileManagerItem {
    char *name;
    mode_t mode;
    off_t size;
} FileManagerItem;
''',
        '''typedef struct FileManagerItem {
    char *name;
    RetroFsKind kind;
    uint64_t size;
} FileManagerItem;
''',
        "portable file manager item",
    )
    text = replace_once(
        text,
        "    state->items[state->count].mode = entry->mode;\n",
        "    state->items[state->count].kind = entry->kind;\n",
        "portable collected kind",
    )
    text = replace_once(text, "    if (S_ISDIR(version.mode)) {\n",
                        "    if (version.kind == RETRO_FS_KIND_DIRECTORY) {\n",
                        "portable directory open")
    text = replace_once(text, "    if (S_ISREG(version.mode)) {\n",
                        "    if (version.kind == RETRO_FS_KIND_REGULAR) {\n",
                        "portable regular open")
    text = replace_once(
        text,
        "static void fm_format_size(off_t size, char *out, size_t out_size) {\n",
        "static void fm_format_size(uint64_t size, char *out, size_t out_size) {\n",
        "portable size formatter",
    )
    text = replace_once(text,
                        "    double value = size < 0 ? 0.0 : (double)size;\n",
                        "    double value = (double)size;\n",
                        "unsigned size conversion")
    old_render = '''        if (S_ISDIR(item->mode)) {
            snprintf(line, sizeof(line), "[D] %.60s/", item->name);
        } else {
            char size[16];
            fm_format_size(item->size, size, sizeof(size));
            snprintf(line, sizeof(line), "[F] %-52.52s %8s", item->name, size);
        }
'''
    new_render = '''        if (item->kind == RETRO_FS_KIND_DIRECTORY) {
            snprintf(line, sizeof(line), "[D] %.60s/", item->name);
        } else {
            char size[16];
            const char *marker = item->kind == RETRO_FS_KIND_REGULAR
                                     ? "F"
                                     : (item->kind == RETRO_FS_KIND_SYMLINK
                                            ? "L"
                                            : "?");
            fm_format_size(item->size, size, sizeof(size));
            snprintf(line, sizeof(line), "[%s] %-52.52s %8s",
                     marker, item->name, size);
        }
'''
    return replace_once(text, old_render, new_render,
                        "portable file manager rendering")


def update_notepad(text: str) -> str:
    return replace_once(
        text,
        "        (state->version.inode != 0) ? &state->version : NULL, &written);\n",
        "        state->version.valid ? &state->version : NULL, &written);\n",
        "portable version validity",
    )


def update_storage_test(text: str) -> str:
    text = replace_once(text, "    TEST_REQUIRE(S_ISREG(version.mode));\n",
                        "    TEST_REQUIRE(version.valid);\n    TEST_REQUIRE(version.kind == RETRO_FS_KIND_REGULAR);\n",
                        "regular kind assertion")
    return replace_once(text, "    TEST_REQUIRE(S_ISDIR(version.mode));\n",
                        "    TEST_REQUIRE(version.valid);\n    TEST_REQUIRE(version.kind == RETRO_FS_KIND_DIRECTORY);\n",
                        "directory kind assertion")


def update_docs(text: str) -> str:
    text = replace_once(
        text,
        "- includes entry name, mode, and size metadata;\n",
        "- includes entry name, portable kind, and `uint64_t` size metadata;\n",
        "portable directory metadata docs",
    )
    marker = "## Portability Status\n\n"
    addition = '''## Public Metadata Contract

`retro_fs.h` exposes only fixed-width and domain-owned types. `RetroFsKind`
classifies regular files, directories, symbolic links, and other targets without
exporting `mode_t` or `S_IS*` macros. `RetroFsVersion` is an adapter-neutral
optimistic-concurrency token containing fixed-width identity, size, kind, and
nanosecond modification fields. Applications may retain and return the token but
must not assign platform meaning to its identity fields.

'''
    return replace_once(text, marker, addition + marker,
                        "portable metadata documentation")


def main() -> int:
    updates = {
        "src/storage/retro_fs.h": lambda _: RETRO_FS_H,
        "src/storage/retro_fs_posix.c": update_posix,
        "src/apps/filemanager_app.c": update_filemanager,
        "src/apps/notepad_app.c": update_notepad,
        "tests/storage_test.c": update_storage_test,
        "docs/STORAGE.md": update_docs,
    }

    rendered: dict[Path, str] = {}
    try:
        for relative, transform in updates.items():
            path = ROOT / relative
            original = path.read_text(encoding="utf-8")
            changed = transform(original)
            if changed == original:
                raise RuntimeError(f"{relative}: transformation made no change")
            rendered[path] = changed
    except (OSError, RuntimeError) as exc:
        print(f"ERROR: {exc}")
        return 1

    for path, content in rendered.items():
        path.write_text(content, encoding="utf-8")
        print(path.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
