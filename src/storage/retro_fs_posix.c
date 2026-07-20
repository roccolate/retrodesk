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

#define RETRO_FS_MAX_TEXT (1024u * 1024u)

static RetroFsError map_errno(int e) {
    if (e == ENOENT || e == ENOTDIR) return RETRO_FS_NOT_FOUND;
    if (e == EACCES || e == EPERM) return RETRO_FS_PERMISSION;
    if (e == EEXIST || e == ENOTEMPTY) return RETRO_FS_CONFLICT;
    if (e == ENOMEM) return RETRO_FS_OOM;
    return RETRO_FS_IO;
}

static void fill_version(const struct stat *s, RetroFsVersion *v) {
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
    if (!b || !b->value || !name || !o || strchr(name, '/')) {
        return RETRO_FS_INVALID_ARGUMENT;
    }
    size_t n = strlen(b->value);
    size_t m = strlen(name);
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
    if (stat(p->value, &s) < 0) return map_errno(errno);
    fill_version(&s, v);
    return RETRO_FS_OK;
}

static int entry_compare(const void *lhs, const void *rhs) {
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

static void free_entries(RetroFsEntry *entries, size_t count) {
    if (!entries) return;
    for (size_t i = 0; i < count; ++i) free(entries[i].name);
    free(entries);
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
    RetroFsError result = RETRO_FS_OK;
    struct dirent *de;
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
        entry.mode = s.st_mode;
        entry.size = s.st_size;
        fill_version(&s, &entry.version);
        RetroFsEntry *next = realloc(entries, (count + 1) * sizeof(*next));
        if (!next) {
            free(entry.name);
            result = RETRO_FS_OOM;
            break;
        }
        entries = next;
        entries[count++] = entry;
    }
    closedir(d);
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

RetroFsError retro_fs_read_text(const RetroFsPath *p, char **out, size_t *len,
                                 RetroFsVersion *v) {
    int fd;
    struct stat s;
    size_t got = 0;
    char *buf;
    if (!p || !p->value || !out || !len) return RETRO_FS_INVALID_ARGUMENT;
    fd = open(p->value, O_RDONLY);
    if (fd < 0) return map_errno(errno);
    if (fstat(fd, &s) < 0) {
        int e = errno;
        close(fd);
        return map_errno(e);
    }
    if (!S_ISREG(s.st_mode)) {
        close(fd);
        return RETRO_FS_UNSUPPORTED;
    }
    if ((uintmax_t)s.st_size > RETRO_FS_MAX_TEXT) {
        close(fd);
        return RETRO_FS_TOO_LARGE;
    }
    buf = malloc((size_t)s.st_size + 1);
    if (!buf) {
        close(fd);
        return RETRO_FS_OOM;
    }
    while (got < (size_t)s.st_size) {
        ssize_t n = read(fd, buf + got, (size_t)s.st_size - got);
        if (n <= 0) {
            free(buf);
            close(fd);
            return RETRO_FS_IO;
        }
        got += (size_t)n;
    }
    close(fd);
    buf[got] = 0;
    bool saw_lf = false;
    bool saw_crlf = false;
    for (size_t i = 0; i < got; ++i) {
        unsigned char c = (unsigned char)buf[i];
        if (c == 0 || c == 27 ||
            (c < 32 && c != '\t' && c != '\n' && c != '\r') || c >= 127) {
            free(buf);
            return RETRO_FS_INVALID_TEXT;
        }
        if (c == '\r') {
            if (i + 1 >= got || buf[i + 1] != '\n') {
                free(buf);
                return RETRO_FS_INVALID_TEXT;
            }
            saw_crlf = true;
        } else if (c == '\n') {
            if (i == 0 || buf[i - 1] != '\r') saw_lf = true;
        }
    }
    if (saw_lf && saw_crlf) {
        free(buf);
        return RETRO_FS_INVALID_TEXT;
    }
    *out = buf;
    *len = got;
    if (v) fill_version(&s, v);
    return RETRO_FS_OK;
}

RetroFsError retro_fs_write_atomic(const RetroFsPath *p, const char *data, size_t n,
                                    const RetroFsVersion *exp,
                                    RetroFsVersion *written) {
    struct stat old = {0};
    if (!p || !p->value || (!data && n)) return RETRO_FS_INVALID_ARGUMENT;
    if (n > RETRO_FS_MAX_TEXT) return RETRO_FS_TOO_LARGE;
    bool exists = lstat(p->value, &old) == 0;
    if (!exists && errno != ENOENT) return map_errno(errno);
    if (exists && (!S_ISREG(old.st_mode) || S_ISLNK(old.st_mode))) {
        return RETRO_FS_UNSUPPORTED;
    }
    if (exp &&
        (!exists ||
         !same_version(exp, &(RetroFsVersion){old.st_dev, old.st_ino, old.st_size,
                                               old.st_mtime, old.st_mode}))) {
        return RETRO_FS_CONFLICT;
    }
    RetroFsPath par = {0};
    RetroFsError er = retro_fs_path_parent(p, &par);
    if (er) return er;
    size_t m = strlen(par.value) + 32;
    char *tmp = malloc(m);
    if (!tmp) {
        retro_fs_path_destroy(&par);
        return RETRO_FS_OOM;
    }
    snprintf(tmp, m, "%s/.retro-tmp-XXXXXX", par.value);
    int fd = mkstemp(tmp);
    if (fd < 0) {
        free(tmp);
        retro_fs_path_destroy(&par);
        return map_errno(errno);
    }
    fchmod(fd, exists ? old.st_mode & 07777 : 0666);
    size_t w = 0;
    while (w < n) {
        ssize_t x = write(fd, data + w, n - w);
        if (x <= 0) {
            close(fd);
            unlink(tmp);
            free(tmp);
            retro_fs_path_destroy(&par);
            return RETRO_FS_IO;
        }
        w += (size_t)x;
    }
    if (fsync(fd) < 0) {
        close(fd);
        unlink(tmp);
        free(tmp);
        retro_fs_path_destroy(&par);
        return RETRO_FS_IO;
    }
    close(fd);
    if (rename(tmp, p->value) < 0) {
        int e = errno;
        unlink(tmp);
        free(tmp);
        retro_fs_path_destroy(&par);
        return map_errno(e);
    }
    int dfd = open(par.value, O_RDONLY | O_DIRECTORY);
    if (dfd >= 0) {
        fsync(dfd);
        close(dfd);
    }
    free(tmp);
    retro_fs_path_destroy(&par);
    if (written) retro_fs_stat(p, written);
    return RETRO_FS_OK;
}

RetroFsError retro_fs_create_file(const RetroFsPath *path) {
    if (!path || !path->value || !path->value[0]) return RETRO_FS_INVALID_ARGUMENT;
    int fd = open(path->value, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0) return map_errno(errno);
    if (close(fd) < 0) return map_errno(errno);
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

    struct stat existing;
    if (lstat(destination->value, &existing) == 0) return RETRO_FS_CONFLICT;
    if (errno != ENOENT) return map_errno(errno);

    /* POSIX rename can replace a destination. The lstat guard keeps the public
       contract non-destructive; a future adapter may use a native no-replace
       primitive where one is available. */
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
